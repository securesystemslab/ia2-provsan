#ifndef ALLOCSITEHANDLER_H
#define ALLOCSITEHANDLER_H

#include "provsan_common.h"
#include "provsan_init.h"

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

typedef int8_t *rust_ptr;
extern "C" {
extern bool __attribute__((weak)) is_safe_address(void *addr);
}

namespace __provsan {

/**
 * @brief A class for tracking allocation metadata for a given allocation site
 * in target source code.
 *
 * @param ptr Pointer to allocated memory.
 * @param size Size of given allocation.
 * @param localID A function local identifier to track faulting allocations in
 * the runtime back to locations in source code.
 * @param bbName Name associated with containing basic block from source.
 * @param funcName Name associated with containing function from source.
 * @param pkey The Pkey that the given AllocationSite faulted on attempted
 * access.
 * @param isRealloc Simple marker for determining if an allocation site is
 * an alloc call or a realloc call. Mostly used for confirming results of
 * traces.
 *
 *
 * @note For each call to alloc (and realloc), an AllocSite will be created to
 * track the pointer of the allocation, the size of the allocation, and a
 * <localID, basicBlockName, funcName> tuple for tracking the call to alloc back
 * to its position in the source code. This information is intended to be used
 * in the compilation process for changing Allocation Sites that should be
 * untrusted to untrusted alloc calls.
 *
 * @note A note on thread safety: The only parameter that is changed at any
 * point after object creation is the PKey that the object faults on, thus this
 * should only be accessed behind a mutex. Currently the only modification to
 * this value is behind a mutex inside AllocSiteHandler::addFaultAlloc().
 */
class AllocSite {
private:
  rust_ptr ptr;
  int64_t size;
  int64_t localID;
  std::string bbName;
  std::string funcName;
  uint32_t pkey;
  bool isRealloc;
  AllocSite()
      : ptr(nullptr), size(-1), localID(-1), pkey(0), isRealloc(false) {}

public:
  AllocSite(rust_ptr ptr, int64_t size, int64_t localID, std::string bbName,
            std::string funcName, uint32_t pkey = 0, bool isRealloc = false)
      : ptr{ptr}, size{size}, localID{localID}, bbName{bbName},
        funcName{funcName}, pkey{pkey}, isRealloc{isRealloc} {
    assert(ptr != nullptr);
    assert(size > 0);
    assert(localID >= 0);
  }

  /// Returns an Error AllocSite.
  static AllocSite error();

  // Note : containsPtr contains potentially wrapping arithmetic. If a ptr
  // and the allocations size exceed max pointer size, then any pointer
  // searched for in the valid range will return False, as it cannot satisfy
  // both requirments in the check.
  bool containsPtr(rust_ptr ptrCmp) {
    // TODO : Note, might be important to cast pointers to uintptr_t type for
    // arithmetic comparisons if it behaves incorrectly.
    return (ptr <= ptrCmp) && (ptrCmp < (ptr + size));
  }

  int64_t id() const { return localID; }

  rust_ptr getPtr() const { return ptr; }

  bool isValid() { return (ptr != nullptr) && (size > 0) && (localID >= 0); }

  // When a given allocation site faults, we add the pkey that the request
  // faulted on and add it to the allocation site metadata to provide insight
  // into which compartment attempted to access the information.
  //
  // WARNING : This is inherently unsafe in a multithreaded environment, thus it
  // should only ever be called in the `addFaultAlloc` function where it is
  // protected behind AllocSiteHandler's mutex.
  void addPkey(uint32_t faultPkey) { pkey = faultPkey; }

  uint32_t getPkey() { return pkey; }

  std::string getBBName() const { return bbName; }

  std::string getFuncName() const { return funcName; }

  bool isReAlloc() { return isRealloc; }

  // Required for AllocSite to be hashable.
  bool operator==(const AllocSite &ac) const {
    return funcName.compare(ac.getFuncName()) == 0 &&
           bbName.compare(ac.getBBName()) == 0 && localID == ac.id();
  }
};

typedef pid_t thread_id;

/**
 * @brief PendingPKeyInfo tracks the pkey and access rights for a pending single
 * step instruction for a given thread (for use with our single stepping
 * approach).
 *
 * @param pkey Faulting PKey to be restored.
 * @param access_rights The previous access rights for the given PKey.
 */
struct PendingPKeyInfo {
public:
  uint32_t pkey;
  unsigned int access_rights;
  PendingPKeyInfo(uint32_t pkey, unsigned int access_rights)
      : pkey(pkey), access_rights(access_rights) {}
};

} // namespace __provsan

namespace std {
// Required to make AllocSite Hashable.
template <> struct hash<__provsan::AllocSite> {
  std::size_t operator()(const __provsan::AllocSite &AS) const {
    return ((std::hash<std::string>()(AS.getFuncName()) ^
             (std::hash<std::string>()(AS.getBBName()) << 1)) >>
            1) ^
           (std::hash<int64_t>()(AS.id()) << 1);
  }
};

} // namespace std

namespace __provsan {

/**
 * @brief A Class that handles mapping of pointers to allocation sites,
 * collecting the set of faulted allocation sites, and tracking PendingPKeyInfo
 * in multi-threaded single step environments.
 *
 * @param allocation_map Maps the pointer result from an alloc or realloc call
 * to its Allocation Site metadata.
 * @param fault_set Contains the set of faulted Allocation Sites.
 * @param pkey_by_tid_map Maps a given thread-id to its PendingPKeyInfo.
 * @param FM Maps Allocation Sites to their associated set.
 *
 * @note AllocSiteHandler is accessed through a global pointer so that
 * all threads access the same handler and data can be synchronized between
 * threads. To handle synchronization and allow for concurrent operations on the
 * separate data fields, each of the listed parameters above have an associated
 * mutex.
 */
class AllocSiteHandler {
  using alloc_set_t = std::unordered_set<AllocSite>;
  using realloc_map_t = std::unordered_map<AllocSite, alloc_set_t>;

private:
  // Mapping from memory location pointer to AllocationSite
  std::map<rust_ptr, AllocSite> allocation_map;
  // allocation_map mutex
  std::mutex alloc_map_mx;
  // Set of faulting AllocationSites
  alloc_set_t fault_set;
  // Fault set mutex
  std::mutex fault_set_mx;
  // Mapping of thread-id to saved pkey information
  std::unordered_map<thread_id, PendingPKeyInfo> pkey_by_tid_map;
  // pkey_by_tid_map mutex
  std::mutex pkey_tid_map_mx;

  // Map AllocSites to their reallocation chain
  realloc_map_t FM;
  // FM mutex
  std::mutex realloc_map_mx;

public:
  AllocSiteHandler() = default;
  ~AllocSiteHandler() {}

  static void init();
  static AllocSiteHandler *getOrInit();

  bool empty() { return allocation_map.empty(); }

  void insertAllocSite(rust_ptr ptr, AllocSite site) {
    // First, obtain the mutex lock to ensure safe addition of item to map.
    const std::lock_guard<std::mutex> alloc_map_guard(alloc_map_mx);

    // Insert AllocationSite for given ptr.
    allocation_map.emplace(ptr, site);
  }

  void removeAllocSite(rust_ptr ptr) {
    // Obtain mutex lock.
    const std::lock_guard<std::mutex> alloc_map_guard(alloc_map_mx);

    // Remove AllocationSite for given ptr.
    allocation_map.erase(ptr);
  }

  AllocSite getAllocSite(rust_ptr ptr) {
    // Obtain mutex lock.
    const std::lock_guard<std::mutex> alloc_map_guard(alloc_map_mx);

    if (allocation_map.empty()) {
      REPORT("INFO : Map is empty, returning error.\n");
      return AllocSite::error();
    }

    // Get AllocSite found from given rust_ptr
    auto map_iter = allocation_map.lower_bound(ptr);

    // First check to see if we found an exact match.
    if (map_iter != allocation_map.end()) {
      // Found valid iterator, check for exact match first
      if (map_iter->first == ptr) {
        // For an exact match, we can return the found allocation site
        return map_iter->second;
      }
    }

    // If it was not an exact match (or iterator was at map.end()), check
    // previous node to see if it is contained within valid range.
    if (map_iter != allocation_map.begin())
      --map_iter;

    if (map_iter->second.containsPtr(ptr))
      return map_iter->second;

    // If pointer was not an exact match, was not the beginning node,
    // and was not the node before the returned result of lower_bound,
    // then item is not contained within map. Return error node.
    REPORT("INFO : Returning AllocSite::error()\n");
    return AllocSite::error();
  }

  // Add a faulting allocation site to the fault_set with the given pkey.
  void addFaultAlloc(rust_ptr ptr, uint32_t pkey) {
    auto alloc = getAllocSite(ptr);
    REPORT("INFO : Getting AllocSite : id(%ld), ptr(%p)\n", alloc.id(),
           alloc.getPtr());

    // Ensure that the allocation site exists and an Error Allocation Site was
    // not returned.
    if (!alloc.isValid()) {
      REPORT("INFO : AllocSite is not valid, will not add it to Fault Set.\n");
      return;
    }

#ifdef MPK_STATS
    if (AllocSiteCount != 0) {
      // Increment the count of the allocation faulting
      assert((uint64_t)alloc.id() < AllocSiteCount && alloc.id() >= 0);
      AllocSiteUseCounter[alloc.id()]++;
    }
#endif

    // Add faulted allocation to fault_set
    const std::lock_guard<std::mutex> fault_set_insertion_guard(fault_set_mx);
    alloc.addPkey(pkey);
    fault_set.insert(alloc);

    // Note: no other code tries to take this lock and also lock the fault map
    // if that changes, the locking logic will need to be updated
    const std::lock_guard<std::mutex> guard(realloc_map_mx);
    auto it = FM.find(alloc);
    if (it == FM.end()) {
      return;
    }

    // For each Allocation Site in the associated set, add them to the fault_set
    // as well. Thus if a reallocated pointer faults, all associated allocation
    // sites are also marked as being unsafe.
    for (auto assoc : it->second) {
      assoc.addPkey(pkey);
      fault_set.insert(assoc);
#ifdef MPK_STATS
      if (AllocSiteCount != 0) {
        assert((uint64_t)assoc.id() < AllocSiteCount && assoc.id() >= 0);
        AllocSiteUseCounter[assoc.id()]++;
      }
#endif
    }
  }

  /// For single instruction stepping, this function will store a given PKey's
  /// permissions for a given thread-id
  void storePendingPKeyInfo(thread_id threadID, PendingPKeyInfo pkeyinfo) {
    // Obtain map key
    const std::lock_guard<std::mutex> pkey_map_guard(pkey_tid_map_mx);

    pkey_by_tid_map.emplace(threadID, pkeyinfo);
  }

  /// For single instruction stepping, this will get the associated PKey
  /// information for a given thread-id from the pkey_by_tid_map, then remove
  /// it from the mapping.
  std::optional<PendingPKeyInfo> getAndRemove(thread_id threadID) {
    // Obtain map key
    const std::lock_guard<std::mutex> pkey_map_guard(pkey_tid_map_mx);

    auto iter = pkey_by_tid_map.find(threadID);
    // If PID does not contain key in map, return None.
    if (iter == pkey_by_tid_map.end())
      return std::nullopt;

    auto ret_val = iter->second;
    pkey_by_tid_map.erase(threadID);
    return ret_val;
  }

  std::unordered_set<AllocSite> &faultingAllocs() {
    const std::lock_guard<std::mutex> fault_set_guard(fault_set_mx);
    return fault_set;
  }

  /// extend realloc chain for newAS with the realloc chain from oldAS
  void updateReallocChain(const AllocSite &oldAS, const AllocSite &newAS) {
    const std::lock_guard<std::mutex> guard(realloc_map_mx);
    auto it = FM.find(oldAS);
    if (it == FM.end()) {
      alloc_set_t AS;
      AS.emplace(oldAS);
      FM.emplace(newAS, AS);
      return;
    }

    alloc_set_t realloc_chain = it->second;
    realloc_chain.emplace(oldAS);
    FM.emplace(newAS, realloc_chain);
  }
};

} // namespace __provsan

extern "C" {
__attribute__((visibility("default"))) void
allocHook(rust_ptr ptr, int64_t size, int64_t localID, const char *bbName,
          const char *funcName);
__attribute__((visibility("default"))) void
reallocHook(rust_ptr newPtr, int64_t newSize, rust_ptr oldPtr, int64_t oldSize,
            int64_t localID, const char *bbName, const char *funcName);
__attribute__((visibility("default"))) void
deallocHook(rust_ptr ptr, int64_t size, int64_t localID);
}
#endif
