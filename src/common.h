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


#ifndef __AUDIOSYSTEM_PASSTHROUGH_COMMON__
#define __AUDIOSYSTEM_PASSTHROUGH_COMMON__

#include <stdlib.h>

#define PASSTHROUGH_HELPER_EXE                  "audiosystem-passthrough"

#define AUDIOSYSTEM_PASSTHROUGH_PATH            "/org/sailfishos/audiosystempassthrough"
#define AUDIOSYSTEM_PASSTHROUGH_IFACE           "org.SailfishOS.AudioSystemPassthrough"

#define AUDIOSYSTEM_PASSTHROUGH_GET_PARAMETERS  "get_parameters"
#define AUDIOSYSTEM_PASSTHROUGH_SET_PARAMETERS  "set_parameters"

#define AUDIOSYSTEM_PASSTHROUGH_IMPL_STR_QTI    "qti"
#define AUDIOSYSTEM_PASSTHROUGH_IMPL_STR_AF     "af"
#define AUDIOSYSTEM_PASSTHROUGH_IMPL_STR_HW2_0  "hw2_0"

/* Read from ENV and command line arguments */
#define ENV_AUDIOSYSTEM_PASSTHROUGH_TYPE        "AUDIOSYSTEM_PASSTHROUGH_TYPE"
#define ENV_AUDIOSYSTEM_PASSTHROUGH_IDX         "AUDIOSYSTEM_PASSTHROUGH_IDX"
#define ENV_AUDIOSYSTEM_PASSTHROUGH_ADDRESS     "AUDIOSYSTEM_PASSTHROUGH_ADDRESS"

#define PULSE_ENV_LOG_LEVEL                     "PULSE_LOG"
#define PULSE_LOG_LEVEL_DEBUG                   (4)

#define DUMMY_MODE_STR                          "dummy"

static inline void log_init(unsigned int *level) {
    const char *e;

    if ((e = getenv(PULSE_ENV_LOG_LEVEL))) {
        *level = atoi(e);

        if (*level > PULSE_LOG_LEVEL_DEBUG)
            *level = PULSE_LOG_LEVEL_DEBUG;
    }
}

#endif
