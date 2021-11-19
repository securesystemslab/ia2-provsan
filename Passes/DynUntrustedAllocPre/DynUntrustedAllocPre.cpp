//===-- UntrustedAlloc.cpp - UntrustedAlloc Infrastructure ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the common initialization infrastructure for the
// DynUntrustedAlloc library.
//
//===----------------------------------------------------------------------===//

#include "DynUntrustedAllocPre.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LazyCallGraph.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/InitializePasses.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <fstream>
#include <set>
#include <string>

#define DEBUG_TYPE "dyn-untrusted"
// Used for printing compile time statistics for DynUntrustedAllocPre pass.

namespace {
// simple helper that parses a delimited string into a vector
// used here to parse an environment variable into a list
std::vector<std::string> str_to_vec(std::string str, char delim) {
  std::vector<std::string> ret;
  size_t start;
  size_t end = 0;
  while ((start = str.find_first_not_of(delim, end)) != std::string::npos) {
    end = str.find(delim, start);
    ret.push_back(str.substr(start, end - start));
  }

  return ret;
}

} // namespace

namespace llvm {

static cl::list<std::string>
    ProvSanAlloc("provsan-alloc",
                 cl::desc("Specify the allocation symbol for trusted memory."),
                 cl::ZeroOrMore);

static cl::list<std::string> ProvSanRealloc(
    "provsan-realloc",
    cl::desc("Specify the re-allocation symbol for trusted memory."),
    cl::ZeroOrMore);

static cl::list<std::string>
    ProvSanFree("provsan-free",
                cl::desc("Specify the symbol used to free trusted memory."),
                cl::ZeroOrMore);

#ifdef MPK_STATS
// Tracker to count number of hook calls we create.
uint64_t hook_count = 0;

// Counters for tracking each type of hook separately
uint64_t alloc_hook_counter = 0;
uint64_t realloc_hook_counter = 0;
uint64_t dealloc_hook_counter = 0;
#endif

void populateFromEnv(cl::list<std::string> &ins, const char *env_var,
                     const char *fallback) {

  const char *var = getenv(env_var);
  if (!var)
    var = "";
  std::string a(var);
  if (a.empty()) {
    // errs() << "ProvSanAlloc = trusted_malloc\n";
    ins.push_back(fallback);
  } else {
    auto v = str_to_vec(a, ',');
    for (auto &s : v) {
      ins.push_back(s);
    }
  }
}

ConstantInt *getDummyID(Module &M) {
  return llvm::ConstantInt::get(IntegerType::getInt64Ty(M.getContext()), -1);
}

llvm::ConstantPointerNull *GlobalNullStr;

PreservedAnalyses DynUntrustedAllocPre::run(Module &M,
                                            ModuleAnalysisManager &MAM) {
  // cl::list cannot have an initial value so initialze them here
  populateFromEnv(ProvSanAlloc, "PROVSAN_ALLOC", "trusted_malloc");
  populateFromEnv(ProvSanRealloc, "PROVSAN_REALLOC", "trusted_realloc");
  populateFromEnv(ProvSanFree, "PROVSAN_FREE", "trusted_free");

  AllocFunctions = GetTargetFunctionSet(M, ProvSanAlloc);
  ReallocFunctions = GetTargetFunctionSet(M, ProvSanRealloc);
  DeallocFunctions = GetTargetFunctionSet(M, ProvSanFree);

  llvm::errs() << "ProvsanPre Pass Running ...\n";

  // Pre-inline pass:
  // Adds function hooks with dummy LocalIDs immediately after calls
  // to allocation functions. Additionally, we must remove the
  // NoInline attribute from RustAlloc functions.
  GlobalNullStr =
      llvm::ConstantPointerNull::get(Type::getInt8PtrTy(M.getContext()));

  AttrBuilder attrBldr;
  attrBldr.addAttribute(Attribute::NoUnwind);
  attrBldr.addAttribute(Attribute::ArgMemOnly);

  AttributeList fnAttrs = AttributeList::get(
      M.getContext(), AttributeList::FunctionIndex, attrBldr);

  // Make function hook to add to all functions we wish to track
  FunctionCallee allocHookFunc = M.getOrInsertFunction(
      "allocHook", fnAttrs,
      Type::getVoidTy(M.getContext()),         // void allocHook(
      Type::getInt8PtrTy(M.getContext()),      // (int8_t *)rust_ptr ptr,
      IntegerType::get(M.getContext(), 64),    // int64_t size,
      IntegerType::getInt64Ty(M.getContext()), // int64_t localID,
      Type::getInt8PtrTy(M.getContext()),      // const char *bbName,
      Type::getInt8PtrTy(M.getContext()));     // const char *funcName)
  allocHook = cast<Function>(allocHookFunc.getCallee());
  // set its linkage
  allocHook->setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);

  FunctionCallee reallocHookFunc = M.getOrInsertFunction(
      "reallocHook", fnAttrs,
      Type::getVoidTy(M.getContext()),         // void reallocHook(
      Type::getInt8PtrTy(M.getContext()),      // rust_ptr newPtr,
      IntegerType::get(M.getContext(), 64),    // int64_t newSize,
      Type::getInt8PtrTy(M.getContext()),      // rust_ptr oldPtr,
      IntegerType::get(M.getContext(), 64),    // int64_t oldSize,
      IntegerType::getInt64Ty(M.getContext()), // int64_t localID,
      Type::getInt8PtrTy(M.getContext()),      // const char *bbName,
      Type::getInt8PtrTy(M.getContext()));     // const char *funcName)
  reallocHook = cast<Function>(reallocHookFunc.getCallee());
  reallocHook->setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);

  FunctionCallee deallocHookFunc = M.getOrInsertFunction(
      "deallocHook", fnAttrs,
      Type::getVoidTy(M.getContext()),          // void deallocHook(
      Type::getInt8PtrTy(M.getContext()),       // rust_ptr ptr,
      IntegerType::get(M.getContext(), 64),     // int64_t size,
      IntegerType::getInt64Ty(M.getContext())); // int64_t localID)
  deallocHook = cast<Function>(deallocHookFunc.getCallee());
  deallocHook->setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);

  hookFunctions(M, MAM);

#ifdef MPK_STATS
  printStats(M);
#endif

  LLVM_DEBUG(errs() << "Finished DynUntrustedPre.\n");
  return PreservedAnalyses::none();
}

llvm::SmallPtrSet<Function *, 4> DynUntrustedAllocPre::GetTargetFunctionSet(
    llvm::Module &M, const cl::list<std::string> &targets) {
  llvm::SmallPtrSet<Function *, 4> ret;
  for (auto &name : targets) {
    auto *F = M.getFunction(name);
    // assert(F && "Target Function not in module!\n");
    ret.insert(F);
  }
  return ret;
}

Instruction *DynUntrustedAllocPre::getHookInst(Module &M, CallBase *CS) {
  Function *F = CS->getCalledFunction();
  if (!F)
    return nullptr;

  if (AllocFunctions.contains(F)) {
#ifdef MPK_STATS
    alloc_hook_counter++;
#endif
    return CallInst::Create((Function *)allocHook,
                            {CS, CS->getArgOperand(0), getDummyID(M),
                             GlobalNullStr, GlobalNullStr});
  } else if (ReallocFunctions.contains(F)) {
#ifdef MPK_STATS
    realloc_hook_counter++;
#endif
    return CallInst::Create((Function *)reallocHook,
                            {CS, CS->getArgOperand(3), CS->getArgOperand(0),
                             CS->getArgOperand(1), getDummyID(M), GlobalNullStr,
                             GlobalNullStr});
  } else if (DeallocFunctions.contains(F)) {
#ifdef MPK_STATS
    dealloc_hook_counter++;
#endif
    return CallInst::Create(
        (Function *)deallocHook,
        {CS->getArgOperand(0), CS->getArgOperand(1), getDummyID(M)});
  } else {
    return nullptr;
  }
}

void DynUntrustedAllocPre::hookFunctions(Module &M,
                                         ModuleAnalysisManager &MAM) {
  for (Function &F : M) {
    if (F.isDeclaration())
      continue;

    auto &FAM =
        MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

    ReversePostOrderTraversal<Function *> RPOT(&F);

    for (BasicBlock *BB : RPOT) {
      for (Instruction &I : *BB) {
        CallBase *CS = dyn_cast<CallBase>(&I);
        if (!CS)
          continue;

        Instruction *newHook = getHookInst(M, CS);
        if (!newHook)
          continue;

        BasicBlock::iterator NextInst;
        if (auto call = dyn_cast<CallInst>(&I)) {
          NextInst = ++I.getIterator();
          assert(NextInst != I.getParent()->end());
          LLVM_DEBUG(errs() << "CallInst(" << I
                            << ") found next iterator: " << *NextInst << "\n");
        } else if (auto invoke = dyn_cast<InvokeInst>(&I)) {
          BasicBlock *NormalDest = invoke->getNormalDest();
          if (!NormalDest->getSinglePredecessor()) {
            DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);
            auto BBNew = SplitEdge(invoke->getParent(), NormalDest, &DT);
            NextInst = BBNew->front().getIterator();
            LLVM_DEBUG(errs() << "InvokeInst(" << I
                              << ") with SplitEdge, found next iterator: "
                              << *NextInst << "\n");
          } else {
            NextInst = NormalDest->getFirstInsertionPt();
            assert(NextInst != NormalDest->end() &&
                   "Could not find insertion point for invoke instr");
            LLVM_DEBUG(errs() << "InvokeInst(" << I
                              << ") with single Pred, found next iterator: "
                              << *NextInst << "\n");
          }
        } else {
          continue;
        }

        errs() << "Inserting Hook\n";
        IRBuilder<> IRB(&*NextInst);
        IRB.Insert(newHook);
#ifdef MPK_STATS
        ++hook_count;
#endif
      }
    }
  }
}

#ifdef MPK_STATS
void DynUntrustedAllocPre::printStats(Module &M) {
  std::string TestDirectory = "TestResults";
  if (!llvm::sys::fs::is_directory(TestDirectory))
    llvm::sys::fs::create_directory(TestDirectory);

  llvm::Expected<llvm::sys::fs::TempFile> PreStats =
      llvm::sys::fs::TempFile::create(TestDirectory +
                                      "/static-pre-%%%%%%%.stat");
  if (!PreStats) {
    LLVM_DEBUG(errs() << "Error making unique filename: "
                      << llvm::toString(PreStats.takeError()) << "\n");
    return;
  }

  llvm::raw_fd_ostream OS(PreStats->FD, /* shouldClose */ false);
  OS << "Total number of hook instructions: " << hook_count << "\n"
     << "Number of alloc hook instructions: " << alloc_hook_counter << "\n"
     << "Number of realloc hook instructions: " << realloc_hook_counter << "\n"
     << "Number of dealloc hook instructions: " << dealloc_hook_counter << "\n";
  OS.flush();

  if (auto E = PreStats->keep()) {
    LLVM_DEBUG(errs() << "Error keeping pre-stats file: "
                      << llvm::toString(std::move(E)) << "\n");
    return;
  }
}
#endif

} // namespace llvm

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "DynUntrustedAllocPre", LLVM_VERSION_STRING,
          [](llvm::PassBuilder &PB) {
            using namespace llvm;
            using OptimizationLevel = typename PassBuilder::OptimizationLevel;
            // PB.registerPipelineParsingCallback(
            //[&](StringRef Name, ModulePassManager &MPM,
            // ArrayRef<PassBuilder::PipelineElement>) {
            // if (Name == "provsan-pre") {
            // MPM.addPass(llvm::DynUntrustedAllocPre());
            // return true;
            //}
            // return false;
            //});

            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel OL) {
                  llvm::errs() << "Register ProvsanPre\n";
                  MPM.addPass(llvm::DynUntrustedAllocPre());
                });
          }};
}
