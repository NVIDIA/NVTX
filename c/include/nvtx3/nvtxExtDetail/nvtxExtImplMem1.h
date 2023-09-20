/*
* Copyright 2009-2020  NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License v2.0 with LLVM Exceptions.
* See https://llvm.org/LICENSE.txt for license information.
* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#ifndef NVTX_EXT_IMPL_MEM_GUARD
#error Never include this file directly -- it is automatically included by nvToolsExtMem.h (except when NVTX_NO_IMPL is defined).
#endif

#define NVTX_EXT_IMPL_GUARD
#include "nvtxExtImpl.h"
#undef NVTX_EXT_IMPL_GUARD

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NVTX_EXT_MODULEID_MEM 1
#define NVTX_EXT_SEGMENT_MEM_CORE1 1

#define NVTXMEM_VERSIONED_IDENTIFIER_L3(NAME, VERSION, COMPATID) NAME##_v##VERSION##_mem##COMPATID
#define NVTXMEM_VERSIONED_IDENTIFIER_L2(NAME, VERSION, COMPATID) NVTXMEM_VERSIONED_IDENTIFIER_L3(NAME, VERSION, COMPATID)
#define NVTXMEM_VERSIONED_IDENTIFIER(NAME) NVTXMEM_VERSIONED_IDENTIFIER_L2(NAME, NVTX_VERSION, NVTX_EXT_COMPATID_MEM)
#define NVTX3EXT_MEM_FN(ID, PART) NVTX3EXT_MEM_FN##ID##PART

#define NVTX3EXT_MEM_GLOBALS NVTX_VERSIONED_IDENTIFIER(nvtxExtGlobals1)

#define NVTX3EXT_MEM_INIT 0
#define NVTX3EXT_MEM_FN_IDX_FIRST 1

#define NVTX3EXT_MEM_FN1_name nvtxMemHeapRegister
#define NVTX3EXT_MEM_FN1_sig nvtxDomainHandle_t domain,nvtxMemHeapDesc_t const* desc
#define NVTX3EXT_MEM_FN1_args domain,desc
#define NVTX3EXT_MEM_FN1_retType nvtxMemHeapHandle_t
#define NVTX3EXT_MEM_FN1_retCmd return
#define NVTX3EXT_MEM_FN1_def NVTX_MEM_HEAP_HANDLE_NO_TOOL
#define NVTX3EXT_MEM_FN1_global_idx 1
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemHeapRegister (NVTX3EXT_MEM_FN1_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)

#define NVTX3EXT_MEM_FN2_name nvtxMemHeapUnregister
#define NVTX3EXT_MEM_FN2_sig nvtxDomainHandle_t domain, nvtxMemHeapHandle_t heap
#define NVTX3EXT_MEM_FN2_args domain,heap
#define NVTX3EXT_MEM_FN2_retType void
#define NVTX3EXT_MEM_FN2_retCmd
#define NVTX3EXT_MEM_FN2_def
#define NVTX3EXT_MEM_FN2_global_idx 2
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemHeapUnregister (NVTX3EXT_MEM_FN2_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)

#define NVTX3EXT_MEM_FN3_name nvtxMemHeapReset
#define NVTX3EXT_MEM_FN3_sig nvtxDomainHandle_t domain, nvtxMemHeapHandle_t heap
#define NVTX3EXT_MEM_FN3_args domain,heap
#define NVTX3EXT_MEM_FN3_retType void
#define NVTX3EXT_MEM_FN3_retCmd
#define NVTX3EXT_MEM_FN3_def
#define NVTX3EXT_MEM_FN3_global_idx 3
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemHeapReset (NVTX3EXT_MEM_FN3_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)

#define NVTX3EXT_MEM_FN4_name nvtxMemRegionsRegister
#define NVTX3EXT_MEM_FN4_sig nvtxDomainHandle_t domain, nvtxMemRegionsRegisterBatch_t const* desc
#define NVTX3EXT_MEM_FN4_args domain,desc
#define NVTX3EXT_MEM_FN4_retType void
#define NVTX3EXT_MEM_FN4_retCmd
#define NVTX3EXT_MEM_FN4_def
#define NVTX3EXT_MEM_FN4_global_idx 4
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemRegionsRegister (NVTX3EXT_MEM_FN4_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)

#define NVTX3EXT_MEM_FN5_name nvtxMemRegionsResize
#define NVTX3EXT_MEM_FN5_sig nvtxDomainHandle_t domain,nvtxMemRegionsResizeBatch_t const* desc
#define NVTX3EXT_MEM_FN5_args domain,desc
#define NVTX3EXT_MEM_FN5_retType void
#define NVTX3EXT_MEM_FN5_retCmd
#define NVTX3EXT_MEM_FN5_def
#define NVTX3EXT_MEM_FN5_global_idx 5
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemRegionsResize (NVTX3EXT_MEM_FN5_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)

#define NVTX3EXT_MEM_FN6_name nvtxMemRegionsUnregister
#define NVTX3EXT_MEM_FN6_sig nvtxDomainHandle_t domain,nvtxMemRegionsUnregisterBatch_t const* desc
#define NVTX3EXT_MEM_FN6_args domain,desc
#define NVTX3EXT_MEM_FN6_retType void
#define NVTX3EXT_MEM_FN6_retCmd
#define NVTX3EXT_MEM_FN6_def
#define NVTX3EXT_MEM_FN6_global_idx 6
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemRegionsUnregister (NVTX3EXT_MEM_FN6_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)

#define NVTX3EXT_MEM_FN7_name nvtxMemRegionsName
#define NVTX3EXT_MEM_FN7_sig nvtxDomainHandle_t domain,nvtxMemRegionsNameBatch_t const* desc
#define NVTX3EXT_MEM_FN7_args domain,desc
#define NVTX3EXT_MEM_FN7_retType void
#define NVTX3EXT_MEM_FN7_retCmd
#define NVTX3EXT_MEM_FN7_def
#define NVTX3EXT_MEM_FN7_global_idx 7
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemRegionsName (NVTX3EXT_MEM_FN7_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)

#define NVTX3EXT_MEM_FN8_name nvtxMemPermissionsAssign
#define NVTX3EXT_MEM_FN8_sig nvtxDomainHandle_t domain,nvtxMemPermissionsAssignBatch_t const* desc
#define NVTX3EXT_MEM_FN8_args domain,desc
#define NVTX3EXT_MEM_FN8_retType void
#define NVTX3EXT_MEM_FN8_retCmd
#define NVTX3EXT_MEM_FN8_def
#define NVTX3EXT_MEM_FN8_global_idx 8
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemPermissionsAssign (NVTX3EXT_MEM_FN8_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)

#define NVTX3EXT_MEM_FN9_name nvtxMemPermissionsCreate
#define NVTX3EXT_MEM_FN9_sig nvtxDomainHandle_t domain,int32_t creationflags
#define NVTX3EXT_MEM_FN9_args domain,creationflags
#define NVTX3EXT_MEM_FN9_retType nvtxMemPermissionsHandle_t
#define NVTX3EXT_MEM_FN9_retCmd return
#define NVTX3EXT_MEM_FN9_def NVTX_MEM_PERMISSIONS_HANDLE_NO_TOOL
#define NVTX3EXT_MEM_FN9_global_idx 9
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemPermissionsCreate (NVTX3EXT_MEM_FN9_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)

#define NVTX3EXT_MEM_FN10_name nvtxMemPermissionsDestroy
#define NVTX3EXT_MEM_FN10_sig nvtxDomainHandle_t domain,nvtxMemPermissionsHandle_t permissions
#define NVTX3EXT_MEM_FN10_args domain,permissions
#define NVTX3EXT_MEM_FN10_retType void
#define NVTX3EXT_MEM_FN10_retCmd
#define NVTX3EXT_MEM_FN10_def
#define NVTX3EXT_MEM_FN10_global_idx 10
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemPermissionsDestroy (NVTX3EXT_MEM_FN10_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)

#define NVTX3EXT_MEM_FN11_name nvtxMemPermissionsReset
#define NVTX3EXT_MEM_FN11_sig nvtxDomainHandle_t domain,nvtxMemPermissionsHandle_t permissions
#define NVTX3EXT_MEM_FN11_args domain,permissions
#define NVTX3EXT_MEM_FN11_retType void
#define NVTX3EXT_MEM_FN11_retCmd
#define NVTX3EXT_MEM_FN11_def
#define NVTX3EXT_MEM_FN11_global_idx 11
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemPermissionsReset (NVTX3EXT_MEM_FN11_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)

#define NVTX3EXT_MEM_FN12_name nvtxMemPermissionsBind
#define NVTX3EXT_MEM_FN12_sig nvtxDomainHandle_t domain,nvtxMemPermissionsHandle_t permissions,uint32_t bindScope, uint32_t bindFlags
#define NVTX3EXT_MEM_FN12_args domain,permissions,bindScope,bindFlags
#define NVTX3EXT_MEM_FN12_retType void
#define NVTX3EXT_MEM_FN12_retCmd
#define NVTX3EXT_MEM_FN12_def
#define NVTX3EXT_MEM_FN12_global_idx 12
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemPermissionsBind (NVTX3EXT_MEM_FN12_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)
 
#define NVTX3EXT_MEM_FN13_name nvtxMemPermissionsUnbind
#define NVTX3EXT_MEM_FN13_sig nvtxDomainHandle_t domain, uint32_t bindScope 
#define NVTX3EXT_MEM_FN13_args domain,bindScope
#define NVTX3EXT_MEM_FN13_retType void
#define NVTX3EXT_MEM_FN13_retCmd
#define NVTX3EXT_MEM_FN13_def
#define NVTX3EXT_MEM_FN13_global_idx 13
#define NVTX3EXT_MEM_SEG_IDX_nvtxMemPermissionsUnbind (NVTX3EXT_MEM_FN13_global_idx-NVTX3EXT_MEM_FN_IDX_FIRST)

/*
13-15 in nvtExtImplMemCudaRt1.h
*/

#define NVTX3EXT_MEM_FN_IDX_LAST 16
#define NVTX3EXT_MEM_FN_IDX_COUNT (NVTX3EXT_MEM_FN_IDX_LAST-NVTX3EXT_MEM_FN_IDX_FIRST+1)


#define NVTX3EXT_MEM_FUNCTIONS( MACRO ) \
MACRO ( 1 ) \
MACRO ( 2 ) \
MACRO ( 3 ) \
MACRO ( 4 ) \
MACRO ( 5 ) \
MACRO ( 6 ) \
MACRO ( 7 ) \
MACRO ( 8 ) \
MACRO ( 9 ) \
MACRO ( 10 ) \
MACRO ( 11 ) \
MACRO ( 12 ) \
MACRO ( 13 )

/*
14-16 in nvtExtImplMemCudaRt1.h
*/

#if defined NVTX_DISABLE

#define NVTX3EXT_MEM_IMPL_WRAPPER_DEFINE(ID) \
NVTX3EXT_MEM_FN(ID,_retType) NVTX3EXT_MEM_FN(ID, _name)  ( NVTX3EXT_MEM_FN(ID,_sig) ) \
{ \
    NVTX3EXT_MEM_FN(ID, _retCmd)  NVTX3EXT_MEM_FN(ID,_def); \
}

#else  /*NVTX_DISABLE*/

#define NVTX3EXT_MEM_IMPL_FNPTR_TYPEDEF(ID) \
    typedef NVTX3EXT_MEM_FN(ID,_retType) (NVTX_API * NVTX3EXT_MEM_FN##ID##_impl_fntype )( NVTX3EXT_MEM_FN(ID,_sig) );

#define NVTX3EXT_MEM_IMPL_WRAPPER_DEFINE(ID) \
NVTX3EXT_MEM_FN(ID,_retType) NVTX3EXT_MEM_FN(ID, _name)  ( NVTX3EXT_MEM_FN(ID,_sig) ) \
{ \
    while(1){ \
        intptr_t slot = NVTX_VERSIONED_IDENTIFIER(nvtxExtGlobals1).slots[ NVTX3EXT_MEM_FN##ID##_global_idx ]; \
        if(slot & ~NVTX_EXTENSION_DISABLED){ \
            return (*(NVTX3EXT_MEM_FN##ID##_impl_fntype)slot) ( NVTX3EXT_MEM_FN(ID,_args) ); \
        } else if(slot==NVTX_EXTENSION_DISABLED) { \
            return NVTX3EXT_MEM_FN(ID,_def); \
        } else { \
            NVTXMEM_VERSIONED_IDENTIFIER(nvtxExtMemInitOnce)(); \
        } \
    } \
}


NVTX_LINKONCE_DEFINE_FUNCTION intptr_t NVTX_API NVTXMEM_VERSIONED_IDENTIFIER(nvtxExtMemGetExportFunction)(uint32_t exportFunctionId)
{
    (void)exportFunctionId;
    return 0;
}

NVTX_LINKONCE_DEFINE_FUNCTION void NVTXMEM_VERSIONED_IDENTIFIER(nvtxExtMemInitOnce)()
{
    nvtxExtModuleSegment_t segment = { 
        NVTX_EXT_SEGMENT_MEM_CORE1,
        NVTX3EXT_MEM_FN_IDX_COUNT, 
        NVTX3EXT_MEM_GLOBALS.slots+NVTX3EXT_MEM_FN_IDX_FIRST
    };

    nvtxExtModuleInfo_t module = {
        NVTX_VERSION,
        sizeof(nvtxExtModuleInfo_t),
        NVTX_EXT_MODULEID_MEM,
        NVTX_EXT_COMPATID_MEM,
        1,
        &segment,
        NVTXMEM_VERSIONED_IDENTIFIER(nvtxExtMemGetExportFunction)
    };

    NVTX_INFO( "%s\n", __FUNCTION__  );

    NVTX_VERSIONED_IDENTIFIER(nvtxExtInitOnce)(
        &module,
        NVTX3EXT_MEM_GLOBALS.slots+NVTX3EXT_MEM_INIT);
}

NVTX3EXT_MEM_FUNCTIONS( NVTX3EXT_MEM_IMPL_FNPTR_TYPEDEF )

#endif /* NVTX_DISABLE */

NVTX3EXT_MEM_FUNCTIONS( NVTX3EXT_MEM_IMPL_WRAPPER_DEFINE )


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
