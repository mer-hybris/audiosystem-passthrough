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

#include "common.h"
#include "impl.h"
#include "logging.h"
#include "dbus-comms.h"

#define BINDER_DEVICE                       GBINDER_DEFAULT_HWBINDER
#define QCRIL_IFACE_HW_RADIO_1_0(x)         "vendor.qti.hardware.radio.am@1.0::" x
#define QCRIL_AUDIO_HW_RADIO_1_0             QCRIL_IFACE_HW_RADIO_1_0("IQcRilAudio")
#define QCRIL_AUDIO_HW_RADIO_CALLBACK_1_0    QCRIL_IFACE_HW_RADIO_1_0("IQcRilAudioCallback")
#define QCRIL_IFACE_1_0(x)                  "vendor.qti.qcril.am@1.0::" x
#define QCRIL_AUDIO_1_0                     QCRIL_IFACE_1_0("IQcRilAudio")
#define QCRIL_AUDIO_CALLBACK_1_0            QCRIL_IFACE_1_0("IQcRilAudioCallback")

#define OFONO_RIL_SUBSCRIPTION_CONF         "/etc/ofono/ril_subscription.conf"
#define OFONO_RIL_SUBSCRIPTION_D            "/etc/ofono/ril_subscription.d"
#define OFONO_RIL_SLOTS_MAX                 (4)

struct qcril_iface_name {
    const gchar *name;
    const gchar *callback;
};

static const struct qcril_iface_name qcril_iface_names[] = {
    { QCRIL_AUDIO_HW_RADIO_1_0,     QCRIL_AUDIO_HW_RADIO_CALLBACK_1_0   },
    { QCRIL_AUDIO_1_0,              QCRIL_AUDIO_CALLBACK_1_0            },
    { NULL, NULL }
};

enum qcril_audio_methods {
    QCRIL_AUDIO_SET_CALLBACK = GBINDER_FIRST_CALL_TRANSACTION,
    QCRIL_AUDIO_SET_ERROR
};

enum qcril_audio_callback_methods {
    QCRIL_AUDIO_CALLBACK_GET_PARAMETERS = GBINDER_FIRST_CALL_TRANSACTION,
    QCRIL_AUDIO_CALLBACK_SET_PARAMETERS
};

typedef struct hidl_app HidlApp;

typedef struct am_client {
    HidlApp *app;
    char* fqname;
    gchar* slot;
    const gchar *interface_name;
    const gchar *callback_name;
    GBinderServiceManager* sm;
    GBinderLocalObject* local;
    GBinderRemoteObject* remote;
    GBinderClient* client;
    gulong wait_id;
    gulong death_id;
} AmClient;

struct hidl_app {
    GMainLoop* loop;
    const AppConfig *config;
    GBinderServiceManager* sm;
    GSList* clients;
    DBusComms *dbus;
};

static HidlApp _app;

static void
am_client_registration_handler(
        GBinderServiceManager* sm,
        const char* name,
        void* user_data);

static void
am_remote_died(
        GBinderRemoteObject* obj,
        void* user_data)
{
    AmClient* am = user_data;

    DBG("%s has died", am->fqname);
    gbinder_remote_object_unref(am->remote);
    am->remote = NULL;

    /* Wait for it to re-appear */
    am->wait_id = gbinder_servicemanager_add_registration_handler(am->sm,
        am->fqname, am_client_registration_handler, am);
}

/* IQcRilAudioCallback::getParameters(string str) generates (string) */
static gboolean
am_client_callback_get_parameters(
        AmClient* am,
        const char* str,
        GBinderLocalReply* reply)
{
    if (am->app->config->dummy_mode)
        return TRUE;

    if (str) {
        gchar* result = NULL;
        dbus_comms_get_parameters(am->app->dbus, str, &result);

        if (result) {
            GBinderWriter writer;
            gbinder_local_reply_init_writer(reply, &writer);
            gbinder_writer_append_int32(&writer, 0 /* OK */);
            gbinder_writer_append_hidl_string(&writer, result);

            return TRUE;
        }
    }

    return FALSE;
}

/* IQcRilAudioCallback::setParameters(string str) generates (int32_t) */
static gboolean
am_client_callback_set_parameters(
        AmClient* am,
        const char* str,
        GBinderLocalReply* reply)
{
    if (am->app->config->dummy_mode)
        return TRUE;

    if (str) {
        GBinderWriter writer;
        guint32 result = 0;

        result = dbus_comms_set_parameters(am->app->dbus, str);
        gbinder_local_reply_init_writer(reply, &writer);
        gbinder_writer_append_int32(&writer, 0 /* OK */);
        gbinder_writer_append_int32(&writer, result);

        return TRUE;
    }

    return FALSE;
}

static GBinderLocalReply*
am_client_callback(
        GBinderLocalObject* obj,
        GBinderRemoteRequest* req,
        guint code,
        guint flags,
        int* status,
        void* user_data)
{
    AmClient* am = user_data;
    const char* iface = gbinder_remote_request_interface(req);

    if (!g_strcmp0(iface, am->callback_name)) {
        GBinderReader reader;
        GBinderLocalReply* reply = gbinder_local_object_new_reply(obj);
        const char* str;

        gbinder_remote_request_init_reader(req, &reader);
        str = gbinder_reader_read_hidl_string_c(&reader);
        switch (code) {
        case QCRIL_AUDIO_CALLBACK_GET_PARAMETERS:
            DBG("IQcRilAudioCallback::getParameters %s %s", am->slot, str);
            if (am_client_callback_get_parameters(am, str, reply)) {
                return reply;
            }
            break;
        case QCRIL_AUDIO_CALLBACK_SET_PARAMETERS:
            DBG("IQcRilAudioCallback::setParameters %s %s", am->slot, str);
            if (am_client_callback_set_parameters(am, str, reply)) {
                return reply;
            }
            break;
        }
        /* We haven't used the reply */
        gbinder_local_reply_unref(reply);
    }
    ERR("Unexpected callback %s %u", iface, code);
    *status = GBINDER_STATUS_FAILED;
    return NULL;
}

static gboolean
am_client_connect(
        AmClient* am)
{
    int status = 0;
    int i;

    for (i = 0; qcril_iface_names[i].name; i++) {
        const gchar *interface_name = qcril_iface_names[i].name;
        const gchar *callback_name = qcril_iface_names[i].callback;
        gchar *fqname;

        fqname = g_strconcat(interface_name, "/", am->slot, NULL);
        am->remote = gbinder_servicemanager_get_service_sync(am->sm,
            fqname, &status); /* auto-released reference */
        if (am->remote) {
            am->fqname = fqname;
            am->interface_name = interface_name;
            am->callback_name = callback_name;
            break;
        } else {
            DBG("Couldn't connect to %s, trying next...", interface_name);
            g_free(fqname);
            continue;
        }
    }

    if (am->remote) {
        GBinderLocalRequest* req;

        DBG("Connected to %s", am->fqname);
        gbinder_remote_object_ref(am->remote);
        am->client = gbinder_client_new(am->remote, am->interface_name);
        am->death_id = gbinder_remote_object_add_death_handler(am->remote,
            am_remote_died, am);
        am->local = gbinder_servicemanager_new_local_object(am->sm,
            am->callback_name, am_client_callback, am);

        /* oneway IQcRilAudio::setCallback(IQcRilAudioCallback) */
        req = gbinder_client_new_request(am->client);
        gbinder_local_request_append_local_object(req, am->local);
        status = gbinder_client_transact_sync_oneway(am->client,
            QCRIL_AUDIO_SET_CALLBACK, req);
        gbinder_local_request_unref(req);
        DBG("setCallback %s status %d", am->slot, status);
        return TRUE;
    }

    ERR("No interfaces could be configured!");

    am->interface_name = qcril_iface_names[0].name;
    am->callback_name = qcril_iface_names[0].callback;
    am->fqname = g_strconcat(am->interface_name, "/", am->slot, NULL);

    ERR("TODO defaulting to wait for %s, if this is not desired please update implementation.", am->fqname);

    return FALSE;
}

static void
am_client_registration_handler(
        GBinderServiceManager* sm,
        const char* name,
        void* user_data)
{
    AmClient* am = user_data;

    if (!strcmp(name, am->fqname) && am_client_connect(am)) {
        DBG("%s has reanimated", am->fqname);
        gbinder_servicemanager_remove_handler(am->sm, am->wait_id);
        am->wait_id = 0;
    } else {
        DBG("%s appeared", name);
    }
}

static AmClient*
am_client_new(
        HidlApp *app,
        const char* slot)
{
    AmClient* am = g_new0(AmClient, 1);

    am->app = app;
    am->slot = g_strdup(slot);
    am->sm = gbinder_servicemanager_ref(app->sm);
    return am;
}

static void
am_client_connect_all(
        GSList *clients)
{
    GSList *i;

    for (i = clients; i; i = i->next) {
        AmClient *am = i->data;

        if (!am_client_connect(am)) {
            DBG("Waiting for %s", am->fqname);
            am->wait_id = gbinder_servicemanager_add_registration_handler(am->sm,
                am->fqname, am_client_registration_handler, am);
        }
    }
}

static void
am_client_free(
        gpointer data)
{
    AmClient* am = data;

    if (am->remote) {
        gbinder_remote_object_remove_handler(am->remote, am->death_id);
        gbinder_remote_object_unref(am->remote);
    }
    if (am->local) {
        gbinder_local_object_drop(am->local);
        gbinder_client_unref(am->client);
    }
    gbinder_servicemanager_remove_handler(am->sm, am->wait_id);
    gbinder_servicemanager_unref(am->sm);
    g_free(am->fqname);
    g_free(am->slot);
    g_free(am);
}

static void
am_client_remove_slot(
        HidlApp *app,
        const gchar *slot_name)
{
    GSList *i;

    for (i = app->clients; i; i = i->next) {
        AmClient *am = i->data;

        if (!g_strcmp0(slot_name, am->slot)) {
            app->clients = g_slist_delete_link(app->clients, i);
            am_client_free(am);
            break;
        }
    }
}

static void
parse_key(
        HidlApp *app,
        GKeyFile *config,
        const char *key)
{
    if (g_key_file_has_key(config, key, "transport", NULL)) {
        gchar *value;
        gchar *name;

        value = g_key_file_get_value(config, key, "transport", NULL);
        if (g_str_has_prefix(value, "binder:name")) {
            name = g_strrstr(value, "=");
            if (name && strlen(name) > 1) {
                name++;
                am_client_remove_slot(app, name);
                app->clients = g_slist_append(app->clients,
                                              am_client_new(app, name));
            }
        }
    }
}

static void
parse_slots_from_file(
        HidlApp *app,
        const gchar *filename)
{
    GKeyFile *config;

    config = g_key_file_new();
    if (g_key_file_load_from_file(config,
                                  filename,
                                  G_KEY_FILE_NONE,
                                  NULL)) {
        gint i;
        for (i = 0; i < OFONO_RIL_SLOTS_MAX; i++) {
            gchar *key = g_strdup_printf("ril_%d", i);
            parse_key(app, config, key);
            g_free(key);
        }
    }

    g_key_file_unref(config);
}

static gboolean
app_parse_all_slots(
        HidlApp *app)
{
    GDir *config_dir;

    parse_slots_from_file(app, OFONO_RIL_SUBSCRIPTION_CONF);
    if ((config_dir = g_dir_open(OFONO_RIL_SUBSCRIPTION_D, 0, NULL))) {
        const gchar *filename;
        while ((filename = g_dir_read_name(config_dir))) {
            if (g_str_has_suffix(filename, ".conf")) {
                gchar *path = g_strdup_printf(OFONO_RIL_SUBSCRIPTION_D "/%s", filename);
                parse_slots_from_file(app, path);
                g_free(path);
            }
        }
        g_dir_close(config_dir);
    }

    return app->clients ? TRUE : FALSE;
}

static void
dbus_connected_cb(
        DBusComms *c,
        gboolean connected,
        void *userdata)
{
    HidlApp *app = userdata;

    if (connected) {
        DBG("DBus up, connect clients");
        am_client_connect_all(app->clients);
    }
}

gboolean
app_qti_init(
        GMainLoop *mainloop,
        const AppConfig *config)
{
    memset(&_app, 0, sizeof(_app));
    _app.loop = mainloop;
    _app.config = config;

    _app.sm = gbinder_servicemanager_new(BINDER_DEVICE);
    if (!app_parse_all_slots(&_app))
        return FALSE;

    if (_app.config->dummy_mode) {
        ERR("Notice! HIDL helper doesn't have proper dummy mode.");
        am_client_connect_all(_app.clients);
    } else {
        _app.dbus = dbus_comms_new(_app.config->address);
        dbus_comms_init_delayed(_app.dbus, dbus_connected_cb, &_app);
    }

    return TRUE;
}

gboolean
app_qti_wait(
        void)
{
    return gbinder_servicemanager_wait(_app.sm, -1);
}

gint
app_qti_done(
        void)
{
    if (_app.dbus)
        dbus_comms_done(_app.dbus);
    g_slist_free_full(_app.clients, am_client_free);
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
