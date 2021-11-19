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
// Pre passes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_DYNAMIC_MPK_UNTRUSTED_PRE_H
#define LLVM_TRANSFORMS_DYNAMIC_MPK_UNTRUSTED_PRE_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"

#define MPK_STATS

namespace llvm {

/// Pass to identify and add runtime hooks to all Rust alloc, realloc, and
/// dealloc calls. Additionally removes the NoInline attribute from functions
/// with the RustAllocator attribute.
class DynUntrustedAllocPre : public PassInfoMixin<DynUntrustedAllocPre> {
public:
  DynUntrustedAllocPre() {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);

private:
  void hookFunctions(Module &M, ModuleAnalysisManager &MAM);
  Instruction *getHookInst(Module &M, CallBase *CS);
  llvm::SmallPtrSet<Function *, 4>
  GetTargetFunctionSet(llvm::Module &M,
                       const llvm::cl::list<std::string> &targets);
#ifdef MPK_STATS
  void printStats(Module &M);
#endif

  Function *allocHook;
  Function *reallocHook;
  Function *deallocHook;

  llvm::SmallPtrSet<Function *, 4> AllocFunctions;
  llvm::SmallPtrSet<Function *, 4> ReallocFunctions;
  llvm::SmallPtrSet<Function *, 4> DeallocFunctions;
};

// void initializeDynUntrustedAllocPrePass(PassRegistry &Registry);
} // namespace llvm

#endif // LLVM_TRANSFORMS_DYNAMIC_MPK_UNTRUSTED_PRE_H
