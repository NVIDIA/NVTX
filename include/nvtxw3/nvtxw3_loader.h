/*
 *  SPDX-FileCopyrightText: Copyright (c) 2023-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *  SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  Licensed under the Apache License v2.0 with LLVM Exceptions.
 *  See https://nvidia.github.io/NVTX/LICENSE.txt for license information.
 */

/** \file nvtxw3_loader.h
 * \brief Reference loader and config utilities for the NVTX Writer API.
 *
 * The functions declared here (see src/nvtxw3_loader.c) are a reference
 * implementation for locating and loading an NVTXW backend library. They are
 * optional: an application may instead load a backend library by any mechanism
 * it prefers, resolve the backend's exported "nvtxwGetInterface" symbol, and
 * request the desired nvtxwInterface_* function table. The producer/backend
 * contract lives in nvtxw3.h.
 */

#if !defined(NVTXW_LOADER_API)
#define NVTXW_LOADER_API

#include <nvtxw3/nvtxw3.h>

#include <string.h> /* For nvtxwConsumeConfigString inline implementation */

#define NVTXW_DECLSPEC extern

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(_WIN32)
#define NVTXW_LIB_PREFIX ""
#define NVTXW_LIB_SUFFIX ".dll"
#else
#define NVTXW_LIB_PREFIX "lib"
#if defined(__APPLE__)
#define NVTXW_LIB_SUFFIX ".dylib"
#else
#define NVTXW_LIB_SUFFIX ".so"
#endif
#endif

/**
 * \brief Default backend library filename used by \ref nvtxwLoad's default search.
 *
 * Note the platform-dependent prefix and suffix above are added here.
 */
#define NVTXW_LIB_FILENAME_DEFAULT NVTXW_LIB_PREFIX "nvtxw3" NVTXW_LIB_SUFFIX

/**
 * \brief Environment variable consulted by \ref nvtxwLoad.
 *
 * When set to a non-empty value, it is treated as the backend library filename
 * or path to load (passed verbatim to dlopen/LoadLibrary) and takes priority
 * over both the `library` argument and the built-in search locations. This lets
 * a tool or launcher redirect the backend without rebuilding the instrumented
 * application, much like NVTX injection uses `NVTX_INJECTION*_PATH`. Unset or
 * empty is ignored.
 */
#define NVTXW_LIBRARY_ENV_VAR "NVTXW3_LIBRARY"

/* Config string format (used by `nvtxwSessionAttributes_t::configString`):
 *
 * The format is key=value pairs, delimited by new-line characters or | (pipe)
 * characters. Values are prohibited from containing those characters. If an
 * entry begins with #, the entry (up to the next new-line or pipe) is discarded
 * as a comment.
 *
 * If a config specifies the same key multiple times, only the first appearance
 * should be honored, and the subsequent appearances should be ignored. This
 * allows a simple scan for a particular key to loop from the beginning until
 * the first occurrence is found, and not have to loop through the rest for
 * repeats. Note that this means building a map from keys to values should not
 * overwrite existing values if a found key already exists in the map. This
 * guarantee allows adding extra key/value pairs to a config string by
 * prepending (to override existing keys) or appending (to set values only if
 * they weren't set already). Keys are tool-specific.
 */

/*--------- Config string utilities ----------------*/

/**
 * \brief Callback function pointer used with \ref nvtxwConsumeConfigString.
 *
 * The state pointer can be used for anything -- nvtxwConsumeConfigString passes
 * it directly to the callback. The begin/end pointers for the key and value are
 * pointing to ranges within the input config string. If the input config string
 * is known to be non-const, this callback can safely cast away const and write
 * to these pointers, for example when simplifying an input config string. To
 * check if a key name is a particular string, use:
 * \code
 *     keyEnd - keyBegin == strlen("ExampleKeyName") &&
 *         strncmp("ExampleKeyName", keyBegin, keyEnd - keyBegin) == 0
 * \endcode
 * In C++, you can construct a string using std::string(keyBegin, keyEnd).
 * Return zero to continue consuming key/value pairs, or non-zero to stop.
 */
typedef int (*nvtxwKeyValuePairConsumer_t)(
    void* state,
    const char* keyBegin,
    const char* keyEnd,
    const char* valBegin,
    const char* valEnd);

/**
 * \brief Parse a config string, invoking a callback for each key/value pair.
 *
 * See \ref nvtxwKeyValuePairConsumer_t for the callback signature. The inline
 * implementation is provided here so backend implementations of NVTXW can use
 * this function without including nvtxw3_loader.c in their build. Users of the
 * NVTXW API may also find it useful to parse/modify a config before passing it
 * to NVTXW.
 */
NVTX_INLINE_STATIC
void nvtxwConsumeConfigString(const char* config, nvtxwKeyValuePairConsumer_t consumer, void* state)
{
    const char* curRead = config;
    const char* const lineBreak = "|\n\r";
    const char* const whitespace = " \t\v"; /* Not including lineBreak characters */
    int consumerStopRequested = 0;

    if (!config || !consumer) return;

    while (*curRead && !consumerStopRequested)
    {
        const char* lineBegin;
        const char* lineEnd;
        const char* keyBegin;
        const char* keyEnd;
        const char* valBegin;
        const char* valEnd;

        /* Read a line, trimming leading whitespace - get pointers to begin/end */
        lineBegin = curRead + strspn(curRead, whitespace);
        lineEnd = lineBegin + strcspn(lineBegin, lineBreak);

        /* Set read pointer to beginning of next line, so we can continue any time */
        curRead = lineEnd + strspn(lineEnd, lineBreak);

        /* Ignore line if it's only whitespace */
        if (lineBegin == lineEnd) continue;
        /* Ignore line if it's a comment */
        if (*lineBegin == '#') continue;

        /* Determine if line has a key and value delimited by '=' */
        keyBegin = lineBegin;
        keyEnd = keyBegin;
        while (keyEnd < lineEnd && *keyEnd != '=') ++keyEnd;

        /* Ignore line if there's no '=' in the line */
        if (keyEnd == lineEnd) continue;
        /* Ignore line if there's no key name before '=' */
        if (keyEnd == keyBegin) continue;

        /* keyEnd now points at '=' after the key */
        valBegin = keyEnd + 1;
        valBegin += strspn(valBegin, whitespace);

        /* Ignore line if all characters after '=' are whitespace  */
        if (valBegin == lineEnd) continue;

        valEnd = lineEnd;

        /* Got begin/end pointers for key and value. We know there are non-
        *  whitespace characters in both of them, and their leading whitespace
        *  was already trimmed. Now trim their trailing whitespace. */
        while (strchr(whitespace, *(keyEnd - 1))) --keyEnd;
        while (strchr(whitespace, *(valEnd - 1))) --valEnd;

        /* Now key and value begin/end pointers can be passed to the consumer */
        consumerStopRequested = consumer(state, keyBegin, keyEnd, valBegin, valEnd);
    }
}

/*--------- Initialization interface ---------*/

/* The nvtxwLoad/nvtxwUnload functions declared below are a reference
 * implementation (see src/nvtxw3_loader.c) for locating and loading an NVTXW
 * backend library. An application may instead load a backend library by any
 * mechanism it prefers, resolve the backend's exported "nvtxwGetInterface"
 * symbol, and request the desired nvtxwInterface_* function table. */

/** \brief Platform-specific module handle for a loaded NVTXW backend library. */
typedef void* nvtxwModuleHandle_t;

/**
 * \brief Load an NVTXW backend library and resolve its exported entry point.
 *
 * Resolving that symbol (\ref NVTXW_GET_INTERFACE_SYMBOL_NAME) is what makes a
 * library a usable backend. The loader does not call into the backend, so the
 * returned function pointer is invoked by the caller (typically via
 * \ref nvtxwLoadInterface) to request an interface table.
 *
 * Library resolution, in priority order:
 *   1. The `NVTXW3_LIBRARY` environment variable, if set to a non-empty value.
 *   2. The `library` argument, if non-NULL: a filename or path passed verbatim
 *      to dlopen/LoadLibrary (a bare filename uses the standard search paths;
 *      an absolute path is used as-is).
 *   3. If `library` is NULL: a default search for \ref NVTXW_LIB_FILENAME_DEFAULT
 *      in the current process's executable directory, the standard dynamic
 *      library search paths, then the current working directory.
 * When the environment variable or the `library` argument names a specific
 * library, that library is the only candidate: if it fails to load or lacks the
 * symbol, that failure is returned rather than falling back to other locations.
 *
 * @param library NULL to use the default search, or a backend filename/path.
 * @param getInterfaceFuncOut must not be NULL. It receives a pointer to the
 *     backend's `nvtxwGetInterface` function, used to request interface tables.
 * @param moduleHandleOut may be NULL. If non-null, it receives the
 *     platform-specific module handle of the loaded backend library when
 *     `NVTXW_RESULT_SUCCESS` is returned. This can be passed to \ref nvtxwUnload
 *     to unload the backend library.
 */
NVTXW_DECLSPEC nvtxwResultCode_t nvtxwLoad(
    const char* library,
    nvtxwGetInterface_t* getInterfaceFuncOut,
    nvtxwModuleHandle_t* moduleHandleOut);

/**
 * \brief Unload a backend library previously loaded by \ref nvtxwLoad.
 *
 * If not called, the backend stays loaded until process exit. Call it only
 * after all sessions, streams, and other backend calls have completed. If the
 * backend exports "nvtxwFinalize" (\ref NVTXW_FINALIZE_SYMBOL_NAME), that hook
 * runs before the module handle is released.
 */
NVTXW_DECLSPEC void nvtxwUnload(
    nvtxwModuleHandle_t moduleHandle);

/**
 * \brief Write a human-readable message for a result code to `messageOut`.
 *
 * The message is truncated to fit `messageOutLen` (including the null
 * terminator), and the return value is its length excluding the terminator.
 * Writes nothing and returns 0 if `messageOut` is NULL or `messageOutLen` is 0.
 * For load and symbol-missing failures, a best-effort OS detail
 * (dlerror()/GetLastError()) may be appended.
 */
NVTXW_DECLSPEC size_t nvtxwGetError(
    nvtxwResultCode_t code,
    char* messageOut,
    size_t messageOutLen);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NVTXW_LOADER_API */
