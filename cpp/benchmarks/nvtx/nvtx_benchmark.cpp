
#include <benchmark/benchmark.h>

#include <nvtx3.hpp>

static void BM_CXX_global_range(::benchmark::State& state)
{
  nvtx3::event_attributes attr {};
  for (auto _ : state) { nvtx3::thread_range r{attr}; }
}
BENCHMARK(BM_CXX_global_range);

static void BM_C_global_range(::benchmark::State& state)
{
  nvtx3::event_attributes attr {};
  for (auto _ : state) {
    nvtxDomainRangePushEx(nullptr, attr.get());
    nvtxDomainRangePop(nullptr);
  }
}
BENCHMARK(BM_C_global_range);
