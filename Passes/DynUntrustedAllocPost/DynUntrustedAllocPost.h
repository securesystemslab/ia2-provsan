//===- Transforms/UntrustedAlloc.h - UntrustedAlloc passes ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the constructor for the Dynamic Untrusted Allocation
// Post passes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_DYNAMIC_MPK_UNTRUSTED_POST_H
#define LLVM_TRANSFORMS_DYNAMIC_MPK_UNTRUSTED_POST_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/JSON.h"

#include <cstdlib>

// Used for printing compile time statistics for DynUntrustedAllocPost pass.
#define MPK_STATS

namespace llvm {
class ModulePass;

struct FaultingSite {
  uint64_t localID;
  uint32_t pkey;
  std::string bbName;
  std::string funcName;
};

/// Pass to patch all hook instructions after the inliner has run with
/// UniqueIDs. When supplied with a patch list (in the format of JSON file)
/// from previous runs, it will also patch allocation sites to be
/// untrusted.
class ProvsanPost : public PassInfoMixin<ProvsanPost> {
public:
  ProvsanPost(std::string mpk_profile_path = "", bool remove_hooks = false)
      : MPKProfilePath(mpk_profile_path), RemoveHooks(remove_hooks) {

    // errs() <<"Attempt to read PROVSAN_PATH from environment variable\n";
    if (MPKProfilePath.empty()) {
      const char *var = getenv("PROVSAN_PATH");
      if (!var)
        var = "";
      std::string tmp(var);
      MPKProfilePath = tmp;
    }

    // errs() <<"Attempt to read PROVSAN_HOOK from environment variable\n";
    if (!RemoveHooks) {
      const char *p = getenv("PROVSAN_HOOK");
      if (p)
        RemoveHooks = (bool)std::stoi(p);
    }
  }
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

private:
  std::vector<std::string> getFaultPaths();
  Optional<json::Array>
  parseJSONArrayFile(llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> File);

  void assignLocalIDs(Module &M);
  void patchInstruction(Module &M, CallBase *inst);
  void removeHooks(Module &M);
  void PrintFaultingLocation(Module &M, CallBase *inst);
  void getDiagMessage(raw_ostream &OS, const DebugLoc &Loc, bool first) const;

#ifdef MPK_STATS
  void printStats(Module &M);
#endif

  std::map<std::string, std::map<uint64_t, FaultingSite>> getFaultingAllocMap();

  std::string MPKProfilePath;
  bool RemoveHooks;
};

// ModulePass *createDynUntrustedAllocPostPass(std::string mpk_profile_path,
// bool remove_hooks);

// void initializeDynUntrustedAllocPostPass(PassRegistry &Registry);
} // namespace llvm

#endif // LLVM_TRANSFORMS_DYNAMIC_MPK_UNTRUSTED_POST_H
