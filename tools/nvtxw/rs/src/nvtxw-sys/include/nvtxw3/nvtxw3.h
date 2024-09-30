/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 * See LICENSE.txt for license information.
 */

#if !defined(NVTXW3_API)
#define NVTXW3_API

#include <nvtx3/nvToolsExtPayload.h>

#include <string.h> /* For nvtxwConsumeConfigString inline implementation */

#ifdef __cplusplus
#define NVTXW3_DECLSPEC extern "C"
#else
#define NVTXW3_DECLSPEC extern
#endif

typedef int32_t nvtxwResultCode_t;

#define NVTXW3_RESULT_SUCCESS                     0
#define NVTXW3_RESULT_FAILED                      1
#define NVTXW3_RESULT_INVALID_ARGUMENT            2
#define NVTXW3_RESULT_INVALID_INIT_MODE           3
#define NVTXW3_RESULT_LIBRARY_NOT_FOUND           4
#define NVTXW3_RESULT_CONFIG_NOT_FOUND            5
#define NVTXW3_RESULT_LOADER_SYMBOL_MISSING       6
#define NVTXW3_RESULT_LOADER_FAILED               7
#define NVTXW3_RESULT_INTERFACE_ID_NOT_SUPPORTED  8
#define NVTXW3_RESULT_CONFIG_MISSING_LOADER_INFO  9
#define NVTXW3_RESULT_UNSUPPORTED_LOADER_MODE    10
#define NVTXW3_RESULT_ENV_VAR_NOT_FOUND          11


#if defined(_WIN32)
#define NVTXW3_LIB_PREFIX ""
#define NVTXW3_LIB_SUFFIX ".dll"
#else
#define NVTXW3_LIB_PREFIX "lib"
#if defined(__APPLE__)
#define NVTXW3_LIB_SUFFIX ".dylib"
#else
#define NVTXW3_LIB_SUFFIX ".so"
#endif
#endif

/* Name of backend library file to use with init mode LIBRARY_DIRECTORY.
*  Note the platform-dependent prefix and suffix above are added here. */
#define NVTXW3_LIB_FILENAME_DEFAULT NVTXW3_LIB_PREFIX "nvtxw3" NVTXW3_LIB_SUFFIX

/* Name of config library file to use with init mode CONFIG_DIRECTORY.
*  Note the platform-dependent prefix and suffix above are added here. */
#define NVTXW3_CONFIG_FILENAME_DEFAULT "nvtxw3.ini"

/* Init modes:  nvtxwInitialize takes nvtxwInitMode_t mode, one of the #defines
*  below, and a modeString, whose meaning is dependent on the mode.  These modes
*  provide a variety of ways to find the NVTXW backend implementation library. */
typedef int32_t nvtxwInitMode_t;

/* Default search mode is to look for library with default filename, as defined
*  by NVTXW3_LIB_FILENAME_DEFAULT, in the following order:
*    1. Directory of current process's executable
*    2. Standard search paths for dynamic libraries
*    3. Current working directory (may not be included in standard search paths)
*  The modeString argument is ignored. */
#define NVTXW3_INIT_MODE_SEARCH_DEFAULT           0

/* The modeString argument is interpreted as a filename or pathname to the
*  backend library.  The string is passed directly to the platform function
*  for loading dynamic libraries (dlopen/LoadLibrary), so that function's
*  behavior will apply.  In general, a filename with no path will try the
*  standard search paths, and an absolute path will be used verbatim. */
#define NVTXW3_INIT_MODE_LIBRARY_FILENAME         1

/* The modeString argument is interpreted as a directory in which to search
*  for the backend library, whose filename is defined by the macro
*  NVTXW3_LIB_FILENAME_DEFAULT. */
#define NVTXW3_INIT_MODE_LIBRARY_DIRECTORY        2

/* The modeString argument is interpreted as a filename or pathname to a
*  config file, which will be used to find the backend library.  If the
*  filename is not an absolute path, it will be interpreted as relative
*  to the current working direcrtory.  See below for config file format. */
#define NVTXW3_INIT_MODE_CONFIG_FILENAME          3

/* The modeString argument is interpreted as a directory in which to search
*  for a config file, which will be used to find the backend library.  The
*  name of the config file is defined by NVTXW3_CONFIG_FILENAME_DEFAULT.
   See below for config file format. */
#define NVTXW3_INIT_MODE_CONFIG_DIRECTORY         4

/* The modeString argument is interpreted as the config string itself.
*  See below for config string format. */
#define NVTXW3_INIT_MODE_CONFIG_STRING            5

/* The modeString argument is interpreted as the name of an environment
*  variable that contains the config string.  See below for config string
*  format. */
#define NVTXW3_INIT_MODE_CONFIG_ENV_VAR           6

/* Config format (for both files and flat config strings):
*
*  The format is key=value pairs, delimited by new-line characters or
*  | (pipe) characters.  Values are prohibited from containing those
*  characters.  If an entry begins with #, the entry (up to the next
*  new-line or pipe) is discarded as a comment.
*
*  When the config string is provided to the SessionBegin function
*  as an argument, it is preprocessed to remove comments, blank lines,
*  and to convert all entry delimiters to a single \n (line feed).
*  This allows the tool to have a simpler config parser, and to print
*  the config in a readable format.
*
*  If a config specifies the same key multiple times, only the first
*  appearance should be honored, and the subsequent appearances should
*  be ignored.  This allows a simple scan for a particular key to loop
*  from the beginning until the first occurrence is found, and not have
*  to loop through the rest for repeats.  Note that this means building
*  a map from keys to values should not overwrite existing values if a
*  found key already exists in the map.  This guarantee allows adding
*  extra key/value pairs to a config string by prepending (to override
*  existing keys) or appending (to set values only if they weren't set
*  already).
*
*  Keys are tool-specific, but the loader supports two keys:
*
*  - InitMode=n
*      Just like the argument to nvtxwInitialize, this allows the user
*      to specify how to find the backend library, using one of the
*      numeric values of the NVTXW3_INIT_MODE_ constants.  Currently,
*      only values 0-2 are supported for init modes specified within
*      a config file/string.
*
*  - InitModeString=string
*      Just like the argument to nvtxwInitialize, this allows the user
*      to specify a mode-specific string for how to find the backend
*      library.  This key is ignored for mode 0 (SEARCH_DEFAULT), but
*      required for other modes.  Currently, only mode values 0-2 are
*      supported for init modes specified within a config file/string.
*/

/*--------- Helpers for consuming config strings ----------------*/

/* Typedef of function pointer for callback to use with nvtxwConsumeConfigString.
*  The state pointer can be used for anything -- nvtxwConsumeConfigString passes
*  it directly to the callback.  The begin/end pointers for the key and value are
*  pointing to ranges within the input config string.  If the input config string
*  is known to be non-const, this callback can safely cast away const and write
*  to these pointers, for example when simplifying an input config string.  To
*  check if a key name is a particular string, use:
*     strncmp("ExampleKeyName", keyBegin, keyEnd - keyBegin) == 0
*  In C++, you can construct a string using std::string(keyBegin, keyEnd).
*  Return zero to continue consuming key/value pairs, or non-zero to stop. */
typedef int (*nvtxwKeyValuePairConsumer_t)(
    void* state,
    const char* keyBegin,
    const char* keyEnd,
    const char* valBegin,
    const char* valEnd);

/* Parse config and call the consumer callback (see typedef above) on each
*  valid key/value pair found in the config.  Inline implementation provided
*  here so backend implementations of NVTXW can use this function without
*  having to include nvtxw3.c in their build.  Users of the NVTXW API may
*  also find it useful to parse/modify a config before passing it to NVTXW. */
NVTX_LINKONCE_DEFINE_FUNCTION
void nvtxwConsumeConfigString(const char* config, nvtxwKeyValuePairConsumer_t consumer, void* state)
{
    const char* curRead = config;
    static const char lineBreak[] = "|\n\r";
    static const char whitespace[] = " \t\v"; /* Not including lineBreak characters */
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
        /* Ignore line if it's is a comment */
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

        /* Got begin/end pointers for key and value.  We know there are non-whitespace
        *  characters in both of them, and their leading whitespace was already trimmed.
        *  Now trim their trailing whitespace. */
        while (strchr(whitespace, *(keyEnd - 1))) --keyEnd;
        while (strchr(whitespace, *(valEnd - 1))) --valEnd;

        /* Now key and value begin/end pointers can be passed to the consumer */
        consumerStopRequested = consumer(state, keyBegin, keyEnd, valBegin, valEnd);
    }
}

/*--------- Initialization interface ---------*/

typedef int32_t nvtxwInterfaceId_t;

typedef nvtxwResultCode_t (*nvtxwGetInterface_t)(
    nvtxwInterfaceId_t interfaceId,
    const void** iface);

/* Initialize the NVTXW library by providing information on how to
*  load the backend library that implements the NVTXW API.  `mode` must
*  be one of the NVTXW3_INIT_MODE_ constants.  `modeString` is required
*  for all modes besides 0 (SEARCH_DEFAULT), and has mode-specific
*  interpretation.  See comments for the mode constants.  Backend library
*  must provide an exported function symbol "nvtxwLoadImplementation",
*  which must return NVTXW3_RESULT_SUCCESS and provide a pointer to its
*  GetInterface function for initialization to be considered successful.
*  Modes that search multiple locations will continue searching after an
*  unsuccessful attempt to initialize a library.
*  `getInterfaceFunc` is an out-param that must be non-null to receive
*  a pointer to the backend's GetInterface function, which is used to
*  make version-safe calls into the backend library.
*  `moduleHandle` is an out-param that can be null.  If non-null, it
*  receives the platform-specific module handle of the loaded backend
*  library when NVTXW3_RESULT_SUCCESS is returned.  This can be passed
*  to nvtxwUnload to unload the backend library. */
NVTXW3_DECLSPEC nvtxwResultCode_t nvtxwInitialize(
    nvtxwInitMode_t mode,
    const char* modeString,
    nvtxwGetInterface_t* getInterfaceFunc,
    void** moduleHandle);

/* A backend library may optionally provide an exported function symbol
*  "nvtxwUnloadImplementation".  If it does, nvtxwUnload will call this
*  function before closing the module handle.  This gives the backend a
*  chance to free any memory tracked in global variables before it gets
*  unloaded.  Attempting to unload the backend is not necessary and not
*  even recommended in common cases -- it is included to ensure clients
*  of the NVTXW API have a way to cleanly pass a memory checker. */
NVTXW3_DECLSPEC void nvtxwUnload(
    void* moduleHandle);

/*----- Typedefs for function pointers backend implements -----*/

typedef nvtxwResultCode_t (*nvtxwLoadImplementation_t)(
    const char* configString,
    nvtxwGetInterface_t* getInterfaceFunc);

typedef void (*nvtxwUnloadImplementation_t)();

/*--------- Interface IDs ----------------*/

#define NVTXW3_INTERFACE_ID_CORE_V1      2

/*--------- INTERFACE_ID_CORE_V1 ---------*/

typedef struct nvtxwSessionHandle_t
{
    void* opaque;
} nvtxwSessionHandle_t;

typedef struct nvtxwStreamHandle_t
{
    void* opaque;
} nvtxwStreamHandle_t;

/* Growable struct of arguments for SessionBegin */
typedef struct nvtxwSessionAttributes_v1
{
    /* Guaranteed to increase when new members are added at the end */
    size_t struct_size;

    /* Provide a name for the session.
    *  Tools may display this name, or use it to name a file or directory
    *  representing the session. */
    const char* name;

    /* String containing configuration options for the session.
    *  Format is key=value, one per line, delimited by \n (line feed).
    *  Key names must not contain an = (equals sign), and values may
    *  contain any character except \r (carriage return), \n (line feed),
    *  or | (pipe).  Tools shall use reasonable defaults for any config
    *  options not provided, and ignore any keys they do not support.
    *  See above for explanation of how config strings are provided.
    *  See tool-specific documentation for lists of supported keys. */
    const char* configString;
} nvtxwSessionAttributes_t;

/* Define whether event ordering in a stream is based on event scope */

/* Event ordering is defined at the stream level, independent of
*  event scopes within the stream. */
#define NVTXW3_STREAM_ORDER_INTERLEAVING_NONE          (int16_t)0

/* Event ordering is defined at the event scope level.  This means
*  ordering guarantees described by the other fields only apply to
*  events of the same scope within the stream.  The order of events
*  in different scopes is unspecified. */
#define NVTXW3_STREAM_ORDER_INTERLEAVING_EVENT_SCOPE   (int16_t)1


/* Define how events are fully or partially sorted in a stream. */

/* No guarantees can be made about event ordering in the stream.
*  Events may need to be sorted by the tool. */
#define NVTXW3_STREAM_ORDERING_TYPE_UNKNOWN            (int16_t)0

/* All events represent single points in time and are fully or
*  partially sorted in the order in which they occurred. */
#define NVTXW3_STREAM_ORDERING_TYPE_STRICT             (int16_t)1

/* Events that represent single points in time are fully or
*  partially sorted in the order in which they occurred, and
*  events representing time ranges in order of begin time. */
#define NVTXW3_STREAM_ORDERING_TYPE_PACKED_RANGE_START (int16_t)2

/* Events that represent single points in time are fully or
*  partially sorted in the order in which they occurred, and
*  events representing time ranges in order of end time. */
#define NVTXW3_STREAM_ORDERING_TYPE_PACKED_RANGE_END   (int16_t)3

/* Define how to quantify skid when events are partially sorted.  Only considered
*  when orderingType is not UNKNOWN.  Which events in the stream this applies to
*  depends on the value of orderInterleaving.  Which timestamp is used for ordering
*  in an event with multiple timestamps depends on the value of orderingType. */

/* Events are fully sorted. */
#define NVTXW3_STREAM_ORDERING_SKID_NONE          0

/* Events are partially sorted.  The orderingSkidAmount field defines "skid" as
*  a number of nanoseconds.  For any two events A and B in the stream or scope
*  (depending on interleaving level), where A is written into the stream before
*  B, the tool must handle the case where B has a lower timestamp than A, but
*  can assume B's timestamp cannot be more than the "skid" number of nanoseconds
*  earlier than A's timestamp.  Note that timestamp values in events cannot be
*  assumed to be in units of nanoseconds, so this value cannot be added directly
*  timestamp values without conversion. */
#define NVTXW3_STREAM_ORDERING_SKID_TIME_NS       1

/* Events are partially sorted.  The orderingSkidAmount field defines "skid" as
*  a number of events.  Regarding only events in a stream or scope (depending on
*  interleaving level), for any event A, the next "skid" number of events after
*  A may have a lower timestamp than A (by any amount of time), but no events
*  written after that can have a lower timestamp than A. */

/* Events are partially sorted.  No event in the stream is written
*  more than the given number of events before any event written
*  previously in the stream.  Note that
*  timestamps in events may not be in units of nanoseconds. */
#define NVTXW3_STREAM_ORDERING_SKID_EVENT_COUNT   2

/* Growable struct of arguments for StreamOpen */
typedef struct nvtxwStreamAttributes_v1
{
    /* Guaranteed to increase when new members are added at the end */
    size_t struct_size;

    /* Name of a stream, used for identification from other streams.
    *  Tools typically will not display stream names.  No two streams
    *  in the same session may have the same name. */
    const char* name;

    /* Name of NVTX domain to use implicitly for all events written into
    *  this stream.  Since registered IDs are required to be unique within
    *  a domain, all ID registration functions called on this stream must
    *  not register the same ID value to mean different things.  Multiple
    *  streams may use the same domain by specifying the same value for
    *  this string, and the tool is expected to combine registrations from
    *  these streams into a single set of registrations for the domain.
    *  If two streams share a domain, and a registration is made in one
    *  stream, the registered ID may be used immediately afterwards in the
    *  other stream, provided the usage occurs on the same thread -- it is
    *  implementation-defined whether or not this is supported if the usage
    *  occurs on a different thread.  Tools are expected to combine data
    *  from any domains registered with the same name, even between NVTXW
    *  and NVTX, when merging data acquired from both APIs. */
    const char* nvtxDomainName;

    /* The default scope for all events in the stream that don't specify
    *  any scope.  See comments below for nvtxwEventScopeAttributes_t.
    *  Note that "nvtxwStream" without brackets may not be used as a node
    *  name here -- this field is defining what that node name will mean
    *  in scope registrations occurring later in this stream.  However,
    *  "nvtxwStream[name]" referencing a different stream by its name
    *  (see above) to use its default scope is supported, as long as that
    *  stream was successfully opened (and may be already closed). */
    const char* eventScopePath;

    /* Information about event ordering inside the stream.  See comments
    *  for #defines above. */
    int16_t orderInterleaving;  /* NVTXW3_STREAM_ORDER_INTERLEAVING_*    */
    int16_t orderingType;       /* NVTXW3_STREAM_ORDERING_TYPE_*         */
    int32_t orderingSkid;       /* NVTXW3_STREAM_ORDERING_SKID_*         */
    int64_t orderingSkidAmount; /* Numeric value, dependent on skid type */
} nvtxwStreamAttributes_t;

/* Growable struct of arguments for EventScopeRegister */
typedef struct nvtxwEventScopeAttributes_v1
{
    /* Guaranteed to increase when new members are added at the end */
    size_t struct_size;

    /* Path delimited by / characters, relative to hierarchy root.
    *  Nodes in the path may use name[key] syntax to indicate an
    *  array of sibling nodes, which may be combined with other
    *  non-array nodes or different arrays at the same scope.
    *  Leading slashes are ignored.  Node names should be ASCII
    *  printable characters, excluding the /, [, and ] characters,
    *  which have special meaning here.  A set of reserved node
    *  names with special properties is given in the documentation
    *  for NVTX Deferred Events.  "nvtxwStream" is a reserved node
    *  name that can be used as a path's root node, indicating the
    *  path is relative to the eventScopePath set for the stream
    *  in which the event scope is registered.  "nvtxwStream[name]"
    *  refers to the eventScopePath of a stream in the session with
    *  matching name.  Note that the NVTX domain is implicitly a
    *  child node of the scope, since multiple domains can assign
    *  events to the same scope, and tools should isolate events
    *  from separate domains. */
    const char* path;

    /* Static event scope ID must be provided, unique within the domain,
       >= NVTX_EVENT_SCOPE_ID_STATIC_START, and
       <  NVTX_EVENT_SCOPE_ID_DYNAMIC_START */
    uint64_t scopeId;
} nvtxwEventScopeAttributes_t;

/* nvtxwInterfaceCore_t is a growable struct of function pointers to
*  the NVTX Writer (NVTXW) API.  Breaking changes will not be made to
*  this interface without also changing the interface ID passed to
*  nvtxwGetInterface_t, e.g. NVTXW3_INTERFACE_ID_CORE_V1.  Non-breaking
*  are made by adding fields to the end of the struct, ensuring the
*  value of 'struct_size' increases, so the presence of a member can
*  be checked by comparing struct_size with that member's offset. */
typedef struct nvtxwInterfaceCore_v1
{
    /* Guaranteed to increase when new members are added at the end */
    size_t struct_size;

    /* Create a session, which represents a collection of trace data
    *  from one or more streams.  Takes a growable struct of session
    *  attributes (see nvtxwSessionAttributes_t). */
    nvtxwResultCode_t (*SessionBegin)(
        nvtxwSessionHandle_t* session,
        const nvtxwSessionAttributes_t* attr);

    /* Notify the implementation that all trace data for the session
    *  has been provided, and the session may be destroyed.  Depending
    *  on configuration options, ending a session may trigger behavior
    *  like writing an output file or opening a data viewer. */
    nvtxwResultCode_t (*SessionEnd)(
        nvtxwSessionHandle_t session);

    /* Create a stream within a session.  A stream is the object events
    *  are written to.  The NVTX domain and event scope are set when
    *  creating a stream, allowing individual events to avoid repeating
    *  these fields.  Since ID values for schemas, registered strings,
    *  etc. are only unique within a domain, all registrations that
    *  assign an ID are done within a stream, since the domain is fixed
    *  inside a stream.  Other stream properties set at creation time
    *  are a name string, and information about the way events in the
    *  stream are ordered. */
    nvtxwResultCode_t (*StreamOpen)(
        nvtxwStreamHandle_t* stream,
        nvtxwSessionHandle_t session,
        const nvtxwStreamAttributes_t* attr);

    /* Destroy the stream object.  This is not expected to trigger a
    *  reaction in the implementation that no more events are coming;
    *  only ending a session is intended to have that effect. */
    nvtxwResultCode_t (*StreamClose)(
        nvtxwStreamHandle_t stream);

    /* Register a scope ID to represent a scope path, so the ID can be
    *  used in events or schemas to efficiently indicate a scope.
    *  Static event scope ID must be provided, unique within the domain,
    *  >= NVTX_EVENT_SCOPE_ID_STATIC_START, and
    *  <  NVTX_EVENT_SCOPE_ID_DYNAMIC_START */
    nvtxwResultCode_t (*EventScopeRegister)(
        nvtxwStreamHandle_t stream,
        const nvtxwEventScopeAttributes_t* attr);

    /* Register a schema ID to represent a schema, which describes the
    *  binary layout of a payload.
    *  Static schema ID must be provided, unique within the domain,
    *  >= NVTX_PAYLOAD_ENTRY_TYPE_SCHEMA_ID_STATIC_START, and
    *  <  NVTX_PAYLOAD_ENTRY_TYPE_SCHEMA_ID_DYNAMIC_START */
    nvtxwResultCode_t (*SchemaRegister)(
        nvtxwStreamHandle_t stream,
        const nvtxPayloadSchemaAttr_t* attr);

    /* Register a schema ID to represent an enum type, including the
    *  mapping between its values and their name strings.
    *  Static schema ID must be provided, unique within the domain,
       >= NVTX_PAYLOAD_ENTRY_TYPE_SCHEMA_ID_STATIC_START, and
       <  NVTX_PAYLOAD_ENTRY_TYPE_SCHEMA_ID_DYNAMIC_START */
    nvtxwResultCode_t (*EnumRegister)(
        nvtxwStreamHandle_t stream,
        const nvtxPayloadEnumAttr_t* attr);

    /* Write a batch of payloads into the stream representing one or more
    *  events.  A logical event with multiple payloads cannot be broken up
    *  across multiple calls to EventWrite.  The schema definitions for
    *  the payloads dictate how they are interpreted as events. */
    nvtxwResultCode_t (*EventWrite)(
        nvtxwStreamHandle_t stream,
        const nvtxPayloadData_t* payloads,
        size_t payloadCount);

} nvtxwInterfaceCore_t;

#endif
