/*
 * Copyright (c) 2020, NVIDIA CORPORATION.
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
 */

#include <gtest/gtest.h>

#include <nvToolsExtSync.h>
#include <nvtx3.hpp>

#include <cupti.h>
#include <generated_nvtx_meta.h>

#include <iostream>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

template <CUpti_CallbackId cbid>
struct params {};

/**
 * @brief Maps an NVTX API's CUPTI callback ID to it's corresponding function
 * parameters type.
 * 
 */
#define PARAMS_TRAIT(NVTX_API)                \
  template <>                                 \
  struct params<CUPTI_CBID_NVTX_##NVTX_API> { \
    using type = NVTX_API##_params;           \
  };

PARAMS_TRAIT(nvtxMarkA);
PARAMS_TRAIT(nvtxMarkW)
PARAMS_TRAIT(nvtxMarkEx)
PARAMS_TRAIT(nvtxRangeStartA)
PARAMS_TRAIT(nvtxRangeStartW)
PARAMS_TRAIT(nvtxRangeStartEx)
PARAMS_TRAIT(nvtxRangeEnd)
PARAMS_TRAIT(nvtxRangePushA)
PARAMS_TRAIT(nvtxRangePushW)
PARAMS_TRAIT(nvtxRangePushEx)
PARAMS_TRAIT(nvtxRangePop)
PARAMS_TRAIT(nvtxNameCategoryA)
PARAMS_TRAIT(nvtxNameCategoryW)
PARAMS_TRAIT(nvtxNameOsThreadA)
PARAMS_TRAIT(nvtxNameOsThreadW)
PARAMS_TRAIT(nvtxNameCuDeviceA)
PARAMS_TRAIT(nvtxNameCuDeviceW)
PARAMS_TRAIT(nvtxNameCuContextA)
PARAMS_TRAIT(nvtxNameCuContextW)
PARAMS_TRAIT(nvtxNameCuStreamA)
PARAMS_TRAIT(nvtxNameCuStreamW)
PARAMS_TRAIT(nvtxNameCuEventA)
PARAMS_TRAIT(nvtxNameCuEventW)
PARAMS_TRAIT(nvtxNameCudaDeviceA)
PARAMS_TRAIT(nvtxNameCudaDeviceW)
PARAMS_TRAIT(nvtxNameCudaStreamA)
PARAMS_TRAIT(nvtxNameCudaStreamW)
PARAMS_TRAIT(nvtxNameCudaEventA)
PARAMS_TRAIT(nvtxNameCudaEventW)
PARAMS_TRAIT(nvtxDomainMarkEx)
PARAMS_TRAIT(nvtxDomainRangeStartEx)
PARAMS_TRAIT(nvtxDomainRangeEnd)
PARAMS_TRAIT(nvtxDomainRangePushEx)
PARAMS_TRAIT(nvtxDomainRangePop)
PARAMS_TRAIT(nvtxDomainRegisterStringA)
PARAMS_TRAIT(nvtxDomainRegisterStringW)
PARAMS_TRAIT(nvtxDomainCreateA)
PARAMS_TRAIT(nvtxDomainDestroy)
//PARAMS_TRAIT(nvtxDomainResourceCreate)
//PARAMS_TRAIT(nvtxDomainResourceDestroy)
//PARAMS_TRAIT(nvtxDomainNameCategoryA)
//PARAMS_TRAIT(nvtxDomainNameCategoryW)
//PARAMS_TRAIT(nvtxDomainCreateW)
//PARAMS_TRAIT(nvtxDomainSyncUserCreate)
//PARAMS_TRAIT(nvtxDomainSyncUserDestroy)
//PARAMS_TRAIT(nvtxDomainSyncUserAcquireStart)
//PARAMS_TRAIT(nvtxDomainSyncUserAcquireFailed)
//PARAMS_TRAIT(nvtxDomainSyncUserAcquireSuccess)
//PARAMS_TRAIT(nvtxDomainSyncUserReleasing)

#define NVTX_DISPATCH_CASE(call_back_id) \
  case call_back_id:                     \
    return f.template operator()<call_back_id>();

/**
 * @brief Dispatches a CUPTI callback ID from the NVTX domain as a non-type
 * template argument to a callable `f`.
 *
 * @tparam F Type of the callable
 * @param cbid The callback ID to dispatch
 * @param f The callable to invoke with `cbid` as the non-type template argument
 * @return Whatever `f` returns
 */
template <typename F>
auto dispatch_nvtx_callback_id(CUpti_CallbackId cbid, F f) {
  switch (cbid) {
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_INVALID)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxMarkA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxMarkW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxMarkEx)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxRangeStartA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxRangeStartW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxRangeStartEx)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxRangeEnd)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxRangePushA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxRangePushW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxRangePushEx)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxRangePop)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCategoryA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCategoryW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameOsThreadA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameOsThreadW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCuDeviceA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCuDeviceW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCuContextA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCuContextW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCuStreamA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCuStreamW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCuEventA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCuEventW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCudaDeviceA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCudaDeviceW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCudaStreamA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCudaStreamW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCudaEventA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxNameCudaEventW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainMarkEx)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainRangeStartEx)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainRangeEnd)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainRangePushEx)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainRangePop)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainResourceCreate)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainResourceDestroy)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainNameCategoryA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainNameCategoryW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainRegisterStringA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainRegisterStringW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainCreateA)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainCreateW)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainDestroy)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainSyncUserCreate)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainSyncUserDestroy)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainSyncUserAcquireStart)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainSyncUserAcquireFailed)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainSyncUserAcquireSuccess)
    NVTX_DISPATCH_CASE(CUPTI_CBID_NVTX_nvtxDomainSyncUserReleasing)
  }
}

struct print_id {
  template <CUpti_CallbackId cbid>
  void operator()() {
    std::cout << cbid << std::endl;
  }
};

void CUPTIAPI nvtx_callback(void *userdata, CUpti_CallbackDomain domain,
                            CUpti_CallbackId cbid, const void *cbdata) {
  std::cout << "my callback\n";

  if (domain == CUPTI_CB_DOMAIN_NVTX) {
    const CUpti_NvtxData *nvtxInfo = (CUpti_NvtxData *)cbdata;
    dispatch_nvtx_callback_id(cbid, print_id{});
  }
}

struct NVTX_Test : public ::testing::Test {
  NVTX_Test() {  // Get path to `libcupti.so` from the `CUPTI_PATH`
    // definition specified as a
    // compile argument
    constexpr char const *cupti_path = TOSTRING(CUPTI_PATH);

    // Inject CUPTI into NVTX
    setenv("NVTX_INJECTION64_PATH", cupti_path, 1);

    // Register `nvtx_callback` to be invoked for all NVTX APIs
    CUpti_SubscriberHandle subscriber;
    cuptiSubscribe(&subscriber, (CUpti_CallbackFunc)nvtx_callback, nullptr);
    cuptiEnableDomain(1, subscriber, CUPTI_CB_DOMAIN_NVTX);
  }
};

TEST_F(NVTX_Test, first) {
  nvtxRangePushA("test");
  nvtxRangePop();
}