#ifndef COIN_SOENVIRONMENT_H
#define COIN_SOENVIRONMENT_H

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
 * \file SoEnvironment.h
 * \brief Modern C++17 environment variable utilities for Coin3D internal use
 * 
 * This file provides C++17-based replacements for the legacy coin_getenv() 
 * and coin_setenv() functions, using standard library features and modern
 * C++ practices.
 */

#include <cstdlib>
#include <string>
#include <optional>

// Internal namespace for Coin3D implementation details
namespace CoinInternal {

/*!
 * \brief Get environment variable value using modern C++17
 * 
 * This function replaces the legacy coin_getenv() with a C++17 implementation
 * that returns an optional string instead of a raw pointer.
 * 
 * \param name Environment variable name
 * \return Optional string containing the value, or nullopt if not set
 */
inline std::optional<std::string> getEnvironmentVariable(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    if (value != nullptr) {
        return std::string(value);
    }
    return std::nullopt;
}

/*!
 * \brief Get environment variable value with default
 * 
 * Convenience function that returns a default value if the environment
 * variable is not set.
 * 
 * \param name Environment variable name
 * \param defaultValue Default value to return if not set
 * \return Environment variable value or default
 */
inline std::string getEnvironmentVariable(const std::string& name, const std::string& defaultValue) {
    auto value = getEnvironmentVariable(name);
    return value.value_or(defaultValue);
}

/*!
 * \brief Check if environment variable is set (legacy coin_getenv compatibility)
 * 
 * For compatibility with existing code that checks if coin_getenv() returns nullptr.
 * 
 * \param name Environment variable name
 * \return Raw pointer to environment variable value, or nullptr if not set
 */
inline const char* getEnvironmentVariableRaw(const std::string& name) {
    return std::getenv(name.c_str());
}

/*!
 * \brief Set environment variable using modern C++17
 * 
 * This function replaces coin_setenv() with a simplified C++17 implementation.
 * Note: This is only for internal use and not part of the public API.
 * 
 * \param name Environment variable name
 * \param value Environment variable value
 * \param overwrite Whether to overwrite existing values
 * \return true if successful, false otherwise
 */
inline bool setEnvironmentVariable(const std::string& name, const std::string& value, bool overwrite = true) {
#ifdef _WIN32
    if (!overwrite && std::getenv(name.c_str()) != nullptr) {
        return true; // Variable exists and we don't want to overwrite
    }
    return _putenv_s(name.c_str(), value.c_str()) == 0;
#else
    return setenv(name.c_str(), value.c_str(), overwrite ? 1 : 0) == 0;
#endif
}

} // namespace CoinInternal

#endif // COIN_SOENVIRONMENT_H