/*
 * Copyright (C) 2019 Jolla Ltd.
 *
 * Contact: Juho Hämäläinen <juho.hamalainen@jolla.com>
 *
 * You may use this file under the terms of BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <gbinder.h>
#include <glib-unix.h>
#include <gutil_log.h>

#include "common.h"
#include "impl.h"
#include "logging.h"

/* Android >= 8 use index 18, <= 7 use index 17 */
#define DEFAULT_AF_BINDER_IDX       (18)

gboolean _app_module = FALSE;

#define RET_OK                      (0)
#define RET_INVARG                  (2)

static const char pname[] = PASSTHROUGH_HELPER_EXE;

typedef struct app App;

struct app {
    GMainLoop* loop;
    int ret;
    gint type;
    AppConfig config;
};

static AppImplementation app_implementations[APP_MAX] = {
    { AUDIOSYSTEM_PASSTHROUGH_IMPL_STR_QTI,     app_qti_init,   app_qti_wait,   app_qti_done   },
    { AUDIOSYSTEM_PASSTHROUGH_IMPL_STR_AF,      app_af_init,    app_af_wait,    app_af_done    },
    { AUDIOSYSTEM_PASSTHROUGH_IMPL_STR_HW2_0,   app_hw2_0_init, app_hw2_0_wait, app_hw2_0_done },
};


static gboolean
app_signal(
        gpointer user_data)
{
    App* app = user_data;

    DBG("Caught signal, %s shutting down...", pname);
    g_main_loop_quit(app->loop);
    return G_SOURCE_CONTINUE;
}

static void
app_run(
        App* app)
{
    guint sigtrm = g_unix_signal_add(SIGTERM, app_signal, app);
    guint sigint = g_unix_signal_add(SIGINT, app_signal, app);

    g_main_loop_run(app->loop);

    g_source_remove(sigtrm);
    g_source_remove(sigint);
}

static gboolean
parse_app_type(
        App *app,
        const gchar *type_str)
{
    guint i;

    if (!type_str)
        type_str = g_getenv(ENV_AUDIOSYSTEM_PASSTHROUGH_TYPE);

    if (!type_str)
        type_str = AUDIOSYSTEM_PASSTHROUGH_IMPL_STR_QTI;

    for (i = 0; i < sizeof(app_implementations) / sizeof(app_implementations[0]); i++) {
        if (!g_strcmp0(type_str, app_implementations[i].name)) {
            DBG("Using %s implementation", app_implementations[i].name);
            app->type = i;
            break;
        }
    }

    return app->type >= 0;
}

static gboolean
app_init(
        App* app,
        int argc,
        char* argv[])
{
    guint level = 0;
    gboolean ok = FALSE;
    GError* error = NULL;
    gchar *type_str = NULL;
    gchar *address_str = NULL;
    gint idx = -1;
    GOptionContext* options = NULL;

    GOptionEntry entries[] = {
        { "address", 'a', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING,
          &address_str, "DBus address for PulseAudio module interface.", NULL },
        { "idx", 'i', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT,
          &idx, "Starting index for binder calls, only applicable with type " AUDIOSYSTEM_PASSTHROUGH_IMPL_STR_AF ".", NULL },
        { "type", 't', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING,
          &type_str, "Passthrough type. Can be " AUDIOSYSTEM_PASSTHROUGH_IMPL_STR_AF ", " AUDIOSYSTEM_PASSTHROUGH_IMPL_STR_QTI ", " AUDIOSYSTEM_PASSTHROUGH_IMPL_STR_HW2_0, NULL },
        { "module", 'm', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
          &_app_module, "Run as child process.", NULL },
        { "verbose", 'v', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
          &app->config.verbose, "Enable verbose output", NULL },
        { NULL, 0, 0, 0, NULL, NULL, NULL }
    };

    options = g_option_context_new(NULL);

    gutil_log_timestamp = FALSE;
    gutil_log_set_type(GLOG_TYPE_STDOUT, pname);
    gutil_log_default.level = GLOG_LEVEL_ERR;
    log_init(&level);

    g_option_context_add_main_entries(options, entries, NULL);
    if (!g_option_context_parse(options, &argc, &argv, &error))
        goto fail;

    if (app->config.verbose || level == PULSE_LOG_LEVEL_DEBUG)
        gutil_log_default.level = GLOG_LEVEL_VERBOSE;

    if (address_str) {
        app->config.address = address_str;
        address_str = NULL;
    } else {
        if (g_getenv(ENV_AUDIOSYSTEM_PASSTHROUGH_ADDRESS))
            app->config.address = g_strdup(g_getenv(ENV_AUDIOSYSTEM_PASSTHROUGH_ADDRESS));
    }

    if (!app->config.address) {
        ERR("No DBus address defined.");
        goto fail;
    }

    app->config.binder_index = idx > 0 ? idx : DEFAULT_AF_BINDER_IDX;
    if (g_getenv(ENV_AUDIOSYSTEM_PASSTHROUGH_IDX))
        app->config.binder_index = atoi(g_getenv(ENV_AUDIOSYSTEM_PASSTHROUGH_IDX));

    if (app->config.binder_index <= 0) {
        ERR("Invalid binder index value %d", idx);
        goto fail;
    }

    if (!parse_app_type(app, type_str)) {
        ERR("Unknown type");
        goto fail;
    }
    g_free(type_str);

    if (!g_strcmp0(app->config.address, DUMMY_MODE_STR)) {
        DBG("Dummy mode enabled.");
        app->config.dummy_mode = TRUE;
    }

    app->loop = g_main_loop_new(NULL, TRUE);
    if (!app_implementations[app->type].init(app->loop, &app->config))
        goto fail;

    ok = TRUE;
    app->ret = RET_OK;
    g_option_context_free(options);

    return ok;

fail:
    g_free(address_str);
    g_free(type_str);

    if (options)
        g_option_context_free(options);

    if (error) {
        ERR("Options: %s", error->message);
        g_error_free(error);
    }

    if (!app->config.address)
        ERR("Address is not defined for %s", pname);

    return ok;
}

static void
app_deinit(
        App *app)
{
    if (app->type >= 0 && app->ret == RET_OK)
        app->ret = app_implementations[app->type].done();
    g_free(app->config.address);
}

int main(int argc, char* argv[])
{
    App app;

    memset(&app, 0, sizeof(app));
    app.ret = RET_INVARG;
    app.type = -1;

    if (app_init(&app, argc, argv)) {
        if (app_implementations[app.type].wait())
            app_run(&app);
        g_main_loop_unref(app.loop);
    }

    app_deinit(&app);
    return app.ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
