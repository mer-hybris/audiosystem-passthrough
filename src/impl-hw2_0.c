/*
 * Copyright (C) 2020 Open Mobile Platform LLC.
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

#define BINDER_DEVICE           GBINDER_DEFAULT_HWBINDER

#define AUDIO_IFACE_2_0(x)      "android.hardware.audio@2.0::" x
#define AUDIO_FACTORY_2_0       AUDIO_IFACE_2_0("IDevicesFactory")
#define AUDIO_DEVICE_2_0        AUDIO_IFACE_2_0("IDevice")
#define AUDIO_SERVICE_NAME      "default"

enum i_devices_factory_methods {
    IDEVICESFACTORY_OPEN_DEVICE = GBINDER_FIRST_CALL_TRANSACTION
};

enum i_device_methods {
    IDEVICE_INIT_CHECK = GBINDER_FIRST_CALL_TRANSACTION,
    IDEVICE_GET_PARAMETERS = GBINDER_FIRST_CALL_TRANSACTION + 17,
    IDEVICE_SET_PARAMETERS
};

typedef struct hw2_0_app HW20App;

struct hw2_0_app {
    GMainLoop* loop;
    const AppConfig *config;
    gulong presence_id;
    GBinderLocalObject* idevicefactory;
    GBinderLocalObject* idevice;
    GBinderServiceManager* sm;
    DBusComms *dbus;
};

static HW20App _app;

static GBinderLocalReply*
handle_calls_cb(
        GBinderLocalObject* obj,
        GBinderRemoteRequest* req,
        guint code,
        guint flags,
        int* status,
        void* user_data);

static void
dbus_connected_cb(
        DBusComms *c,
        gboolean connected,
        void *userdata)
{
    HW20App *app = userdata;

    if (connected) {
        DBG("DBus up, connect clients");
        gbinder_servicemanager_add_service_sync(app->sm,
                    AUDIO_SERVICE_NAME, app->idevicefactory);
    }
}

static GBinderLocalReply*
handle_calls_cb(
        GBinderLocalObject* obj,
        GBinderRemoteRequest* req,
        guint code,
        guint flags,
        int* status,
        void* user_data)
{
    HW20App *app = user_data;
    const char* iface = gbinder_remote_request_interface(req);
    GBinderLocalReply* reply = NULL;

    if (!g_strcmp0(iface, AUDIO_FACTORY_2_0)) {
        switch (code) {
            case IDEVICESFACTORY_OPEN_DEVICE: {
                GBinderWriter writer;
                DBG("openDevice method called");
                reply = gbinder_local_object_new_reply(obj);
                gbinder_local_reply_init_writer(reply, &writer);

                if (!app->idevice) {
                    app->idevice = gbinder_servicemanager_new_local_object(app->sm,
                                    AUDIO_DEVICE_2_0, handle_calls_cb, app);
                }

                gbinder_writer_append_int32(&writer, GBINDER_STATUS_OK);
                gbinder_writer_append_local_object(&writer, app->idevice);
                *status = GBINDER_STATUS_OK;
                break;
            }
            default:
                ERR("Unspecified method called: Number %u in IDevicesFactory.hal", code);
                break;
        }
    }
    else if (!g_strcmp0(iface, AUDIO_DEVICE_2_0)) {
        switch (code) {
            case IDEVICE_INIT_CHECK: {
                GBinderWriter writer;
                DBG("initCheck method called");
                reply = gbinder_local_object_new_reply(obj);
                gbinder_local_reply_init_writer(reply, &writer);
                gbinder_writer_append_int32(&writer, GBINDER_STATUS_OK);
                *status = GBINDER_STATUS_OK;
                break;
            }
            case IDEVICE_GET_PARAMETERS: {
                GBinderWriter writer;
                DBG("getParameters method called");
                reply = gbinder_local_object_new_reply(obj);
                gbinder_local_reply_init_writer(reply, &writer);
                gbinder_writer_append_int32(&writer, GBINDER_STATUS_OK);
                *status = GBINDER_STATUS_OK;
                break;
            }
            case IDEVICE_SET_PARAMETERS: {
                DBG("setParameters method called");
                reply = gbinder_local_object_new_reply(obj);
                gbinder_local_reply_append_int32(reply, GBINDER_STATUS_OK);
                *status = GBINDER_STATUS_OK;
                break;
            }
            default: /* No need to do anything with other calls */
                ERR("Unspecified method called: Number %u in IDevice.hal", code);
                *status = GBINDER_STATUS_OK;
                break;
        }
    }

    return reply;
}

static void
sm_presence_handler(
        GBinderServiceManager* sm,
        void* _app)
{
    HW20App* app = _app;

    if (gbinder_servicemanager_is_present(app->sm)) {
        DBG("Service manager has reappeared.");

        gbinder_servicemanager_add_service_sync(app->sm,
                    AUDIO_SERVICE_NAME, app->idevicefactory);
    } else {
        DBG("Service manager has died.");
    }
}

gboolean
app_hw2_0_init(
        GMainLoop *mainloop,
        const AppConfig *config)
{
    memset(&_app, 0, sizeof(_app));
    _app.loop = mainloop;
    _app.config = config;
    _app.sm = gbinder_servicemanager_new(BINDER_DEVICE);
    _app.idevicefactory = gbinder_servicemanager_new_local_object(_app.sm,
                                                                  AUDIO_FACTORY_2_0, 
                                                                  handle_calls_cb, 
                                                                  &_app);
    _app.presence_id = gbinder_servicemanager_add_presence_handler(_app.sm,
                                                                   sm_presence_handler,
                                                                   &_app);
    
    if (_app.config->dummy_mode) {
        /* add service immediately as we don't connect to DBus. */
        sm_presence_handler(_app.sm, &_app);
    } else {
        ERR("Notice! This implementation doesn't do other than dummy mode for now");
        _app.dbus = dbus_comms_new(_app.config->address);
        dbus_comms_init_delayed(_app.dbus, dbus_connected_cb, &_app);
    }

    return TRUE;
}

gboolean
app_hw2_0_wait(
        void)
{
    return gbinder_servicemanager_wait(_app.sm, -1);
}

gint
app_hw2_0_done(
        void)
{
    if (_app.dbus)
        dbus_comms_done(_app.dbus);
    if (_app.sm) {
        gbinder_servicemanager_remove_handler(_app.sm, _app.presence_id);
        gbinder_servicemanager_unref(_app.sm);
    }
    if (_app.idevice)
        gbinder_local_object_drop(_app.idevice);
    if (_app.idevicefactory)
        gbinder_local_object_drop(_app.idevicefactory);
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
