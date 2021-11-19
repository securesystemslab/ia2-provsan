#include "alloc_site_handler.h"

extern "C" {
bool is_safe_address(void *addr) { return false; }
}

namespace __provsan {
#define DEFAULT_PKEY 0

AllocSiteHandler *AllocSiteHandle = nullptr;

std::once_flag AllocHandlerInitFlag;

AllocSite AllocSite::error() { return AllocSite(); }

void AllocSiteHandler::init() {
  AllocSiteHandle = new AllocSiteHandler();
  provsan_untrusted_constructor();
}

AllocSiteHandler *AllocSiteHandler::getOrInit() {
  std::call_once(AllocHandlerInitFlag, AllocSiteHandler::init);
  if (!AllocSiteHandle)
    REPORT("AllocSiteHandle is null!\n");
  return AllocSiteHandle;
}

} // namespace __provsan

extern "C" {
void allocHook(rust_ptr ptr, int64_t size, int64_t localID, const char *bbName,
               const char *funcName) {
  __provsan::AllocSite site(ptr, size, localID, bbName, funcName);
  auto handler = __provsan::AllocSiteHandler::getOrInit();
  handler->insertAllocSite(ptr, site);
  REPORT(
      "INFO : AllocSiteHook for address: %p ID: %d bbName: %s funcName: %s.\n",
      ptr, localID, bbName, funcName);

#ifdef MPK_STATS
  if (AllocSiteCount != 0)
    allocHookCalls++;
#endif
}

/// reallocHook will remove the previous mapping from oldPtr -> oldAllocSite,
/// and replace it with a mapping from newPtr -> newAllocSite<oldAllocSite>,
/// where the oldAllocSite is added as part of the set of associated allocations
/// for the new mapping.
void reallocHook(rust_ptr newPtr, int64_t newSize, rust_ptr oldPtr,
                 int64_t oldSize, int64_t localID, const char *bbName,
                 const char *funcName) {
  // Get the AllocSiteHandler and the old AllocSite for the associated oldPtr.
  auto handler = __provsan::AllocSiteHandler::getOrInit();
  auto oldAS = handler->getAllocSite(oldPtr);

  if (!oldAS.isValid()) {
    // Returned ErrorAlloc, which should not be part of the realloc chain.
    __provsan::AllocSite site(newPtr, newSize, localID, bbName, funcName);
    handler->insertAllocSite(newPtr, site);
    REPORT("ERROR<AllocSite> : Realloc Site: %p : %d could not find the "
           "previous allocation: %d\n",
           newPtr, site.id(), oldAS.id());
    return;
  }

  __provsan::AllocSite newAS(newPtr, newSize, localID, bbName, funcName,
                                   DEFAULT_PKEY, true);

  // Get the previously associated set from the site being re-allocated and
  // add the previous site to the associated set.
  handler->updateReallocChain(oldAS, newAS);

  // Remove previous Allocation Site from the mapping.
  handler->removeAllocSite(oldPtr);

  handler->insertAllocSite(newPtr, newAS);
  REPORT("INFO : ReallocSiteHook for oldptr: %p, newptr: %p, ID: %d bbName: %s "
         "funcName: %s.\n",
         oldPtr, newPtr, localID, bbName, funcName);

#ifdef MPK_STATS
  if (AllocSiteCount != 0)
    reallocHookCalls++;
#endif
}

void deallocHook(rust_ptr ptr, int64_t size, int64_t localID) {
  auto handler = __provsan::AllocSiteHandler::getOrInit();
  handler->removeAllocSite(ptr);
  REPORT("INFO : DeallocSiteHook for address: %p ID: %d.\n", ptr, localID);

#ifdef MPK_STATS
  if (AllocSiteCount != 0)
    deallocHookCalls++;
#endif
}
} // end extern "C"
