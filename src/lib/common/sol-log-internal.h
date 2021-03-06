/*
 * This file is part of the Soletta Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "sol-log.h"

#ifdef SOL_LOG_ENABLED
#define SOL_LOG_INTERNAL_DECLARE(_var, _name)    \
    struct sol_log_domain _var = {               \
        .name = "sol-" _name,                    \
        .color = SOL_LOG_COLOR_MAGENTA,          \
        .level = SOL_LOG_LEVEL_WARNING           \
    }

#define SOL_LOG_INTERNAL_DECLARE_STATIC(_var, _name)    \
    static SOL_LOG_INTERNAL_DECLARE(_var, _name)

#define SOL_LOG_INTERNAL_INIT_ONCE                               \
    do {                                                        \
        static bool _log_internal_init_once_first = true;       \
        if (_log_internal_init_once_first) {                    \
            sol_log_domain_init_level(SOL_LOG_DOMAIN);            \
            _log_internal_init_once_first = false;              \
        }                                                       \
    } while (0)
#else
#define SOL_LOG_INTERNAL_DECLARE(_var, _name)
#define SOL_LOG_INTERNAL_DECLARE_STATIC(_var, _name)
#define SOL_LOG_INTERNAL_INIT_ONCE
#ifdef SOL_LOG_DOMAIN
#undef SOL_LOG_DOMAIN
#define SOL_LOG_DOMAIN NULL
#endif
#endif
