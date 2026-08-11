/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Licensed under the Apache License v2.0 with LLVM Exceptions.
 * See https://nvidia.github.io/NVTX/LICENSE.txt for license information.
 */

/* setenv/unsetenv are POSIX; request them even under the project's strict C90.
 * This must precede any system header include.  On Apple, also enable Darwin
 * extensions so NVTX's use of RTLD_DEFAULT (from <dlfcn.h>) remains visible
 * when a strict POSIX feature-test macro is set. */
#if !defined(_WIN32) && (!defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200112L)
#  undef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 200112L
#endif
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#  define _DARWIN_C_SOURCE 1
#endif

/*
 * Unit tests for the NVTXW loader (nvtxw3_loader.c): nvtxwLoad across its library
 * resolution paths (default search, explicit library, NVTXW3_LIBRARY override)
 * and error paths, nvtxwUnload, and nvtxwGetError.  The recording mock backend
 * (built as libnvtxw3 by the test build) stands in for a real backend.
 *
 * The test driver (RunTest) receives, as forwarded arguments:
 *   argv[0] = absolute path to the mock backend library file (libnvtxw3.so)
 *   argv[1] = directory containing that library (unused; kept for compatibility)
 *   argv[2] = absolute path to a library that does NOT export the backend symbol
 */

#include <nvtx3/nvToolsExtPayload.h>
#include <nvtx3/nvToolsExtCounters.h>
#include <nvtxw3/nvtxw3_helpers.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Portable environment variable set/unset for the override tests. */
static void SetEnvVar(const char* name, const char* value)
{
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

static void UnsetEnvVar(const char* name)
{
#if defined(_WIN32)
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

/* Load the backend through the setup helper, exercise the interface end-to-end,
 * and unload.  Returns NVTXW_RESULT_SUCCESS only if every step succeeds. */
static nvtxwResultCode_t LoadAndExercise(const char* library)
{
    const nvtxwInterface_t* iface = NULL;
    nvtxwModuleHandle_t module = NULL;
    nvtxwResultCode_t result;
    nvtxwSessionAttributes_t sessionAttr;
    nvtxwSessionHandle_t session = NULL;
    nvtxwStreamHandle_t stream = NULL;
    nvtxDomainHandle_t domain = NULL;
    nvtxwEventHelperSchemaIds_t schemaIds;
    nvtxwEventWriter_t writer;
    nvtxwEventAttributesUtf8_t utf8Attr;

    result = nvtxwLoadInterface(library, &iface, &module);
    if (result != NVTXW_RESULT_SUCCESS) return result;
    if (!iface || !module)
    {
        nvtxwUnload(module);
        return NVTXW_RESULT_FAILED;
    }

    nvtxwSessionAttributesInit(&sessionAttr);
    sessionAttr.name = "loader";

    memset(&utf8Attr, 0, sizeof(utf8Attr));

    result = NVTXW_RESULT_FAILED;
    if (iface->SessionBegin(&sessionAttr, &session) == NVTXW_RESULT_SUCCESS &&
        nvtxwDomainRegister(iface, session, "domain", &domain) == NVTXW_RESULT_SUCCESS &&
        nvtxwEventSchemasRegister(
            iface, domain, NVTXW_EVENT_HELPER_SCHEMA_MARK_UTF8, &schemaIds)
            == NVTXW_RESULT_SUCCESS &&
        nvtxwStreamOpen(
            iface, session, "stream", domain, NULL,
            NVTX_TIME_DOMAIN_ID_NONE, &stream) == NVTXW_RESULT_SUCCESS)
    {
        utf8Attr.message = "loader";
        nvtxwEventWriterInit(&writer, iface, stream, &schemaIds);
        if (nvtxwMarkWriteUtf8(&writer, 1, utf8Attr)
                == NVTXW_RESULT_SUCCESS &&
            iface->SessionEnd(session) == NVTXW_RESULT_SUCCESS)
        {
            result = NVTXW_RESULT_SUCCESS;
        }
    }

    nvtxwUnload(module);
    return result;
}

#ifdef __cplusplus
extern "C" {
#endif
NVTX_DYNAMIC_EXPORT
extern int RunTest(int argc, const char** argv);
#ifdef __cplusplus
}
#endif
NVTX_DYNAMIC_EXPORT
int RunTest(int argc, const char** argv)
{
    NVTX_EXPORT_UNMANGLED_FUNCTION_NAME

    const char* backendPath;
    const char* noSymbolPath;
    nvtxwGetInterface_t getInterface = NULL;
    nvtxwModuleHandle_t module = NULL;
    nvtxwResultCode_t result;
    char message[256];
    size_t len;

    if (argc < 3) return 1; /* test harness must forward the three paths */
    backendPath = argv[0];
    /* argv[1] (backend directory) is no longer needed by the loader contract. */
    noSymbolPath = argv[2];

    /* Ensure no stale override leaks in from the surrounding environment. */
    UnsetEnvVar(NVTXW_LIBRARY_ENV_VAR);

    /*--- Argument validation ---*/

    if (nvtxwLoad(backendPath, NULL, NULL) != NVTXW_RESULT_INVALID_ARGUMENT)
        return 10;

    /*--- Explicit library path ---*/

    if (LoadAndExercise(backendPath) != NVTXW_RESULT_SUCCESS) return 20;

    /* A path that cannot be opened reports a load failure (the loader captures
     * the dlopen/LoadLibrary error detail for these codes). */
    getInterface = NULL;
    module = NULL;
    result = nvtxwLoad(
        "this-library-does-not-exist.so", &getInterface, &module);
    if (result != NVTXW_RESULT_LIBRARY_LOAD_FAILED) return 21;

    /* A library that loads but lacks the backend entry point. */
    getInterface = NULL;
    module = NULL;
    result = nvtxwLoad(noSymbolPath, &getInterface, &module);
    if (result != NVTXW_RESULT_LIBRARY_SYMBOL_MISSING) return 22;

    /*--- Default search (the mock backend is co-located with the executable) ---*/

    if (LoadAndExercise(NULL) != NVTXW_RESULT_SUCCESS) return 23;

    /*--- Interface ID negotiation through the resolved entry point ---*/

    /* The loader returns the backend's single entry point; the caller invokes
     * it to request an interface table.  A supported ID yields a table; an
     * unsupported ID is reported without disturbing the loaded module. */
    getInterface = NULL;
    module = NULL;
    result = nvtxwLoad(backendPath, &getInterface, &module);
    if (result != NVTXW_RESULT_SUCCESS || !getInterface) return 30;
    {
        const void* iface = NULL;
        if (getInterface(NVTXW_INTERFACE_VERSION, &iface) != NVTXW_RESULT_SUCCESS ||
            !iface)
        {
            nvtxwUnload(module);
            return 31;
        }
        iface = NULL;
        if (getInterface(0x7fffffff, &iface) != NVTXW_RESULT_INTERFACE_VERSION_NOT_SUPPORTED ||
            iface)
        {
            nvtxwUnload(module);
            return 32;
        }
    }
    nvtxwUnload(module);

    /*--- NVTXW3_LIBRARY environment override ---*/

    /* An override fills the default (NULL) resolution path. */
    SetEnvVar(NVTXW_LIBRARY_ENV_VAR, backendPath);
    if (LoadAndExercise(NULL) != NVTXW_RESULT_SUCCESS)
    {
        UnsetEnvVar(NVTXW_LIBRARY_ENV_VAR);
        return 40;
    }

    /* The override is authoritative: it wins even over an explicit (here bogus)
     * library argument. */
    if (LoadAndExercise("this-library-does-not-exist.so") != NVTXW_RESULT_SUCCESS)
    {
        UnsetEnvVar(NVTXW_LIBRARY_ENV_VAR);
        return 41;
    }

    /* A failing override does not fall back to the default search. */
    SetEnvVar(NVTXW_LIBRARY_ENV_VAR, "this-library-does-not-exist.so");
    getInterface = NULL;
    module = NULL;
    result = nvtxwLoad(NULL, &getInterface, &module);
    if (result != NVTXW_RESULT_LIBRARY_LOAD_FAILED)
    {
        UnsetEnvVar(NVTXW_LIBRARY_ENV_VAR);
        return 42;
    }

    /* An empty override is ignored, restoring the default search. */
    SetEnvVar(NVTXW_LIBRARY_ENV_VAR, "");
    if (LoadAndExercise(NULL) != NVTXW_RESULT_SUCCESS)
    {
        UnsetEnvVar(NVTXW_LIBRARY_ENV_VAR);
        return 43;
    }

    UnsetEnvVar(NVTXW_LIBRARY_ENV_VAR);

    /*--- nvtxwUnload tolerates NULL ---*/

    nvtxwUnload(NULL);

    /*--- nvtxwGetError ---*/

    len = nvtxwGetError(NVTXW_RESULT_SUCCESS, message, sizeof(message));
    if (len == 0 || strcmp(message, "success") != 0) return 80;

    len = nvtxwGetError(NVTXW_RESULT_INVALID_ARGUMENT, message, sizeof(message));
    if (len == 0 || strcmp(message, "invalid argument") != 0) return 81;

    len = nvtxwGetError(NVTXW_RESULT_LIBRARY_NOT_FOUND, message, sizeof(message));
    if (len == 0 || strcmp(message, "library not found") != 0) return 82;

    /* An unknown code must still produce a non-empty, terminated message. */
    len = nvtxwGetError(12345, message, sizeof(message));
    if (len == 0 || message[0] == '\0') return 83;

    /* No output when there is no room. */
    if (nvtxwGetError(NVTXW_RESULT_SUCCESS, message, 0) != 0) return 84;
    if (nvtxwGetError(NVTXW_RESULT_SUCCESS, NULL, sizeof(message)) != 0) return 85;

    /* Truncation: a one-byte buffer must hold only the terminator. */
    message[0] = 'X';
    nvtxwGetError(NVTXW_RESULT_INVALID_ARGUMENT, message, 1);
    if (message[0] != '\0') return 86;

    return 0;
}
