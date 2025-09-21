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
 * \file CoinTidbits.cpp
 * \brief Consolidated C++17 utility functions for Coin
 * 
 * This file consolidates functionality from the former tidbits files:
 * - src/tidbits.cpp
 * - src/C/tidbits.h
 * - src/tidbitsp.h
 * 
 * Contains various utility functions that don't really belong anywhere 
 * specific in Coin, but which are included to keep Coin portable.
 */

#include "C/CoinTidbits.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <cassert>
#include <cerrno>
#include <cmath>
#include <cfloat>
#include <clocale>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <vector>
#include <algorithm>
#include <mutex>

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif /* HAVE_WINDOWS_H */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_IO_H
#include <io.h>
#endif /* HAVE_IO_H */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_IEEEFP_H
#include <ieeefp.h>
#endif /* HAVE_IEEEFP_H */

#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif /* HAVE_DIRECT_H */

#include "C/base/string.h"
#include "base/list.h"
#include "C/errors/debugerror.h"
#include "coindefs.h"
#include "misc/SoEnvironment.h"

/* ********************************************************************** */

#ifdef __cplusplus
extern "C" {
#endif

/* ********************************************************************** */
/* Global state variables */

#ifdef COIN_THREADSAFE
static std::mutex atexit_list_monitor;
#endif /* COIN_THREADSAFE */

static int COIN_DEBUG_EXTRA = -1;
static int COIN_DEBUG_NORMALIZE = -1;

/* ********************************************************************** */
/* Initialization function */

void coin_init_tidbits(void)
{
    const char* env;
    
    env = CoinInternal::getEnvironmentVariableRaw("COIN_DEBUG_EXTRA");
    if (env && atoi(env) == 1) {
        COIN_DEBUG_EXTRA = 1;
    } else {
        COIN_DEBUG_EXTRA = 0;
    }
    
    env = CoinInternal::getEnvironmentVariableRaw("COIN_DEBUG_NORMALIZE");
    if (env && atoi(env) == 1) {
        COIN_DEBUG_NORMALIZE = 1;
    } else {
        COIN_DEBUG_NORMALIZE = 0;
    }
}

/* ********************************************************************** */
/* sprintf/snprintf wrapper functions */

typedef int func_vsnprintf(char*, size_t, const char*, va_list);

static int coin_common_vsnprintf(func_vsnprintf* func,
                                char* dst, size_t n,
                                const char* fmtstr, va_list args) __attribute__((unused))
{
    int length;
    static int debug = -1;
    
    if (debug == -1) {
        const char* env = CoinInternal::getEnvironmentVariableRaw("COIN_DEBUG_NPRINTF");
        debug = (env && (atoi(env) > 0)) ? 1 : 0;
    }
    
    assert(n > 1);
    n -= 1;
    
    if (debug) {
        printf("dst==%p, n==%zu, fmtstr=='%s'\n", dst, n, fmtstr);
    }
    
#ifdef HAVE_VA_COPY_MACRO
    va_list argscopy;
    va_copy(argscopy, args);
    length = (*func)(dst, n, fmtstr, argscopy);
    va_end(argscopy);
#else
    length = (*func)(dst, n, fmtstr, args);
#endif
    
    if (debug) {
        printf("==> length==%d\n", length);
    }
    
    if (length >= (((int)n) - 1)) {
        length = -1;
    }
    
    return length;
}

#ifdef HAVE_VSNPRINTF

int coin_vsnprintf(char* dst, unsigned int n, const char* fmtstr, va_list args)
{
    return coin_common_vsnprintf(vsnprintf, dst, (size_t)n, fmtstr, args);
}

#elif defined HAVE__VSNPRINTF

int coin_vsnprintf(char* dst, unsigned int n, const char* fmtstr, va_list args)
{
    return coin_common_vsnprintf(_vsnprintf, dst, (size_t)n, fmtstr, args);
}

#else /* neither vsnprintf() nor _vsnprintf() available, roll our own */

static FILE* nullfileptr(void)
{
    static FILE* nullfp = NULL;
    
    if (!nullfp) {
        struct stat sbuf;
        const char unixdevnull[] = "/dev/null";
        int nullfd = -1;
        
        if ((stat(unixdevnull, &sbuf) == 0) && (sbuf.st_mode & S_IFCHR))
            nullfd = open(unixdevnull, O_WRONLY);
        
        if (nullfd == -1) {
            const char* tmpname = tmpnam(NULL);
            if (tmpname) {
                nullfd = open(tmpname, O_CREAT|O_WRONLY, 0666);
                (void)remove(tmpname);
            }
        }
        
        if (nullfd != -1) nullfp = fdopen(nullfd, "w");
        if (!nullfp) nullfp = stderr;
    }
    
    return nullfp;
}

int coin_vsnprintf(char* dst, unsigned int n, const char* fmtstr, va_list args)
{
    int len;
    
#ifdef HAVE_VA_COPY_MACRO
    va_list argscopy;
    
    va_copy(argscopy, args);
    len = vfprintf(nullfileptr(), fmtstr, argscopy);
    va_end(argscopy);
    
    if (((unsigned int) len + 1) > n) return -1;
    
    va_copy(argscopy, args);
    (void)vsprintf(dst, fmtstr, argscopy);
    va_end(argscopy);
#else
    len = vfprintf(nullfileptr(), fmtstr, args);
    if (((unsigned int) len + 1) > n) return -1;
    (void)vsprintf(dst, fmtstr, args);
#endif
    
    return len;
}

#endif /* coin_vsnprintf() */

int coin_snprintf(char* dst, unsigned int n, const char* fmtstr, ...)
{
    int len;
    va_list argptr;
    va_start(argptr, fmtstr);
    len = coin_vsnprintf(dst, n, fmtstr, argptr);
    va_end(argptr);
    return len;
}

/* ********************************************************************** */
/* Environment variable functions */

#ifdef HAVE_GETENVIRONMENTVARIABLE
static struct envvar_data* envlist_head = NULL;
static struct envvar_data* envlist_tail = NULL;

struct envvar_data {
    char* name;
    char* val;
    struct envvar_data* next;
};

static void envlist_cleanup(void)
{
    struct envvar_data* ptr = envlist_head;
    while (ptr != NULL) {
        struct envvar_data* tmp = ptr;
        free(ptr->name);
        free(ptr->val);
        ptr = ptr->next;
        free(tmp);
    }
    envlist_head = NULL;
    envlist_tail = NULL;
}

static void envlist_append(struct envvar_data* item)
{
    item->next = NULL;
    if (envlist_head == NULL) {
        envlist_head = item;
        envlist_tail = item;
        coin_atexit_func("envlist_cleanup", envlist_cleanup, CC_ATEXIT_ENVIRONMENT);
    } else {
        envlist_tail->next = item;
        envlist_tail = item;
    }
}
#endif /* HAVE_GETENVIRONMENTVARIABLE */

const char* coin_getenv(const char* envname)
{
#ifdef HAVE_GETENVIRONMENTVARIABLE
    int neededsize;
    neededsize = GetEnvironmentVariable(envname, NULL, 0);
    
    if (neededsize >= 1) {
        int resultsize;
        struct envvar_data* envptr;
        char* valbuf = (char*)malloc(neededsize);
        if (valbuf == NULL) {
            return NULL;
        }
        resultsize = GetEnvironmentVariable(envname, valbuf, neededsize);
        if (resultsize != (neededsize - 1)) {
            free(valbuf);
            return NULL;
        }
        
        envptr = envlist_head;
        while ((envptr != NULL) && (strcmp(envptr->name, envname) != 0))
            envptr = envptr->next;
        
        if (envptr != NULL) {
            free(envptr->val);
            envptr->val = valbuf;
        } else {
            envptr = (struct envvar_data*)malloc(sizeof(struct envvar_data));
            if (envptr == NULL) {
                free(valbuf);
                return NULL;
            }
            envptr->name = strdup(envname);
            if (envptr->name == NULL) {
                free(envptr);
                free(valbuf);
                return NULL;
            }
            envptr->val = valbuf;
            envlist_append(envptr);
        }
        return envptr->val;
    }
    return NULL;
#else
    return getenv(envname);
#endif
}

SbBool coin_setenv(const char* name, const char* value, int overwrite)
{
#ifdef HAVE_GETENVIRONMENTVARIABLE
    struct envvar_data* envptr, * prevptr;
    envptr = envlist_head;
    prevptr = NULL;
    while ((envptr != NULL) && (strcmp(envptr->name, name) != 0)) {
        prevptr = envptr;
        envptr = envptr->next;
    }
    if (envptr) {
        if (prevptr) prevptr->next = envptr->next;
        else envlist_head = envptr->next;
        if (envlist_tail == envptr) envlist_tail = prevptr;
        free(envptr->name);
        free(envptr->val);
        free(envptr);
    }
    
    if (overwrite || (GetEnvironmentVariable(name, NULL, 0) == 0))
        return SetEnvironmentVariable(name, value) ? COIN_TRUE : COIN_FALSE;
    else
        return COIN_TRUE;
#else
    return (setenv(name, value, overwrite) == 0);
#endif
}

void coin_unsetenv(const char* name)
{
#ifdef HAVE_GETENVIRONMENTVARIABLE
    struct envvar_data* envptr, * prevptr;
    envptr = envlist_head;
    prevptr = NULL;
    while ((envptr != NULL) && (strcmp(envptr->name, name) != 0)) {
        prevptr = envptr;
        envptr = envptr->next;
    }
    if (envptr) {
        if (prevptr) prevptr->next = envptr->next;
        else envlist_head = envptr->next;
        if (envlist_tail == envptr) envlist_tail = prevptr;
        free(envptr->name);
        free(envptr->val);
        free(envptr);
    }
    SetEnvironmentVariable(name, NULL);
#else
    unsetenv(name);
#endif
}

/* ********************************************************************** */
/* Endianness and byte order functions */

#define COIN_BSWAP_8(x)  ((x) & 0xff)
#define COIN_BSWAP_16(x) ((COIN_BSWAP_8(x)  << 8)  | COIN_BSWAP_8((x)  >> 8))
#define COIN_BSWAP_32(x) ((COIN_BSWAP_16(x) << 16) | COIN_BSWAP_16((x) >> 16))
#define COIN_BSWAP_64(x) ((COIN_BSWAP_32(x) << 32) | COIN_BSWAP_32((x) >> 32))

static int coin_endianness = COIN_HOST_IS_UNKNOWNENDIAN;

int coin_host_get_endianness(void)
{
    union temptype {
        uint32_t value;
        uint8_t  bytes[4];
    } temp;
    
    if (coin_endianness != COIN_HOST_IS_UNKNOWNENDIAN) {
        return coin_endianness;
    }
    
    temp.bytes[0] = 0x00;
    temp.bytes[1] = 0x01;
    temp.bytes[2] = 0x02;
    temp.bytes[3] = 0x03;
    switch (temp.value) {
        case 0x03020100: return coin_endianness = COIN_HOST_IS_LITTLEENDIAN;
        case 0x00010203: return coin_endianness = COIN_HOST_IS_BIGENDIAN;
    }
    assert(0 && "system has unknown endianness");
    return COIN_HOST_IS_UNKNOWNENDIAN;
}

std::uint16_t coin_hton_uint16(std::uint16_t value)
{
    switch (coin_host_get_endianness()) {
    case COIN_HOST_IS_BIGENDIAN:
        break;
    case COIN_HOST_IS_LITTLEENDIAN:
        value = COIN_BSWAP_16(value);
        break;
    default:
        assert(0 && "system has unknown endianness");
    }
    return value;
}

std::uint16_t coin_ntoh_uint16(std::uint16_t value)
{
    return coin_hton_uint16(value);
}

std::uint32_t coin_hton_uint32(std::uint32_t value)
{
    switch (coin_host_get_endianness()) {
    case COIN_HOST_IS_BIGENDIAN:
        break;
    case COIN_HOST_IS_LITTLEENDIAN:
        value = COIN_BSWAP_32(value);
        break;
    default:
        assert(0 && "system has unknown endianness");
    }
    return value;
}

std::uint32_t coin_ntoh_uint32(std::uint32_t value)
{
    return coin_hton_uint32(value);
}

std::uint64_t coin_hton_uint64(std::uint64_t value)
{
    switch (coin_host_get_endianness()) {
    case COIN_HOST_IS_BIGENDIAN:
        break;
    case COIN_HOST_IS_LITTLEENDIAN:
        value = COIN_BSWAP_64(value);
        break;
    default:
        assert(0 && "system has unknown endianness");
    }
    return value;
}

std::uint64_t coin_ntoh_uint64(std::uint64_t value)
{
    return coin_hton_uint64(value);
}

void coin_hton_float_bytes(float value, char* result)
{
    union {
        float f32;
        uint32_t u32;
    } val;
    
    assert(sizeof(float) == sizeof(uint32_t));
    val.f32 = value;
    val.u32 = coin_hton_uint32(val.u32);
    memcpy(result, &val.u32, sizeof(uint32_t));
}

float coin_ntoh_float_bytes(const char* value)
{
    union {
        float f32;
        uint32_t u32;
    } val;
    
    assert(sizeof(float) == sizeof(uint32_t));
    memcpy(&val.u32, value, sizeof(uint32_t));
    val.u32 = coin_ntoh_uint32(val.u32);
    return val.f32;
}

void coin_hton_double_bytes(double value, char* result)
{
    union {
        double d64;
        uint64_t u64;
    } val;
    
    assert(sizeof(double) == sizeof(uint64_t));
    val.d64 = value;
    val.u64 = coin_hton_uint64(val.u64);
    memcpy(result, &val.u64, sizeof(uint64_t));
}

double coin_ntoh_double_bytes(const char* value)
{
    union {
        double d64;
        uint64_t u64;
    } val;
    
    assert(sizeof(double) == sizeof(uint64_t));
    memcpy(&val.u64, value, sizeof(uint64_t));
    val.u64 = coin_ntoh_uint64(val.u64);
    return val.d64;
}

/* ********************************************************************** */
/* Power of two functions */

SbBool coin_is_power_of_two(std::uint32_t x)
{
    return (x != 0) && ((x & (x - 1)) == 0);
}

std::uint32_t coin_next_power_of_two(std::uint32_t x)
{
    assert((x < (uint32_t)(1 << 31)) && "overflow");
    
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    
    return x + 1;
}

std::uint32_t coin_geq_power_of_two(std::uint32_t x)
{
    if (coin_is_power_of_two(x)) return x;
    return coin_next_power_of_two(x);
}

/* ********************************************************************** */
/* Jitter function */

void coin_viewvolume_jitter(int numpasses, int curpass, const int* vpsize, float* jitter)
{
    /* jitter values from OpenGL Programming Guide */
    static float jitter2[] = {
        0.25f, 0.75f,
        0.75f, 0.25f
    };
    static float jitter3[] = {
        0.5033922635f, 0.8317967229f,
        0.7806016275f, 0.2504380877f,
        0.2261828938f, 0.4131553612f
    };
    static float jitter4[] = {
        0.375f, 0.25f,
        0.125f, 0.75f,
        0.875f, 0.25f,
        0.625f, 0.75f
    };
    static float jitter5[] = {
        0.5f, 0.5f,
        0.3f, 0.1f,
        0.7f, 0.9f,
        0.9f, 0.3f,
        0.1f, 0.7f
    };
    static float jitter6[] = {
        0.4646464646f, 0.4646464646f,
        0.1313131313f, 0.7979797979f,
        0.5353535353f, 0.8686868686f,
        0.8686868686f, 0.5353535353f,
        0.7979797979f, 0.1313131313f,
        0.2020202020f, 0.2020202020f
    };
    static float jitter8[] = {
        0.5625f, 0.4375f,
        0.0625f, 0.9375f,
        0.3125f, 0.6875f,
        0.6875f, 0.8125f,
        0.8125f, 0.1875f,
        0.9375f, 0.5625f,
        0.4375f, 0.0625f,
        0.1875f, 0.3125f
    };
    static float jitter9[] = {
        0.5f, 0.5f,
        0.1666666666f, 0.9444444444f,
        0.5f, 0.1666666666f,
        0.5f, 0.8333333333f,
        0.1666666666f, 0.2777777777f,
        0.8333333333f, 0.3888888888f,
        0.1666666666f, 0.6111111111f,
        0.8333333333f, 0.7222222222f,
        0.8333333333f, 0.0555555555f
    };
    static float jitter12[] = {
        0.4166666666f, 0.625f,
        0.9166666666f, 0.875f,
        0.25f, 0.375f,
        0.4166666666f, 0.125f,
        0.75f, 0.125f,
        0.0833333333f, 0.125f,
        0.75f, 0.625f,
        0.25f, 0.875f,
        0.5833333333f, 0.375f,
        0.9166666666f, 0.375f,
        0.0833333333f, 0.625f,
        0.583333333f, 0.875f
    };
    static float jitter16[] = {
        0.375f, 0.4375f,
        0.625f, 0.0625f,
        0.875f, 0.1875f,
        0.125f, 0.0625f,
        0.375f, 0.6875f,
        0.875f, 0.4375f,
        0.625f, 0.5625f,
        0.375f, 0.9375f,
        0.625f, 0.3125f,
        0.125f, 0.5625f,
        0.125f, 0.8125f,
        0.375f, 0.1875f,
        0.875f, 0.9375f,
        0.875f, 0.6875f,
        0.125f, 0.3125f,
        0.625f, 0.8125f
    };
    
    static float* jittertab[] = {
        jitter2,
        jitter3,
        jitter4,
        jitter5,
        jitter6,
        jitter8,
        jitter8,
        jitter9,
        jitter12,
        jitter12,
        jitter12,
        jitter16,
        jitter16,
        jitter16,
        jitter16
    };
    
    float* jittab;
    
    if (numpasses > 16) numpasses = 16;
    if (curpass >= numpasses) curpass = numpasses - 1;
    
    jittab = jittertab[numpasses-2];
    
    if (numpasses < 2) {
        jitter[0] = 0.0f;
        jitter[1] = 0.0f;
        jitter[2] = 0.0f;
    } else {
        jitter[0] = (jittab[curpass*2] - 0.5f) * 2.0f / ((float)vpsize[0]);
        jitter[1] = (jittab[curpass*2+1] - 0.5f) * 2.0f / ((float)vpsize[1]);
        jitter[2] = 0.0f;
    }
}

/* ********************************************************************** */
/* Atexit system */

void free_std_fds(void);

typedef struct {
    char* name;
    coin_atexit_f* func;
    int32_t priority;
    uint32_t cnt;
} tb_atexit_data;

static cc_list* atexit_list = NULL;
static SbBool isexiting = COIN_FALSE;

static int atexit_qsort_cb(const void* q0, const void* q1)
{
    tb_atexit_data* p0, * p1;
    
    p0 = *((tb_atexit_data**) q0);
    p1 = *((tb_atexit_data**) q1);
    
    if (p0->priority < p1->priority) return -1;
    if (p0->priority > p1->priority) return 1;
    
    if (p0->cnt < p1->cnt) return -1;
    return 1;
}

void coin_atexit_cleanup(void)
{
    int i, n;
    tb_atexit_data* data;
    const char* debugstr;
    SbBool debug = COIN_FALSE;
    
    if (!atexit_list) return;
    
    isexiting = COIN_TRUE;
    
    debugstr = CoinInternal::getEnvironmentVariableRaw("COIN_DEBUG_CLEANUP");
    debug = debugstr && (atoi(debugstr) > 0);
    
    n = cc_list_get_length(atexit_list);
    qsort(cc_list_get_array(atexit_list), n, sizeof(void*), atexit_qsort_cb);
    
    for (i = n-1; i >= 0; i--) {
        data = (tb_atexit_data*) cc_list_get(atexit_list, i);
        if (debug) {
            fprintf(stdout, "coin_atexit_cleanup: invoking %s()\n", data->name);
        }
        data->func();
        free(data->name);
        free(data);
    }
    
    free_std_fds();
    
    cc_list_destruct(atexit_list);
    atexit_list = NULL;
    isexiting = COIN_FALSE;
    
    if (debug) {
        fprintf(stdout, "coin_atexit_cleanup: fini\n");
    }
}

void coin_atexit_func(const char* name, coin_atexit_f* f, coin_atexit_priorities priority)
{
#ifdef COIN_THREADSAFE
    atexit_list_monitor.lock();
#endif /* COIN_THREADSAFE */
    
    assert(!isexiting && "tried to attach an atexit function while exiting");
    
    if (atexit_list == NULL) {
        atexit_list = cc_list_construct();
    }
    
    {
        tb_atexit_data* data;
        
        data = (tb_atexit_data*) malloc(sizeof(tb_atexit_data));
        data->name = strdup(name);
        data->func = f;
        data->priority = priority;
        data->cnt = cc_list_get_length(atexit_list);
        
        cc_list_append(atexit_list, data);
    }
    
#ifdef COIN_THREADSAFE
    atexit_list_monitor.unlock();
#endif /* COIN_THREADSAFE */
}

void cc_coin_atexit(coin_atexit_f* f)
{
    coin_atexit_func("cc_coin_atexit", f, CC_ATEXIT_EXTERNAL);
}

void cc_coin_atexit_static_internal(coin_atexit_f* fp)
{
    coin_atexit_func("cc_coin_atexit_static_internal", fp, CC_ATEXIT_STATIC_DATA);
}

SbBool coin_is_exiting(void)
{
    return isexiting;
}

/* ********************************************************************** */
/* File descriptor functions */

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

static FILE* coin_stdin = NULL;
static FILE* coin_stdout = NULL;
static FILE* coin_stderr = NULL;
static int coin_dup_stdin = -1;
static int coin_dup_stdout = -1;
static int coin_dup_stderr = -1;

void free_std_fds(void)
{
    if (coin_stdin) {
        assert(coin_dup_stdin != -1);
        fclose(coin_stdin);
        coin_stdin = NULL;
        dup2(coin_dup_stdin, STDIN_FILENO);
        close(coin_dup_stdin);
        coin_dup_stdin = -1;
    }
    if (coin_stdout) {
        assert(coin_dup_stdout != -1);
        fclose(coin_stdout);
        coin_stdout = NULL;
        dup2(coin_dup_stdout, STDOUT_FILENO);
        close(coin_dup_stdout);
        coin_dup_stdout = -1;
    }
    if (coin_stderr) {
        assert(coin_dup_stderr != -1);
        fclose(coin_stderr);
        coin_stderr = NULL;
        dup2(coin_dup_stderr, STDERR_FILENO);
        close(coin_dup_stderr);
        coin_dup_stderr = -1;
    }
}

FILE* coin_get_stdin(void)
{
    if (!coin_stdin) {
        coin_dup_stdin = dup(STDIN_FILENO);
        coin_stdin = fdopen(STDIN_FILENO, "r");
    }
    return coin_stdin;
}

FILE* coin_get_stdout(void)
{
    if (!coin_stdout) {
        coin_dup_stdout = dup(STDOUT_FILENO);
        coin_stdout = fdopen(STDOUT_FILENO, "w");
    }
    return coin_stdout;
}

FILE* coin_get_stderr(void)
{
    if (!coin_stderr) {
        coin_dup_stderr = dup(STDERR_FILENO);
        coin_stderr = fdopen(STDERR_FILENO, "w");
    }
    return coin_stderr;
}

/* ********************************************************************** */
/* Locale functions */

SbBool coin_locale_set_portable(cc_string* storeold)
{
    const char* loc;
    
    const char* deflocale = setlocale(LC_NUMERIC, NULL);
    if (strcmp(deflocale, "C") == 0) {
        return COIN_FALSE;
    }
    
    cc_string_construct(storeold);
    cc_string_set_text(storeold, deflocale);
    
    loc = setlocale(LC_NUMERIC, "C");
    assert(loc != NULL && "could not set locale to supposed portable C locale");
    (void)loc; // suppress unused variable warning in release builds
    return COIN_TRUE;
}

void coin_locale_reset(cc_string* storedold)
{
    const char* l = setlocale(LC_NUMERIC, cc_string_get_text(storedold));
    assert(l != NULL && "could not reset locale");
    (void)l; // suppress unused variable warning in release builds
    cc_string_clean(storedold);
}

double coin_atof(const char* ptr)
{
    double v;
    cc_string storedlocale;
    SbBool changed = coin_locale_set_portable(&storedlocale);
    v = atof(ptr);
    if (changed) {
        coin_locale_reset(&storedlocale);
    }
    return v;
}

/* ********************************************************************** */
/* ASCII85 functions */

static int coin_encode_ascii85(const unsigned char* in, unsigned char* out)
{
    uint32_t data =
        ((uint32_t)(in[0])<<24) |
        ((uint32_t)(in[1])<<16) |
        ((uint32_t)(in[2])<< 8) |
        ((uint32_t)(in[3]));
    
    if (data == 0) {
        out[0] = 'z';
        return 1;
    }
    out[4] = (unsigned char) (data%85 + '!');
    data /= 85;
    out[3] = (unsigned char) (data%85 + '!');
    data /= 85;
    out[2] = (unsigned char) (data%85 + '!');
    data /= 85;
    out[1] = (unsigned char) (data%85 + '!');
    data /= 85;
    out[0] = (unsigned char) (data%85 + '!');
    return 5;
}

void coin_output_ascii85(FILE* fp,
                        const unsigned char val,
                        unsigned char* tuple,
                        unsigned char* linebuf,
                        int* tuplecnt, int* linecnt,
                        const int rowlen,
                        const SbBool flush)
{
    int i;
    if (flush) {
        for (i = *tuplecnt; i < 4; i++) tuple[i] = 0;
    } else {
        tuple[(*tuplecnt)++] = val;
    }
    if (flush || *tuplecnt == 4) {
        if (*tuplecnt) {
            int add = coin_encode_ascii85(tuple, linebuf + *linecnt);
            if (flush) {
                if (add == 1) {
                    for (i = 0; i < 5; i++) linebuf[*linecnt + i] = '!';
                }
                *linecnt += *tuplecnt + 1;
            } else {
                *linecnt += add;
            }
            *tuplecnt = 0;
        }
        if (*linecnt >= rowlen) {
            unsigned char store = linebuf[rowlen];
            linebuf[rowlen] = 0;
            fprintf(fp, "%s\n", linebuf);
            linebuf[rowlen] = store;
            for (i = rowlen; i < *linecnt; i++) {
                linebuf[i-rowlen] = linebuf[i];
            }
            *linecnt -= rowlen;
        }
        if (flush && *linecnt) {
            linebuf[*linecnt] = 0;
            fprintf(fp, "%s\n", linebuf);
        }
    }
}

void coin_flush_ascii85(FILE* fp,
                       unsigned char* tuple,
                       unsigned char* linebuf,
                       int* tuplecnt, int* linecnt,
                       const int rowlen)
{
    coin_output_ascii85(fp, 0, tuple, linebuf, tuplecnt, linecnt, rowlen, COIN_TRUE);
}

/* ********************************************************************** */
/* Version parsing */

SbBool coin_parse_versionstring(const char* versionstr,
                               int* major,
                               int* minor,
                               int* patch)
{
    char buffer[256];
    char* dotptr;
    
    *major = 0;
    if (minor) *minor = 0;
    if (patch) *patch = 0;
    if (versionstr == NULL) return COIN_FALSE;
    
    (void)strncpy(buffer, versionstr, 255);
    buffer[255] = '\0';
    dotptr = strchr(buffer, '.');
    if (dotptr) {
        char* spaceptr;
        char* start = buffer;
        *dotptr = '\0';
        *major = atoi(start);
        if (minor == NULL) return COIN_TRUE;
        start = ++dotptr;
        
        dotptr = strchr(start, '.');
        spaceptr = strchr(start, ' ');
        if (!dotptr && spaceptr) dotptr = spaceptr;
        if (dotptr && spaceptr && spaceptr < dotptr) dotptr = spaceptr;
        if (dotptr) {
            int terminate = *dotptr == ' ';
            *dotptr = '\0';
            *minor = atoi(start);
            if (patch == NULL) return COIN_TRUE;
            if (!terminate) {
                start = ++dotptr;
                dotptr = strchr(start, ' ');
                if (dotptr) *dotptr = '\0';
                *patch = atoi(start);
            }
        } else {
            *minor = atoi(start);
        }
    } else {
        cc_debugerror_post("coin_parse_versionstring",
                          "Invalid versionstring: \"%s\"\n", versionstr);
        return COIN_FALSE;
    }
    return COIN_TRUE;
}

/* ********************************************************************** */
/* File system functions */

#ifdef HAVE__GETCWD
#define getcwd_wrapper(buf, size) _getcwd(buf, (int)size)
#elif defined(HAVE_GETCWD)
#define getcwd_wrapper(buf, size) getcwd(buf, size)
#else
#define getcwd_wrapper(buf, size) NULL
#endif

SbBool coin_getcwd(cc_string* str)
{
    char buf[256], * dynbuf = NULL;
    size_t bufsize = sizeof(buf);
    char* cwd = getcwd_wrapper(buf, bufsize);
    
    while ((cwd == NULL) && (errno == ERANGE)) {
        bufsize *= 2;
        free(dynbuf);
        dynbuf = (char*)malloc(bufsize);
        cwd = getcwd_wrapper(dynbuf, bufsize);
    }
    if (cwd == NULL) {
        cc_string_set_text(str, strerror(errno));
    } else {
        cc_string_set_text(str, cwd);
    }
    
    free(dynbuf);
    return cwd ? COIN_TRUE : COIN_FALSE;
}

/* ********************************************************************** */
/* Math utility functions */

int coin_finite(double value)
{
#ifdef HAVE_FINITE
    return finite(value);
#elif defined(HAVE__FINITE)
    return _finite(value);
#else
    return !coin_isinf(value) && !coin_isnan(value);
#endif
}

/* ********************************************************************** */
/* Prime number functions */

static const unsigned long coin_prime_table[32] = {
    2,
    5,
    11,
    17,
    37,
    67,
    131,
    257,
    521,
    1031,
    2053,
    4099,
    8209,
    16411,
    32771,
    65537,
    131101,
    262147,
    524309,
    1048583,
    2097169,
    4194319,
    8388617,
    16777259,
    33554467,
    67108879,
    134217757,
    268435459,
    536870923,
    1073741827,
    2147483659U,
    4294967291U
};

unsigned long coin_geq_prime_number(unsigned long num)
{
    int i;
    for (i = 0; i < 32; i++) {
        if (coin_prime_table[i] >= num) {
            return coin_prime_table[i];
        }
    }
    return num;
}

/* ********************************************************************** */
/* OS detection */

int coin_runtime_os(void)
{
#if defined(__APPLE__)
    return COIN_OS_X;
#elif defined(HAVE_WIN32_API)
    return COIN_MSWINDOWS;
#else
    return COIN_UNIX;
#endif
}

/* ********************************************************************** */
/* Debug functions */

int coin_debug_extra(void)
{
#if COIN_DEBUG
    return COIN_DEBUG_EXTRA;
#else
    return 0;
#endif
}

int coin_debug_normalize(void)
{
#if COIN_DEBUG
    return COIN_DEBUG_NORMALIZE;
#else
    return 0;
#endif
}

int coin_debug_caching_level(void)
{
#if COIN_DEBUG
    static int COIN_DEBUG_CACHING = -1;
    if (COIN_DEBUG_CACHING < 0) {
        const char* env = CoinInternal::getEnvironmentVariableRaw("COIN_DEBUG_CACHING");
        if (env) COIN_DEBUG_CACHING = atoi(env);
        else COIN_DEBUG_CACHING = 0;
    }
    return COIN_DEBUG_CACHING;
#else
    return 0;
#endif
}

/* ********************************************************************** */

#ifdef __cplusplus
} /* extern "C" */
#endif
