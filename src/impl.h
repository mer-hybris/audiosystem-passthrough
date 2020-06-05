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


#ifndef _AUDIOSYSTEM_PASSTHROUGH_HELPER_IMPL_
#define _AUDIOSYSTEM_PASSTHROUGH_HELPER_IMPL_

#include "common.h"

enum app_type {
    APP_QTI,
    APP_AF,
    APP_HW2_0,
    APP_MAX
};

typedef struct app_config {
    gchar *address;
    gboolean dummy_mode;
    gboolean verbose;
    gint binder_index;
} AppConfig;

typedef gboolean (*app_init_cb)(GMainLoop *mainloop, const AppConfig *config);
typedef gboolean (*app_wait_cb)(void);
typedef gint     (*app_done_cb)(void);

typedef struct app_implementation {
    const char *name;
    app_init_cb init;
    app_wait_cb wait;
    app_done_cb done;
} AppImplementation;

gboolean
app_af_init(
        GMainLoop *mainloop,
        const AppConfig *config);

gboolean
app_af_wait(
        void);

gint
app_af_done(
        void);


gboolean
app_qti_init(
        GMainLoop *mainloop,
        const AppConfig *config);

gboolean
app_qti_wait(
        void);

gint
app_qti_done(
        void);

gboolean
app_hw2_0_init(
        GMainLoop *mainloop,
        const AppConfig *config);

gboolean
app_hw2_0_wait(
        void);

gint
app_hw2_0_done(
        void);

#endif
