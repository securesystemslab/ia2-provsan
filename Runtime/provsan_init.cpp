#include "provsan_utils.h"
#include "provsan_common.h"
#include "provsan_fault_handler.h"
#include <cstdint>

struct sigaction *prevAction = nullptr;
struct sigaction *SEGVAction = nullptr;
struct sigaction *SIGTAction = nullptr;

#ifdef MPK_STATS
std::atomic<uint64_t> *AllocSiteUseCounter(nullptr);
std::atomic<uint64_t> allocHookCalls(0);
std::atomic<uint64_t> reallocHookCalls(0);
std::atomic<uint64_t> deallocHookCalls(0);
std::atomic<uint64_t> AllocSiteCount(0);
#endif

extern "C" {
void provsan_SEGV_fault_handler(void *oldact) {
  REPORT("INFO : Replacing SIGSEGV fault handler with "
         "__provsan::segMPKHandle.\n");
  if (!SEGVAction) {
    static struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = __provsan::pku_segv_handler;
    SEGVAction = &sa;
  }

  if (oldact != nullptr && prevAction != nullptr) {
    if (prevAction->sa_sigaction == __provsan::pku_segv_handler) {
      REPORT("ERROR : Attempting to copy segMPKHandle into oldact.\n");
    }
    memcpy(oldact, prevAction, sizeof(struct sigaction));
  }

  sigaction(SIGSEGV, SEGVAction, prevAction);
}

extern uint64_t __attribute__((weak)) AllocSiteTotal = 0;

/// Constructor will set up the segMPKHandle fault handler, and additionally
/// the stepMPKHandle when testing single stepping.
void provsan_untrusted_constructor() {
#ifdef MPK_STATS
  // If MPK_STATS is defined, grab the total allocation sites value and
  // initialize dynamic array. std::atomic should be 0 initialized according to
  // docs.
  if (AllocSiteTotal != 0) {
    AllocSiteUseCounter = new std::atomic<uint64_t>[AllocSiteTotal]();
  }
  AllocSiteCount = AllocSiteTotal;
#endif

  REPORT("INFO : Initializing and replacing segFaultHandler.\n");

  // Set up our fault handler
  static struct sigaction sa;
  static struct sigaction sa_old;
  memset(&sa, 0, sizeof(struct sigaction));
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = __provsan::pku_segv_handler;
  sigaction(SIGSEGV, &sa, &sa_old);
  if (!SEGVAction)
    SEGVAction = &sa;
  prevAction = &sa_old;

// If PAGE_MPK is not defined, default to Single Step
#ifndef PAGE_MPK
  // If we are single stepping, we add an additional signal handler.
  static struct sigaction sa_trap;

  sa_trap.sa_flags = SA_SIGINFO;
  sigemptyset(&sa_trap.sa_mask);
  sa_trap.sa_sigaction = __provsan::pku_trap_handler;
  sigaction(SIGTRAP, &sa_trap, nullptr);
  if (!SIGTAction)
    SIGTAction = &sa_trap;
#endif
}
}
