#ifndef COIN_DEBUG_ERROR_INTERNAL_H
#define COIN_DEBUG_ERROR_INTERNAL_H

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
 * \file CoinDebugError.h
 * \brief C++17 replacement for Inventor/C/errors/debugerror.h
 * 
 * This file provides C++17-based replacements for the debug error
 * handling functionality, using modern C++ practices while maintaining
 * compatibility with the legacy C API.
 */

#include "Inventor/C/basic.h"
#include <functional>
#include <string>
#include <cstdarg>

namespace CoinInternal {

// ===== SEVERITY LEVELS =====

enum class DebugErrorSeverity {
    Error,
    Warning, 
    Info
};

// Legacy C enum compatibility
enum CC_DEBUGERROR_SEVERITY {
    CC_DEBUGERROR_ERROR = static_cast<int>(DebugErrorSeverity::Error),
    CC_DEBUGERROR_WARNING = static_cast<int>(DebugErrorSeverity::Warning),
    CC_DEBUGERROR_INFO = static_cast<int>(DebugErrorSeverity::Info)
};

// ===== C++ ERROR CLASSES =====

/*!
 * \brief Modern C++17 debug error class
 */
class DebugError {
public:
    DebugError(DebugErrorSeverity sev, const std::string& source, const std::string& message)
        : severity_(sev), source_(source), message_(message) {}
    
    DebugErrorSeverity getSeverity() const noexcept { return severity_; }
    const std::string& getSource() const noexcept { return source_; }
    const std::string& getMessage() const noexcept { return message_; }
    
    std::string getDebugString() const {
        const char* severityStr = "";
        switch (severity_) {
            case DebugErrorSeverity::Error:   severityStr = "ERROR"; break;
            case DebugErrorSeverity::Warning: severityStr = "WARNING"; break;
            case DebugErrorSeverity::Info:    severityStr = "INFO"; break;
        }
        return std::string(severityStr) + " in " + source_ + ": " + message_;
    }

private:
    DebugErrorSeverity severity_;
    std::string source_;
    std::string message_;
};

// ===== CALLBACK HANDLING =====

using DebugErrorCallback = std::function<void(const DebugError&)>;

/*!
 * \brief Debug error handler manager
 */
class DebugErrorHandler {
public:
    static DebugErrorHandler& instance() {
        static DebugErrorHandler handler;
        return handler;
    }
    
    void setCallback(DebugErrorCallback callback) {
        callback_ = callback;
    }
    
    void handleError(const DebugError& error) {
        if (callback_) {
            callback_(error);
        } else {
            defaultHandler(error);
        }
    }
    
    // For C API compatibility
    void setCallbackC(void (*func)(const void*, void*), void* data) {
        callback_c_ = func;
        callback_data_ = data;
    }
    
    void* getCallbackC() const { return reinterpret_cast<void*>(callback_c_); }
    void* getCallbackData() const { return callback_data_; }

private:
    DebugErrorCallback callback_;
    void (*callback_c_)(const void*, void*) = nullptr;
    void* callback_data_ = nullptr;
    
    static void defaultHandler(const DebugError& error) {
        // Default implementation - print to stderr
        std::fprintf(stderr, "Coin %s\n", error.getDebugString().c_str());
        std::fflush(stderr);
    }
};

// ===== CONVENIENCE FUNCTIONS =====

/*!
 * \brief Post a debug error message
 */
template<typename... Args>
void postDebugError(const char* source, const char* format, Args&&... args) {
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), format, std::forward<Args>(args)...);
    
    DebugError error(DebugErrorSeverity::Error, source ? source : "unknown", buffer);
    DebugErrorHandler::instance().handleError(error);
}

/*!
 * \brief Post a debug warning message
 */
template<typename... Args>
void postDebugWarning(const char* source, const char* format, Args&&... args) {
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), format, std::forward<Args>(args)...);
    
    DebugError error(DebugErrorSeverity::Warning, source ? source : "unknown", buffer);
    DebugErrorHandler::instance().handleError(error);
}

/*!
 * \brief Post a debug info message
 */
template<typename... Args>
void postDebugInfo(const char* source, const char* format, Args&&... args) {
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), format, std::forward<Args>(args)...);
    
    DebugError error(DebugErrorSeverity::Info, source ? source : "unknown", buffer);
    DebugErrorHandler::instance().handleError(error);
}

} // namespace CoinInternal

// ===== C API COMPATIBILITY LAYER =====

// For compatibility with existing code that uses the C API
extern "C" {

// Legacy C structure for compatibility
struct cc_debugerror {
    void* super;  // cc_error placeholder
    int severity;
};

using cc_debugerror_cb = void (*)(const cc_debugerror*, void*);

// C API functions that wrap the C++ implementation
COIN_DLL_API void cc_debugerror_post(const char* source, const char* format, ...);
COIN_DLL_API void cc_debugerror_postwarning(const char* source, const char* format, ...);
COIN_DLL_API void cc_debugerror_postinfo(const char* source, const char* format, ...);

COIN_DLL_API void cc_debugerror_init(cc_debugerror* me);
COIN_DLL_API void cc_debugerror_clean(cc_debugerror* me);

COIN_DLL_API int cc_debugerror_get_severity(const cc_debugerror* me);

COIN_DLL_API void cc_debugerror_set_handler_callback(cc_debugerror_cb function, void* data);
COIN_DLL_API cc_debugerror_cb cc_debugerror_get_handler_callback(void);
COIN_DLL_API void* cc_debugerror_get_handler_data(void);
COIN_DLL_API cc_debugerror_cb cc_debugerror_get_handler(void** data);

} // extern "C"

#endif // COIN_DEBUG_ERROR_INTERNAL_H