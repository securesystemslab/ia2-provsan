#ifndef MPK_COMMON_H
#define MPK_COMMON_H

#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstring>

// Flag for controlling optional Stats tracking
#ifdef MPK_STATS
#include <atomic>

// Pointer to the global array tracking number of faults per allocation site
extern std::atomic<uint64_t> *AllocSiteUseCounter;
extern std::atomic<uint64_t> allocHookCalls;
extern std::atomic<uint64_t> reallocHookCalls;
extern std::atomic<uint64_t> deallocHookCalls;
extern std::atomic<uint64_t> AllocSiteCount;
#endif

#ifdef MPK_ENABLE_LOGGING
#define REPORT(...) fprintf(stderr, __VA_ARGS__)
#else
#define REPORT(...)                                                            \
  do {                                                                         \
  } while (0)
#endif

#endif // MPK_COMMON_H
