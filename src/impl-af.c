/*
 * Copyright (C) 2019-2022 Jolla Ltd.
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
#include <unistd.h>

#include "common.h"
#include "impl.h"
#include "logging.h"
#include "dbus-comms.h"

#define BINDER_DEVICE               GBINDER_DEFAULT_BINDER
#define SERVICE_NAME                "media.audio_flinger"
#define SERVICE_IFACE               "android.media.IAudioFlinger"


enum af_methods {
    AF_SET_PARAMETERS = GBINDER_FIRST_CALL_TRANSACTION, /* + starting index */
    AF_GET_PARAMETERS,
    AF_REGISTER_CLIENT,
};

typedef struct af_app AfApp;

struct af_app {
    GMainLoop* loop;
    const AppConfig *config;
    gulong presence_id;
    GBinderServiceManager* sm;
    GBinderLocalObject *local;
    DBusComms *dbus;
};

static AfApp _app;

static guint
binder_idx(
        AfApp *app,
        guint idx)
{
    return app->config->binder_index + idx;
}

static GBinderLocalReply*
app_reply(
        GBinderLocalObject* obj,
        GBinderRemoteRequest* req,
        guint code,
        guint flags,
        int* status,
        void* user_data)
{
    AfApp* app = user_data;
    const char* iface;
    GBinderLocalReply* reply = NULL;

    iface = gbinder_remote_request_interface(req);
    if (g_strcmp0(iface, SERVICE_IFACE)) {
        ERR("Unexpected interface \"%s\"", iface);
        *status = -1;
    } else if (app->config->dummy_mode) {
        DBG("request %d, reply NO_ERROR", code);
        reply = gbinder_local_object_new_reply(obj);
        gbinder_local_reply_append_int32(reply, 0);
        *status = 0;
    } else if (code == binder_idx(app, AF_SET_PARAMETERS)) {
        GBinderReader reader;
        int token;
        int iohandle;
        const char *key_value_pairs;

        gbinder_remote_request_init_reader(req, &reader);
        gbinder_reader_read_int32(&reader, &token);
        gbinder_reader_read_int32(&reader, &iohandle);
        key_value_pairs = gbinder_reader_read_string8(&reader);

        DBG("(%d) setParameters(%d, \"%s\")", token, iohandle, key_value_pairs);
        dbus_comms_set_parameters(app->dbus, key_value_pairs);

        reply = gbinder_local_object_new_reply(obj);
        gbinder_local_reply_append_int32(reply, 0);
        *status = 0;
    } else if (code == binder_idx(app, AF_GET_PARAMETERS)) {
        GBinderReader reader;
        int token;
        int iohandle;
        const char *keys;
        char *key_value_pairs = NULL;

        gbinder_remote_request_init_reader(req, &reader);
        gbinder_reader_read_int32(&reader, &token);
        gbinder_reader_read_int32(&reader, &iohandle);
        keys = gbinder_reader_read_string8(&reader);

        dbus_comms_get_parameters(app->dbus, keys, &key_value_pairs);
        DBG("(%d) getParameters(%d, \"%s\"): \"%s\"", token, iohandle, keys, key_value_pairs);

        reply = gbinder_local_object_new_reply(obj);
        gbinder_local_reply_append_string8(reply, key_value_pairs ? key_value_pairs : "");
        g_free(key_value_pairs);
        *status = 0;
    } else if (code == binder_idx(app, AF_REGISTER_CLIENT)) {
        DBG("register client");
        *status = 0;
    } else {
        ERR("Unknown code (%u)", code);
        *status = 0;
    }

    return reply;
}

static void
app_add_service_done(
        GBinderServiceManager* sm,
        int status,
        void *user_data)
{
    AfApp *app = user_data;

    if (status == GBINDER_STATUS_OK) {
        DBG("Added " SERVICE_NAME);
    } else {
        ERR("Failed to add " SERVICE_NAME " (%d)", status);
        g_main_loop_quit(app->loop);
    }
}

static void
sm_presence_handler(
        GBinderServiceManager* sm,
        void* user_data)
{
    AfApp* app = user_data;

    if (gbinder_servicemanager_is_present(app->sm)) {
        DBG("Service manager has appeared.");
        gbinder_servicemanager_add_service(app->sm, SERVICE_NAME, app->local,
                                           app_add_service_done, app);
    } else {
        DBG("Service manager has died.");
    }
}

static void
dbus_connected_cb(
        DBusComms *c,
        gboolean connected,
        void *userdata)
{
    AfApp *app = userdata;

    if (connected) {
        DBG("DBus up, connect service");
        gbinder_servicemanager_add_service(app->sm, SERVICE_NAME, app->local,
                                           app_add_service_done, app);
    }
}

gboolean
app_af_init(
        GMainLoop *mainloop,
        const AppConfig *config)
{
    const gchar *device = config->device ? config->device : BINDER_DEVICE;

    memset(&_app, 0, sizeof(_app));

    if (!g_file_test(device, G_FILE_TEST_EXISTS)) {
        int times = 60;
        DBG("Device node %s doesn't exist, let's wait for it for a while...", device);
        while (!g_file_test(device, G_FILE_TEST_EXISTS) && times--)
            usleep(500000); /* 0.5 seconds */
    }

    _app.loop = mainloop;
    _app.config = config;
    _app.sm = gbinder_servicemanager_new(device);
    _app.local = gbinder_servicemanager_new_local_object(_app.sm,
                                                         SERVICE_IFACE,
                                                         app_reply,
                                                         &_app);
    _app.presence_id = gbinder_servicemanager_add_presence_handler(_app.sm,
                                                                   sm_presence_handler,
                                                                   &_app);
    if (_app.config->dummy_mode) {
        /* add service immediately as we don't connect to DBus. */
        sm_presence_handler(_app.sm, &_app);
    } else {
        _app.dbus = dbus_comms_new(_app.config->address);
        dbus_comms_init_delayed(_app.dbus, dbus_connected_cb, &_app);
    }

    return TRUE;
}

gboolean
app_af_wait(
        void)
{
    return gbinder_servicemanager_wait(_app.sm, -1);
}

gint
app_af_done(
        void)
{
    if (_app.dbus)
        dbus_comms_done(_app.dbus);
    if (_app.sm)
        gbinder_servicemanager_unref(_app.sm);

    return 0;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
