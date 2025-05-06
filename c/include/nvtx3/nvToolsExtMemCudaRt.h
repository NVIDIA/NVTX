/*
 * SPDX-FileCopyrightText: Copyright (c) 2009-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#if defined(NVTX_AS_SYSTEM_HEADER)
#if defined(__clang__)
#pragma clang system_header
#elif defined(__GNUC__) || defined(__NVCOMPILER)
#pragma GCC system_header
#elif defined(_MSC_VER)
#pragma system_header
#endif
#endif

#include "nvToolsExtMem.h"

#include "cuda.h"
#include "cuda_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef NVTX_MEM_CUDART_CONTENTS_V1
#define NVTX_MEM_CUDART_CONTENTS_V1

/** \defgroup MEMORY_CUDART Memory CUDA Runtime
 * See page \ref PAGE_MEMORY_CUDART.
 * @{
 */

/** \brief The memory is from a CUDA runtime array.
 *
 * Relevant functions: cudaMallocArray,  cudaMalloc3DArray
 * Also cudaArray_t from other types such as cudaMipmappedArray_t
 *
 * NVTX_MEM_HEAP_HANDLE_PROCESS_WIDE is not supported
 *
 * nvtxMemHeapRegister receives a heapDesc of type cudaArray_t because the description can be retrieved by tools through cudaArrayGetInfo()
 * nvtxMemRegionRegisterEx receives a regionDesc of type nvtxMemCudaArrayRangeDesc_t
 */
#define NVTX_MEM_TYPE_CUDA_ARRAY 0x11

/** \brief structure to describe memory in a CUDA array object
 */
typedef struct nvtxMemCudaArrayRangeDesc_v1
{
    uint16_t extCompatID; /* Set to NVTX_EXT_COMPATID_MEM */
    uint16_t structSize; /* Size of the structure. */
    uint32_t reserved0;
    cudaArray_t  src;
    size_t offset[3];
    size_t extent[3];
} nvtxMemCudaArrayRangeDesc_v1;
typedef nvtxMemCudaArrayRangeDesc_v1 nvtxMemCudaArrayRangeDesc_t;


/** \brief The memory is from a CUDA device array.
 *
 * Relevant functions: cuArrayCreate,  cuArray3DCreate
 * Also CUarray from other types such as CUmipmappedArray
 *
 * NVTX_MEM_HEAP_HANDLE_PROCESS_WIDE is not supported
 *
 * nvtxMemHeapRegister receives a heapDesc of type cudaArray_t because the description can be retrieved by tools through cudaArrayGetInfo()
 * nvtxMemRegionRegisterEx receives a regionDesc of type nvtxMemCuArrayRangeDesc_t
 */
#define NVTX_MEM_TYPE_CU_ARRAY 0x12

/** \brief structure to describe memory in a CUDA array object
 */
typedef struct nvtxMemCuArrayRangeDesc_v1
{
    uint16_t extCompatID; /* Set to NVTX_EXT_COMPATID_MEM */
    uint16_t structSize; /* Size of the structure. */
    uint32_t reserved0;
    CUarray  src;
    size_t offset[3];
    size_t extent[3];
} nvtxMemCuArrayRangeDesc_v1;
typedef nvtxMemCuArrayRangeDesc_v1 nvtxMemCuArrayRangeDesc_t;

/* Reserving 0x2-0xF for more common types */

#define NVTX_MEM_CUDA_PEER_ALL_DEVICES -1

/** \brief Get the permission object that represent the CUDA runtime device
 * or cuda driver context
 *
 * This object will allow developers to adjust permissions applied to work executed
 * on the GPU.  It may be inherited or overridden by permissions object bound
 * with NVTX_MEM_PERMISSIONS_BIND_SCOPE_CUDA_STREAM, depending on the binding flags.
 *
 * Ex. change the peer to peer access permissions between devices in entirety
 * or punch through special holes
 *
 * By default, all memory is accessible that naturally would be to a CUDA kernel until
 * modified otherwise by nvtxMemCudaSetPeerAccess or changing regions.
 *
 * This object should also represent the CUDA driver API level context.
*/
NVTX_DECLSPEC nvtxMemPermissionsHandle_t NVTX_API nvtxMemCudaGetProcessWidePermissions(
    nvtxDomainHandle_t domain);

/** \brief Get the permission object that represent the CUDA runtime device
 * or cuda driver context
 *
 * This object will allow developers to adjust permissions applied to work executed
 * on the GPU.  It may be inherited or overridden by permissions object bound
 * with NVTX_MEM_PERMISSIONS_BIND_SCOPE_CUDA_STREAM, depending on the binding flags.
 *
 * Ex. change the peer to peer access permissions between devices in entirety
 * or punch through special holes
 *
 * By default, all memory is accessible that naturally would be to a CUDA kernel until
 * modified otherwise by nvtxMemCudaSetPeerAccess or changing regions.
 *
 * This object should also represent the CUDA driver API level context.
*/
NVTX_DECLSPEC nvtxMemPermissionsHandle_t NVTX_API nvtxMemCudaGetDeviceWidePermissions(
    nvtxDomainHandle_t domain,
    int device);

/** \brief Change the default behavior for all memory mapped in from a particular device.
 *
 * While typically all memory defaults to readable and writable, users may desire to limit
 * access to reduced default permissions such as read-only and a per-device basis.
 *
 * Regions can used to further override smaller windows of memory.
 *
 * devicePeer can be NVTX_MEM_CUDA_PEER_ALL_DEVICES
 *
*/
NVTX_DECLSPEC void NVTX_API nvtxMemCudaSetPeerAccess(
    nvtxDomainHandle_t domain,
    nvtxMemPermissionsHandle_t permissions,
    int devicePeer, /* device number such as from cudaGetDevice() or NVTX_MEM_CUDA_PEER_ALL_DEVICES */
    uint32_t flags); /* NVTX_MEM_PERMISSIONS_REGION_FLAGS_* */

/** \brief Mark memory ranges as initialized.
*
* The heap refers the the heap within which the region resides.
* This can be from nvtxMemHeapRegister, NVTX_MEM_HEAP_HANDLE_PROCESS_WIDE, or one provided from other extension API.
*
* The regionType arg will define which type is used in regionDescArray.
* The most commonly used type is NVTX_MEM_TYPE_VIRTUAL_ADDRESS.
*
* The regionCount arg is how many element are in regionDescArray and regionHandleArrayOut.
*
* The regionHandleArrayOut arg points to an array where the tool will provide region handles.
* If a pointer if provided, it is expected to have regionCount elements.
* This pointer can be NULL if regionType is NVTX_MEM_TYPE_VIRTUAL_ADDRESS.  In this case,
* the user can use the pointer to the virtual memory to reference the region in other
* related functions which accept a nvtxMemRegionRef_t.
*/
typedef struct nvtxMemMarkInitializedBatch_v1
{
    uint16_t extCompatID; /* Set to NVTX_EXT_COMPATID_MEM */
    uint16_t structSize; /* Size of the structure. */

    uint32_t regionType; /* NVTX_MEM_TYPE_* */

    size_t regionDescCount;
    size_t regionDescElementSize;
    void const* regionDescElements; /* this will also become the handle for this region */

} nvtxMemMarkInitializedBatch_v1;
typedef nvtxMemMarkInitializedBatch_v1 nvtxMemMarkInitializedBatch_t;

/** \brief Register a region of memory inside of a heap of linear process virtual memory
*
* stream is the CUDA stream where the range was accessed and initialized.
*/
NVTX_DECLSPEC void NVTX_API nvtxMemCudaMarkInitialized(
    nvtxDomainHandle_t domain,
    cudaStream_t stream,
    uint8_t isPerThreadStream, /* 0 for false, otherwise true */
    nvtxMemMarkInitializedBatch_t const* desc);

/** @} */

#endif /* NVTX_MEM_CUDART_CONTENTS_V1 */

#ifdef __GNUC__
#pragma GCC visibility push(internal)
#endif

#ifndef NVTX_NO_IMPL
#define NVTX_EXT_IMPL_MEM_CUDART_GUARD /* Ensure other headers cannot be included directly */
#include "nvtxDetail/nvtxExtImplMemCudaRt_v1.h"
#undef NVTX_EXT_IMPL_MEM_CUDART_GUARD
#endif /*NVTX_NO_IMPL*/

#ifdef __GNUC__
#pragma GCC visibility pop
#endif


#ifdef __cplusplus
}
#endif /* __cplusplus */
