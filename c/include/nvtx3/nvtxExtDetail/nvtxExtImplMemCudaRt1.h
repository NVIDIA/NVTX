/*
* Copyright 2009-2020  NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License v2.0 with LLVM Exceptions.
* See https://llvm.org/LICENSE.txt for license information.
* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#ifndef NVTX_EXT_IMPL_MEM_CUDART_GUARD
#error Never include this file directly -- it is automatically included by nvToolsExtMemCudaRt.h (except when NVTX_NO_IMPL is defined).
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Other macros will be inherited from nvtExtImplMemCudaRt1.h
 * We only need to redefine the range
 */

#define NVTX3EXT_MEM_FN14_name nvtxMemCudaGetProcessWidePermissions
#define NVTX3EXT_MEM_FN14_sig nvtxDomainHandle_t domain 
#define NVTX3EXT_MEM_FN14_args domain
#define NVTX3EXT_MEM_FN14_retType nvtxMemPermissionsHandle_t
#define NVTX3EXT_MEM_FN14_retCmd return
#define NVTX3EXT_MEM_FN14_def NVTX_MEM_PERMISSIONS_HANDLE_NO_TOOL
#define NVTX3EXT_MEM_FN14_global_idx 14
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemCudaGetProcessWidePermissions (NVTX3EXT_MEM_FN14_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)


#define NVTX3EXT_MEM_FN15_name nvtxMemCudaGetDeviceWidePermissions
#define NVTX3EXT_MEM_FN15_sig nvtxDomainHandle_t domain, int device 
#define NVTX3EXT_MEM_FN15_args domain,device
#define NVTX3EXT_MEM_FN15_retType nvtxMemPermissionsHandle_t
#define NVTX3EXT_MEM_FN15_retCmd return
#define NVTX3EXT_MEM_FN15_def NVTX_MEM_PERMISSIONS_HANDLE_NO_TOOL
#define NVTX3EXT_MEM_FN15_global_idx 15
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemCudaGetDeviceWidePermissions (NVTX3EXT_MEM_FN15_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)


#define NVTX3EXT_MEM_FN16_name nvtxMemCudaSetPeerAccess
#define NVTX3EXT_MEM_FN16_sig nvtxDomainHandle_t domain, nvtxMemPermissionsHandle_t permissions, int devicePeer, uint32_t flags
#define NVTX3EXT_MEM_FN16_args domain,permissions, devicePeer, flags
#define NVTX3EXT_MEM_FN16_retType void
#define NVTX3EXT_MEM_FN16_retCmd
#define NVTX3EXT_MEM_FN16_def 
#define NVTX3EXT_MEM_FN16_global_idx 16
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemCudaSetPeerAccess (NVTX3EXT_MEM_FN16_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)


#undef NVTX3EXT_MEM_FUNCTIONS
#define NVTX3EXT_MEM_FUNCTIONS( MACRO ) \
MACRO ( 14 ) \
MACRO ( 15 ) \
MACRO ( 16 )

#if defined NVTX_DISABLE

#else  /*NVTX_DISABLE*/

NVTX3EXT_MEM_FUNCTIONS( NVTX3EXT_MEM_IMPL_FNPTR_TYPEDEF )

#endif /* NVTX_DISABLE */

NVTX3EXT_MEM_FUNCTIONS( NVTX3EXT_MEM_IMPL_WRAPPER_DEFINE )

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
