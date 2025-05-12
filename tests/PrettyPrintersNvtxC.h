/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#pragma once
#include <nvtx3/nvToolsExt.h>
#include <iostream>

// Pretty-printers for color, payload, and message discriminated-union types

inline void WriteColorType(std::ostream& os, nvtxColorType_t t)
{
    switch (t)
    {
        case NVTX_COLOR_ARGB   : os << "NVTX_COLOR_ARGB"; break;
        case NVTX_COLOR_UNKNOWN: os << "<UNKNOWN TYPE>";  break;
        default                : os << "<INVALID TYPE = " << static_cast<int32_t>(t) << ">";
    }
}

inline std::ostream& operator<<(std::ostream& os, nvtxColorType_t t)
{
    WriteColorType(os, t);
    return os;
}

inline void WritePayloadType(std::ostream& os, nvtxPayloadType_t t)
{
    switch (t)
    {
        case NVTX_PAYLOAD_TYPE_UNSIGNED_INT64: os << "NVTX_PAYLOAD_TYPE_UNSIGNED_INT64"; break;
        case NVTX_PAYLOAD_TYPE_INT64         : os << "NVTX_PAYLOAD_TYPE_INT64         "; break;
        case NVTX_PAYLOAD_TYPE_DOUBLE        : os << "NVTX_PAYLOAD_TYPE_DOUBLE        "; break;
        case NVTX_PAYLOAD_TYPE_UNSIGNED_INT32: os << "NVTX_PAYLOAD_TYPE_UNSIGNED_INT32"; break;
        case NVTX_PAYLOAD_TYPE_INT32         : os << "NVTX_PAYLOAD_TYPE_INT32         "; break;
        case NVTX_PAYLOAD_TYPE_FLOAT         : os << "NVTX_PAYLOAD_TYPE_FLOAT         "; break;
        case NVTX_PAYLOAD_UNKNOWN            : os << "<UNKNOWN TYPE>";                   break;
        default                              : os << "<INVALID TYPE = " << static_cast<int32_t>(t) << ">";
    }
}

inline void WritePayloadValue(std::ostream& os, nvtxPayloadType_t t, nvtxEventAttributes_v2::payload_t val)
{
    switch (t)
    {
        case NVTX_PAYLOAD_TYPE_UNSIGNED_INT64: os << val.ullValue;      break;
        case NVTX_PAYLOAD_TYPE_INT64         : os << val.llValue;       break;
        case NVTX_PAYLOAD_TYPE_DOUBLE        : os << val.dValue;        break;
        case NVTX_PAYLOAD_TYPE_UNSIGNED_INT32: os << val.uiValue;       break;
        case NVTX_PAYLOAD_TYPE_INT32         : os << val.iValue;        break;
        case NVTX_PAYLOAD_TYPE_FLOAT         : os << val.fValue;        break;
        case NVTX_PAYLOAD_UNKNOWN            : os << "<IGNORED VALUE>"; break;
        default                              : os << "<INVALID VALUE>";
    }
}

inline void WritePayload(std::ostream& os, nvtxPayloadType_t t, nvtxEventAttributes_v2::payload_t val)
{
    WritePayloadType(os, t);
    os << " = ";
    WritePayloadValue(os, t, val);
}

inline std::ostream& operator<<(std::ostream& os, nvtxPayloadType_t t)
{
    WritePayloadType(os, t);
    return os;
}

inline void WriteMessageType(std::ostream& os, nvtxMessageType_t t)
{
    switch (t)
    {
        case NVTX_MESSAGE_TYPE_ASCII     : os << "NVTX_MESSAGE_TYPE_ASCII";      break;
        case NVTX_MESSAGE_TYPE_UNICODE   : os << "NVTX_MESSAGE_TYPE_UNICODE";    break;
        case NVTX_MESSAGE_TYPE_REGISTERED: os << "NVTX_MESSAGE_TYPE_REGISTERED"; break;
        case NVTX_MESSAGE_UNKNOWN        : os << "<UNKNOWN TYPE>";               break;
        default                          : os << "<INVALID TYPE = " << static_cast<int32_t>(t) << ">";
    }
}

inline void WriteMessageValue(std::ostream& os, nvtxMessageType_t t, nvtxMessageValue_t val)
{
    switch (t)
    {
        case NVTX_MESSAGE_TYPE_ASCII     : os << val.ascii;             break;
        case NVTX_MESSAGE_TYPE_UNICODE   : os << "<Some wide chars>";   break;
        case NVTX_MESSAGE_TYPE_REGISTERED: os << "Registered handle: " << static_cast<const void*>(val.registered); break;
        case NVTX_MESSAGE_UNKNOWN        : os << "<IGNORED VALUE>";     break;
        default                          : os << "<INVALID VALUE>";
    }
}

inline void WriteMessage(std::ostream& os, nvtxMessageType_t t, nvtxMessageValue_t val)
{
    WriteMessageType(os, t);
    os << " = ";
    WriteMessageValue(os, t, val);
}

inline std::ostream& operator<<(std::ostream& os, nvtxMessageType_t t)
{
    WriteMessageType(os, t);
    return os;
}

// Pretty-printer for attributes struct

#if 1
inline std::ostream& operator<<(std::ostream& os, nvtxEventAttributes_t const& a)
{
    os << "{ver: " << a.version
        << ", size: " << a.size
        << ", category: " << a.category
        << ", color: " << static_cast<nvtxColorType_t>(a.colorType) << " 0x" << std::hex << a.color << std::dec
        << ", payload: " << static_cast<nvtxPayloadType_t>(a.payloadType) << " ";
    WritePayloadValue(os, static_cast<nvtxPayloadType_t>(a.payloadType), a.payload);
    os << ", message: " << static_cast<nvtxMessageType_t>(a.messageType) << " \"";
    WriteMessageValue(os, static_cast<nvtxMessageType_t>(a.messageType), a.message);
    os << "\"}";

    return os;
}
#else
inline std::ostream& operator<<(std::ostream& os, nvtxEventAttributes_t const& a)
{
    os
        << "uint16_t version = " << a.version << "\n"
        << "uint16_t size = " << a.size << "\n"
        << "uint32_t category = " << a.category << "\n"
        << "int32_t colorType = " << (nvtxColorType_t)a.colorType << "\n"
        << "uint32_t color = 0x" << std::hex << a.color << std::dec << "\n"
        << "int32_t payloadType = " << (nvtxPayloadType_t)a.payloadType << "\n"
        << "(union) payload = ";
    WritePayloadValue(os, (nvtxPayloadType_t)a.payloadType, a.payload);
    os << "\n"
        << "int32_t messageType = " << (nvtxMessageType_t)a.messageType << "\n"
        << "(union) message = ";
    WriteMessageValue(os, (nvtxMessageType_t)a.messageType, a.message);
    os << "\n";

    return os;
}
#endif
