#define NVTX_NO_IMPL
#include "nvtx3/nvToolsExt.h"

#ifdef ENABLE_CUDA
#include "nvtx3/nvToolsExtCuda.h"
#endif

#ifdef ENABLE_CUDART
#include "nvtx3/nvToolsExtCudaRt.h"
#endif

#include "nvtx3/nvToolsExtSync.h"
