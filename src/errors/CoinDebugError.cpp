/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/*!
 * \file CoinDebugError.cpp
 * \brief C++17 implementation of debug error handling with C API compatibility
 */

#include "CoinDebugError.h"
#include <cstdarg>
#include <memory>

// ===== C API COMPATIBILITY IMPLEMENTATION =====

extern "C" {

void cc_debugerror_post(const char* source, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    CoinInternal::DebugError error(CoinInternal::DebugErrorSeverity::Error, 
                                  source ? source : "unknown", buffer);
    CoinInternal::DebugErrorHandler::instance().handleError(error);
}

void cc_debugerror_postwarning(const char* source, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    CoinInternal::DebugError error(CoinInternal::DebugErrorSeverity::Warning, 
                                  source ? source : "unknown", buffer);
    CoinInternal::DebugErrorHandler::instance().handleError(error);
}

void cc_debugerror_postinfo(const char* source, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    CoinInternal::DebugError error(CoinInternal::DebugErrorSeverity::Info, 
                                  source ? source : "unknown", buffer);
    CoinInternal::DebugErrorHandler::instance().handleError(error);
}

void cc_debugerror_init(cc_debugerror* me) {
    if (me) {
        me->super = nullptr;
        me->severity = CC_DEBUGERROR_ERROR;
    }
}

void cc_debugerror_clean(cc_debugerror* me) {
    // Nothing to clean in the C++17 implementation
    (void)me;
}

int cc_debugerror_get_severity(const cc_debugerror* me) {
    return me ? me->severity : CC_DEBUGERROR_ERROR;
}

void cc_debugerror_set_handler_callback(cc_debugerror_cb function, void* data) {
    CoinInternal::DebugErrorHandler::instance().setCallbackC(
        reinterpret_cast<void(*)(const void*, void*)>(function), data);
}

cc_debugerror_cb cc_debugerror_get_handler_callback(void) {
    return reinterpret_cast<cc_debugerror_cb>(
        CoinInternal::DebugErrorHandler::instance().getCallbackC());
}

void* cc_debugerror_get_handler_data(void) {
    return CoinInternal::DebugErrorHandler::instance().getCallbackData();
}

cc_debugerror_cb cc_debugerror_get_handler(void** data) {
    if (data) {
        *data = cc_debugerror_get_handler_data();
    }
    return cc_debugerror_get_handler_callback();
}

} // extern "C"