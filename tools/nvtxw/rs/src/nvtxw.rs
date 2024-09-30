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

use std::ffi::c_void;
use std::ffi::{CString, OsStr, OsString};
use std::fs::{File, OpenOptions};
use std::io;
use std::mem::size_of;
use std::path::{Path, PathBuf};
use std::ptr::{null, null_mut};

use crate::nvtxw_bindings;

pub use nvtxw_bindings::nvtxPayloadData_t as PayloadData;
pub use nvtxw_bindings::nvtxPayloadEntryTypeInfo_t as PayloadEntryTypeInfo;
pub use nvtxw_bindings::nvtxPayloadEnumAttr_t as PayloadEnumAttr;
pub use nvtxw_bindings::nvtxPayloadEnum_t as PayloadEnum;
pub use nvtxw_bindings::nvtxPayloadSchemaAttr_t as PayloadSchemaAttr;
pub use nvtxw_bindings::nvtxPayloadSchemaEntry_t as PayloadSchemaEntry;
pub use nvtxw_bindings::nvtxScopeAttr_t as ScopeAttr;
pub use nvtxw_bindings::nvtxSemanticsHeader_t as SemanticsHeader;

pub use nvtxw_bindings::nvtxwEventScopeAttributes_t as EventScopeAttributes;
pub use nvtxw_bindings::nvtxwGetInterface_t as GetInterface;
pub use nvtxw_bindings::nvtxwInterfaceCore_t as InterfaceCore;
pub use nvtxw_bindings::nvtxwResultCode_t as ResultCode;
pub use nvtxw_bindings::nvtxwSessionAttributes_t as SessionAttributes;
pub use nvtxw_bindings::nvtxwSessionHandle_t as SessionHandle;
pub use nvtxw_bindings::nvtxwStreamAttributes_t as StreamAttributes;
pub use nvtxw_bindings::nvtxwStreamHandle_t as StreamHandle;

assert_type_eq_all!(i32, ResultCode);

pub struct InterfaceHandle {
    core: InterfaceCore,
    module_handle: *mut c_void,
}

pub const NVTX_PAYLOAD_ENTRY_FLAG_UNUSED: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_UNUSED as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_POINTER: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_POINTER as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_OFFSET_FROM_BASE: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_OFFSET_FROM_BASE as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_OFFSET_FROM_HERE: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_OFFSET_FROM_HERE as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_ZERO_TERMINATED: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_ZERO_TERMINATED as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_PAYLOAD_INDEX: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_PAYLOAD_INDEX as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_DEEP_COPY: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_DEEP_COPY as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_HIDE: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_HIDE as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_EVENT_TIMESTAMP: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_EVENT_TIMESTAMP as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_RANGE_END: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_RANGE_END as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_MARK: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_MARK as u64;
pub const NVTX_PAYLOAD_ENTRY_FLAG_COUNTER: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_FLAG_COUNTER as u64;

pub const NVTX_PAYLOAD_ENTRY_TYPE_INVALID: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_INVALID as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_CHAR: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_CHAR as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_UCHAR: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_UCHAR as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_SHORT: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_SHORT as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_USHORT: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_USHORT as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_INT: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_INT as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_UINT: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_UINT as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_LONG: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_LONG as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_ULONG: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_ULONG as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_LONGLONG: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_LONGLONG as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_ULONGLONG: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_ULONGLONG as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_INT8: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_INT8 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_UINT8: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_UINT8 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_INT16: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_INT16 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_UINT16: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_UINT16 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_INT32: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_INT32 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_UINT32: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_UINT32 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_INT64: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_INT64 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_UINT64: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_UINT64 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_FLOAT: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_FLOAT as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_LONGDOUBLE: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_LONGDOUBLE as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_SIZE: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_SIZE as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_ADDRESS: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_ADDRESS as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_WCHAR: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_WCHAR as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_CHAR8: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_CHAR8 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_CHAR16: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_CHAR16 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_CHAR32: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_CHAR32 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_INFO_ARRAY_SIZE: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_INFO_ARRAY_SIZE as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_BYTE: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_BYTE as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_INT128: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_INT128 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_UINT128: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_UINT128 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_FLOAT16: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_FLOAT16 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_FLOAT32: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_FLOAT32 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_FLOAT64: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_FLOAT64 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_FLOAT128: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_FLOAT128 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_BF16: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_BF16 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_TF32: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_TF32 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_CATEGORY: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_CATEGORY as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_COLOR_ARGB: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_COLOR_ARGB as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_SCOPE_ID: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_SCOPE_ID as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_PID_UINT32: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_PID_UINT32 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_PID_UINT64: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_PID_UINT64 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_TID_UINT32: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_TID_UINT32 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_TID_UINT64: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_TID_UINT64 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_CSTRING: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_CSTRING as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF8: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF8 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF16: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF16 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF32: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_CSTRING_UTF32 as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_NVTX_REGISTERED_STRING_HANDLE: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_NVTX_REGISTERED_STRING_HANDLE as u64;
pub const NVTX_PAYLOAD_ENTRY_TYPE_UNION_SELECTOR: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENTRY_TYPE_UNION_SELECTOR as u64;

pub const NVTX_TYPE_PAYLOAD_SCHEMA_REFERENCED: u64 =
    nvtxw_bindings::NVTX_TYPE_PAYLOAD_SCHEMA_REFERENCED as u64;
pub const NVTX_TYPE_PAYLOAD_SCHEMA_RAW: u64 = nvtxw_bindings::NVTX_TYPE_PAYLOAD_SCHEMA_RAW as u64;

pub const NVTX_PAYLOAD_SCHEMA_TYPE_INVALID: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_TYPE_INVALID as u64;
pub const NVTX_PAYLOAD_SCHEMA_TYPE_STATIC: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_TYPE_STATIC as u64;
pub const NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC as u64;
pub const NVTX_PAYLOAD_SCHEMA_TYPE_UNION: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_TYPE_UNION as u64;
pub const NVTX_PAYLOAD_SCHEMA_TYPE_UNION_WITH_INTERNAL_SELECTOR: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_TYPE_UNION_WITH_INTERNAL_SELECTOR as u64;

pub const NVTX_PAYLOAD_SCHEMA_FLAG_NONE: u64 = nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_FLAG_NONE as u64;
pub const NVTX_PAYLOAD_SCHEMA_FLAG_DEEP_COPY: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_FLAG_DEEP_COPY as u64;
pub const NVTX_PAYLOAD_SCHEMA_FLAG_REFERENCED: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_FLAG_REFERENCED as u64;
pub const NVTX_PAYLOAD_SCHEMA_FLAG_COUNTER_GROUP: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_FLAG_COUNTER_GROUP as u64;

pub const NVTX_PAYLOAD_SCHEMA_ATTR_NAME: u64 = nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_ATTR_NAME as u64;
pub const NVTX_PAYLOAD_SCHEMA_ATTR_TYPE: u64 = nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_ATTR_TYPE as u64;
pub const NVTX_PAYLOAD_SCHEMA_ATTR_FLAGS: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_ATTR_FLAGS as u64;
pub const NVTX_PAYLOAD_SCHEMA_ATTR_ENTRIES: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_ATTR_ENTRIES as u64;
pub const NVTX_PAYLOAD_SCHEMA_ATTR_NUM_ENTRIES: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_ATTR_NUM_ENTRIES as u64;
pub const NVTX_PAYLOAD_SCHEMA_ATTR_STATIC_SIZE: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_ATTR_STATIC_SIZE as u64;
pub const NVTX_PAYLOAD_SCHEMA_ATTR_ALIGNMENT: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_ATTR_ALIGNMENT as u64;
pub const NVTX_PAYLOAD_SCHEMA_ATTR_SCHEMA_ID: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_ATTR_SCHEMA_ID as u64;
pub const NVTX_PAYLOAD_SCHEMA_ATTR_EXTENSION: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_SCHEMA_ATTR_EXTENSION as u64;

pub const NVTX_PAYLOAD_ENUM_ATTR_NAME: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENUM_ATTR_NAME as u64;
pub const NVTX_PAYLOAD_ENUM_ATTR_ENTRIES: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENUM_ATTR_ENTRIES as u64;
pub const NVTX_PAYLOAD_ENUM_ATTR_NUM_ENTRIES: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENUM_ATTR_NUM_ENTRIES as u64;
pub const NVTX_PAYLOAD_ENUM_ATTR_SIZE: u64 = nvtxw_bindings::NVTX_PAYLOAD_ENUM_ATTR_SIZE as u64;
pub const NVTX_PAYLOAD_ENUM_ATTR_SCHEMA_ID: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENUM_ATTR_SCHEMA_ID as u64;
pub const NVTX_PAYLOAD_ENUM_ATTR_EXTENSION: u64 =
    nvtxw_bindings::NVTX_PAYLOAD_ENUM_ATTR_EXTENSION as u64;

pub const NVTXW3_RESULT_SUCCESS: i32 = nvtxw_bindings::NVTXW3_RESULT_SUCCESS;
pub const NVTXW3_RESULT_FAILED: i32 = nvtxw_bindings::NVTXW3_RESULT_FAILED;
pub const NVTXW3_RESULT_INVALID_ARGUMENT: i32 = nvtxw_bindings::NVTXW3_RESULT_INVALID_ARGUMENT;
pub const NVTXW3_RESULT_INVALID_INIT_MODE: i32 = nvtxw_bindings::NVTXW3_RESULT_INVALID_INIT_MODE;
pub const NVTXW3_RESULT_LIBRARY_NOT_FOUND: i32 = nvtxw_bindings::NVTXW3_RESULT_LIBRARY_NOT_FOUND;
pub const NVTXW3_RESULT_CONFIG_NOT_FOUND: i32 = nvtxw_bindings::NVTXW3_RESULT_CONFIG_NOT_FOUND;
pub const NVTXW3_RESULT_LOADER_SYMBOL_MISSING: i32 =
    nvtxw_bindings::NVTXW3_RESULT_LOADER_SYMBOL_MISSING;
pub const NVTXW3_RESULT_LOADER_FAILED: i32 = nvtxw_bindings::NVTXW3_RESULT_LOADER_FAILED;
pub const NVTXW3_RESULT_INTERFACE_ID_NOT_SUPPORTED: i32 =
    nvtxw_bindings::NVTXW3_RESULT_INTERFACE_ID_NOT_SUPPORTED;
pub const NVTXW3_RESULT_CONFIG_MISSING_LOADER_INFO: i32 =
    nvtxw_bindings::NVTXW3_RESULT_CONFIG_MISSING_LOADER_INFO;
pub const NVTXW3_RESULT_UNSUPPORTED_LOADER_MODE: i32 =
    nvtxw_bindings::NVTXW3_RESULT_UNSUPPORTED_LOADER_MODE;
pub const NVTXW3_RESULT_ENV_VAR_NOT_FOUND: i32 = nvtxw_bindings::NVTXW3_RESULT_ENV_VAR_NOT_FOUND;

pub const NVTXW3_INIT_MODE_SEARCH_DEFAULT: i32 = nvtxw_bindings::NVTXW3_INIT_MODE_SEARCH_DEFAULT;
pub const NVTXW3_INIT_MODE_LIBRARY_FILENAME: i32 =
    nvtxw_bindings::NVTXW3_INIT_MODE_LIBRARY_FILENAME;
pub const NVTXW3_INIT_MODE_LIBRARY_DIRECTORY: i32 =
    nvtxw_bindings::NVTXW3_INIT_MODE_LIBRARY_DIRECTORY;
pub const NVTXW3_INIT_MODE_CONFIG_FILENAME: i32 = nvtxw_bindings::NVTXW3_INIT_MODE_CONFIG_FILENAME;
pub const NVTXW3_INIT_MODE_CONFIG_DIRECTORY: i32 =
    nvtxw_bindings::NVTXW3_INIT_MODE_CONFIG_DIRECTORY;
pub const NVTXW3_INIT_MODE_CONFIG_STRING: i32 = nvtxw_bindings::NVTXW3_INIT_MODE_CONFIG_STRING;
pub const NVTXW3_INIT_MODE_CONFIG_ENV_VAR: i32 = nvtxw_bindings::NVTXW3_INIT_MODE_CONFIG_ENV_VAR;

pub const NVTXW3_INTERFACE_ID_CORE_V1: i32 = nvtxw_bindings::NVTXW3_INTERFACE_ID_CORE_V1;

/*
pub const NVTXW3_STREAM_ORDER_INTERLEAVING_NONE: i16 =
    nvtxw_bindings::NVTXW3_STREAM_ORDER_INTERLEAVING_NONE as i16;
pub const NVTXW3_STREAM_ORDER_INTERLEAVING_EVENT_SCOPE: i16 =
    nvtxw_bindings::NVTXW3_STREAM_ORDER_INTERLEAVING_EVENT_SCOPE as i16;

pub const NVTXW3_STREAM_ORDERING_TYPE_UNKNOWN: i16 =
    nvtxw_bindings::NVTXW3_STREAM_ORDERING_TYPE_UNKNOWN as i16;
pub const NVTXW3_STREAM_ORDERING_TYPE_STRICT: i16 =
    nvtxw_bindings::NVTXW3_STREAM_ORDERING_TYPE_STRICT as i16;
pub const NVTXW3_STREAM_ORDERING_TYPE_PACKED_RANGE_START: i16 =
    nvtxw_bindings::NVTXW3_STREAM_ORDERING_TYPE_PACKED_RANGE_START as i16;
pub const NVTXW3_STREAM_ORDERING_TYPE_PACKED_RANGE_END: i16 =
    nvtxw_bindings::NVTXW3_STREAM_ORDERING_TYPE_PACKED_RANGE_END as i16;
*/

pub const NVTXW3_STREAM_ORDER_INTERLEAVING_NONE: i16 = 0;
pub const NVTXW3_STREAM_ORDER_INTERLEAVING_EVENT_SCOPE: i16 = 1;

pub const NVTXW3_STREAM_ORDERING_TYPE_UNKNOWN: i16 = 0;
pub const NVTXW3_STREAM_ORDERING_TYPE_STRICT: i16 = 1;
pub const NVTXW3_STREAM_ORDERING_TYPE_PACKED_RANGE_START: i16 = 2;
pub const NVTXW3_STREAM_ORDERING_TYPE_PACKED_RANGE_END: i16 = 3;

pub const NVTXW3_STREAM_ORDERING_SKID_NONE: i32 = nvtxw_bindings::NVTXW3_STREAM_ORDERING_SKID_NONE;
pub const NVTXW3_STREAM_ORDERING_SKID_TIME_NS: i32 =
    nvtxw_bindings::NVTXW3_STREAM_ORDERING_SKID_TIME_NS;
pub const NVTXW3_STREAM_ORDERING_SKID_EVENT_COUNT: i32 =
    nvtxw_bindings::NVTXW3_STREAM_ORDERING_SKID_EVENT_COUNT;

fn check(result: ResultCode) -> Result<(), ResultCode> {
    if result != NVTXW3_RESULT_SUCCESS {
        return Err(result);
    }
    Ok(())
}

// compatibility for File::create_new before Rust 1.77
fn file_create_new<P: AsRef<Path>>(path: P) -> io::Result<File> {
    OpenOptions::new()
        .read(true)
        .write(true)
        .create_new(true)
        .open(path.as_ref())
}

pub fn initialize_simple(backend: Option<OsString>) -> Result<InterfaceHandle, ResultCode> {
    let default_backend = if cfg!(windows) {
        "NvtxwBackend.dll"
    } else if cfg!(target_vendor = "apple") {
        "libNvtxwBackend.dylib"
    } else {
        "libNvtxwBackend.so"
    };

    let mut path_backend = if let Some(os_backend) = backend {
        PathBuf::from(os_backend)
    } else {
        PathBuf::from(default_backend)
    };
    if path_backend.is_dir() {
        path_backend.push(default_backend);
    }
    let s_backend = path_backend.to_str().expect("backend to_str() failed");
    let c_mode_string = CString::new(s_backend).expect("c_mode_string CString::new failed");

    let mut get_interface_func: GetInterface = Default::default();
    let mut module_handle: *mut c_void = null_mut();
    check(unsafe {
        nvtxw_bindings::nvtxwInitialize(
            NVTXW3_INIT_MODE_LIBRARY_FILENAME,
            c_mode_string.as_ptr(),
            &mut get_interface_func,
            &mut module_handle,
        )
    })?;

    let get_interface = get_interface_func.ok_or(NVTXW3_RESULT_FAILED)?;

    let mut interface_void: *const c_void = null();
    check(unsafe { get_interface(NVTXW3_INTERFACE_ID_CORE_V1, &mut interface_void) })?;

    if interface_void.is_null() {
        return Err(NVTXW3_RESULT_FAILED);
    }

    let ptr_interface = interface_void as *const InterfaceCore;
    let interface_handle = InterfaceHandle {
        core: unsafe { *ptr_interface },
        module_handle,
    };

    Ok(interface_handle)
}

pub fn unload(interface: &InterfaceHandle) {
    unsafe {
        nvtxw_bindings::nvtxwUnload(interface.module_handle);
    }
}

pub fn session_begin(
    interface: &InterfaceHandle,
    attr: SessionAttributes,
) -> Result<SessionHandle, ResultCode> {
    let session_begin = interface
        .core
        .SessionBegin
        .ok_or(NVTXW3_RESULT_INVALID_ARGUMENT)?;

    let mut session: SessionHandle = Default::default();
    check(unsafe { session_begin(&mut session, &attr) })?;

    if session.opaque.is_null() {
        return Err(NVTXW3_RESULT_FAILED);
    }

    Ok(session)
}

pub fn session_begin_simple(
    interface: &InterfaceHandle,
    output: OsString,
    force: bool,
    merge: Option<OsString>,
) -> Result<SessionHandle, ResultCode> {
    let extension = "nsys-rep";
    let mut path_output = PathBuf::from(output);

    // avoid "*.nsys-rep.nsys-rep", since the backend adds the extension for us
    if path_output.extension() == Some(OsStr::new(extension)) {
        path_output.set_extension("");
    }

    // however, we are responsible for when the output already exists
    let mut path_test = path_output.with_extension(extension);
    // claim the file, will be overwritten by the backend if successful
    match file_create_new(&path_test) {
        Ok(_) => {}
        Err(ref e) if e.kind() == io::ErrorKind::AlreadyExists => {
            if force {
                println!("Overwriting {:?}", &path_test);
            } else {
                let mut i = 1;
                let retry_limit = 100;
                loop {
                    path_test = path_output.with_extension(format!("{}.{}", i, extension));
                    match file_create_new(path_test) {
                        Ok(_) => {
                            path_output.set_extension(format!("{}", i));
                            break;
                        }
                        Err(ref e) if e.kind() == io::ErrorKind::AlreadyExists => {
                            if i >= retry_limit {
                                return Err(NVTXW3_RESULT_FAILED);
                            }
                        }
                        Err(_) => {
                            return Err(NVTXW3_RESULT_FAILED);
                        }
                    }
                    i += 1;
                }
            }
        }
        Err(_) => {
            return Err(NVTXW3_RESULT_FAILED);
        }
    }

    let s_output = path_output.to_str().expect("output to_str() failed");
    let c_output = CString::new(s_output).expect("c_output CString::new failed");

    let c_config = merge
        .map(|p| format!("ReportMerge={}", p.to_str().expect("merge to_str() failed")))
        .map(|s| CString::new(s).expect("c_config CString::new failed"));

    let attr = SessionAttributes {
        struct_size: size_of::<SessionAttributes>(),
        name: c_output.as_ptr(),
        configString: c_config.as_ref().map_or_else(null, |o| o.as_ptr()),
    };

    session_begin(interface, attr)
}

pub fn session_end(interface: &InterfaceHandle, session: SessionHandle) -> Result<(), ResultCode> {
    let session_end = interface
        .core
        .SessionEnd
        .ok_or(NVTXW3_RESULT_INVALID_ARGUMENT)?;

    check(unsafe { session_end(session) })
}

pub fn stream_open(
    interface: &InterfaceHandle,
    session: SessionHandle,
    attr: StreamAttributes,
) -> Result<StreamHandle, ResultCode> {
    let stream_open = interface
        .core
        .StreamOpen
        .ok_or(NVTXW3_RESULT_INVALID_ARGUMENT)?;

    let mut stream: StreamHandle = Default::default();
    check(unsafe { stream_open(&mut stream, session, &attr) })?;

    if stream.opaque.is_null() {
        return Err(NVTXW3_RESULT_FAILED);
    }

    Ok(stream)
}

pub fn stream_open_simple(
    interface: &InterfaceHandle,
    session: SessionHandle,
    stream_name: String,
    domain_name: String,
) -> Result<StreamHandle, ResultCode> {
    let c_stream_name = CString::new(stream_name).expect("c_stream_name CString::new failed");
    let c_domain_name = CString::new(domain_name).expect("c_domain_name CString::new failed");

    let attr = StreamAttributes {
        struct_size: size_of::<StreamAttributes>(),
        name: c_stream_name.as_ptr(),
        nvtxDomainName: c_domain_name.as_ptr(),
        eventScopePath: null(),
        orderInterleaving: NVTXW3_STREAM_ORDER_INTERLEAVING_NONE,
        orderingType: NVTXW3_STREAM_ORDERING_TYPE_UNKNOWN,
        orderingSkid: NVTXW3_STREAM_ORDERING_SKID_NONE,
        orderingSkidAmount: 0,
    };

    stream_open(interface, session, attr)
}

pub fn stream_close(interface: &InterfaceHandle, stream: StreamHandle) -> Result<(), ResultCode> {
    let stream_close = interface
        .core
        .StreamClose
        .ok_or(NVTXW3_RESULT_INVALID_ARGUMENT)?;

    check(unsafe { stream_close(stream) })
}

pub fn event_scope_register(
    interface: &InterfaceHandle,
    stream: StreamHandle,
    attr: &EventScopeAttributes,
) -> Result<(), ResultCode> {
    let event_scope_register = interface
        .core
        .EventScopeRegister
        .ok_or(NVTXW3_RESULT_INVALID_ARGUMENT)?;

    check(unsafe { event_scope_register(stream, attr) })
}

pub fn schema_register(
    interface: &InterfaceHandle,
    stream: StreamHandle,
    attr: &PayloadSchemaAttr,
) -> Result<(), ResultCode> {
    let schema_register = interface
        .core
        .SchemaRegister
        .ok_or(NVTXW3_RESULT_INVALID_ARGUMENT)?;

    check(unsafe { schema_register(stream, attr) })
}

pub fn enum_register(
    interface: &InterfaceHandle,
    stream: StreamHandle,
    attr: &PayloadEnumAttr,
) -> Result<(), ResultCode> {
    let enum_register = interface
        .core
        .EnumRegister
        .ok_or(NVTXW3_RESULT_INVALID_ARGUMENT)?;

    check(unsafe { enum_register(stream, attr) })
}

pub fn event_write(
    interface: &InterfaceHandle,
    stream: StreamHandle,
    events: &[PayloadData],
) -> Result<(), ResultCode> {
    let event_write = interface
        .core
        .EventWrite
        .ok_or(NVTXW3_RESULT_INVALID_ARGUMENT)?;

    let count = events.len();
    let ptr = events.as_ptr();

    check(unsafe { event_write(stream, ptr, count) })
}
