
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

#include "DynUntrustedAllocPost.h"

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
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ModuleSlotTracker.h"
#include "llvm/IR/Type.h"
#include "llvm/InitializePasses.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Local.h"

#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#define DEBUG_TYPE "dyn-untrusted"

using namespace llvm;

static cl::opt<std::string>
    MPKTestProfilePath("mpk-test-profile-path", cl::init(""), cl::Hidden,
                       cl::value_desc("filename"),
                       cl::desc("Specify the path of profile data file. This is"
                                "mainly for test purpose."));

static cl::opt<bool>
    MPKTestRemoveHooks("mpk-test-remove-hooks", cl::init(false), cl::Hidden,
                       cl::desc("Remove hook instructions. This is mainly"
                                "for test purpose."));

static cl::opt<bool> MPKVerbosePatching(
    "mpk-verbose-patching", cl::init(false), cl::Hidden,
    cl::desc("Print out patched instructions on instrumentation pass."));

#ifdef MPK_STATS
// Ensure we assign a local ID to the same number of hooks as we made in the
// Pre pass.
uint64_t total_hooks = 0;
// Count the number of modified Alloc instructions
uint64_t modified_inst_count = 0;
#endif

enum HookIndex {
  allocHookIndex = 2,
  reallocHookIndex = 4,
  deallocHookIndex = -1 /*2*/
};

/// A mapping between hook function and the position of the localID argument.
// Note : Changed DeallocHook from 2 (correct position for index) to
// -1 to indicate we dont want to number this hook anymore. This is
// to increase stability of mapping between profiling and instrumentation
// builds. DeallocHook does not use the localID in any meaningful way.
// TODO : Remove localID from deallocHook.
const static std::map<std::string, HookIndex> patchArgIndexMap = {
    {"allocHook", allocHookIndex},
    {"reallocHook", reallocHookIndex},
    {"deallocHook", deallocHookIndex}};

// Currently only patching __rust_alloc and __rust_alloc_zeroed
const static std::map<std::string, std::string> AllocReplacementMap = {
    {"__rust_alloc", "__rust_untrusted_alloc"},
    {"trusted_malloc", "untrusted_malloc"},
    {"__rust_alloc_zeroed", "__rust_untrusted_alloc_zeroed"},
};

/// Map for counting total number of hooks split between type.
static std::map<std::string, int> hookCountMap = {
    {"allocHook", 0}, {"reallocHook", 0}, {"deallocHook", 0}};

std::vector<Instruction *> hookList;
std::vector<CallBase *> patchList;

class IDGenerator {
  uint64_t id;

public:
  IDGenerator() : id(0) {}

  ConstantInt *getConstID(Module &M) {
    return llvm::ConstantInt::get(IntegerType::getInt64Ty(M.getContext()),
                                  id++);
  }

  ConstantInt *getConstIntCount(Module &M) {
    return llvm::ConstantInt::get(IntegerType::getInt64Ty(M.getContext()), id);
  }
};

static IDGenerator IDG;

PreservedAnalyses ProvsanPost::run(Module &M, ModuleAnalysisManager &MAM) {

  llvm::errs() << "ProvsanPost Pass Running ...\n";
  // Additional flags for easier testing with opt.
  if (MPKProfilePath.empty() && !MPKTestProfilePath.empty())
    MPKProfilePath = MPKTestProfilePath;
  if (MPKTestRemoveHooks)
    RemoveHooks = MPKTestRemoveHooks;

  // Post inliner pass, iterate over all functions and find hook CallSites.
  // Assign a unique local ID in a deterministic pattern to ensure localID is
  // consistent between runs.
  assignLocalIDs(M);

  if (!MPKProfilePath.empty()) {
    for (auto *allocSite : patchList) {
      patchInstruction(M, allocSite);
    }
  }

  if (RemoveHooks)
    removeHooks(M);

#ifdef MPK_STATS
  printStats(M);

  // If MPK_STATS is enables, then we create a global containing the value of
  // the total number of allocation sites
  GlobalVariable *AllocSiteTotal = cast<GlobalVariable>(M.getOrInsertGlobal(
      "AllocSiteTotal", IntegerType::getInt64Ty(M.getContext())));
  AllocSiteTotal->setInitializer(IDG.getConstIntCount(M));
#endif
  LLVM_DEBUG(errs() << "DynUntrustedPost finish.\n");
  return PreservedAnalyses::none();
}

// Reference for this function can be found in the LLVM JSON support
// library documentation, `llvm/include/llvm/Support/JSON.h` titled:
// "Converting JSON values to C++ types". This describes a function:
// fromJSON(const json::Value&, T&) -> bool
// which passes the JSON object as its first argument and a reference
// to the data structure you are deserializing as the second argument.
// The boolean return is in reference to the stability of the deserialized
// object and should be handled by the caller.
bool fromJSON(const llvm::json::Value &Alloc, FaultingSite &F) {
  llvm::json::Path::Root Root("FaultingSite");
  llvm::json::ObjectMapper O(Alloc, Root);

  int64_t temp_id;
  bool temp_id_result = O.map("id", temp_id);
  if (temp_id < 0)
    return false;

  int64_t temp_pkey;
  bool temp_pkey_result = O.map("pkey", temp_pkey);
  if (temp_pkey < 0)
    return false;

  std::string temp_bbName;
  bool temp_bbName_result = O.map("bbName", temp_bbName);
  if (temp_bbName.empty())
    return false;

  std::string temp_funcName;
  bool temp_funcName_result = O.map("funcName", temp_funcName);
  if (temp_funcName.empty())
    return false;

  F.localID = static_cast<uint64_t>(temp_id);
  F.pkey = static_cast<uint32_t>(temp_pkey);
  F.bbName = temp_bbName;
  F.funcName = temp_funcName;

  return O && temp_id_result && temp_pkey_result && temp_bbName_result &&
         temp_funcName_result;
}

std::vector<std::string> ProvsanPost::getFaultPaths() {
  std::vector<std::string> fault_files;
  if (llvm::sys::fs::is_directory(MPKProfilePath)) {
    std::error_code EC;
    for (llvm::sys::fs::directory_iterator F(MPKProfilePath, EC), E;
         F != E && !EC; F.increment(EC)) {
      auto file_extension = llvm::sys::path::extension(F->path());
      if (StringSwitch<bool>(file_extension.lower())
              .Case(".json", true)
              .Default(false)) {
        fault_files.push_back(F->path());
      }
    }
  } else {
    fault_files.push_back(MPKProfilePath);
  }

  return fault_files;
}

Optional<json::Array>
ProvsanPost::parseJSONArrayFile(llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> File) {
  std::error_code ec = File.getError();
  if (ec) {
    LLVM_DEBUG(errs() << "File could not be read: " << ec.message() << "\n");
    return None;
  }

  Expected<json::Value> ParseResult =
      json::parse(File.get().get()->getBuffer());
  if (Error E = ParseResult.takeError()) {
    LLVM_DEBUG(errs() << "Failed to Parse JSON array: " << E << "\n");
    consumeError(std::move(E));
    return None;
  }

  if (!ParseResult->getAsArray()) {
    LLVM_DEBUG(errs() << "Failed to get JSON Value as JSON array.\n");
    return None;
  }

  return *ParseResult->getAsArray();
}

std::map<std::string, std::map<uint64_t, FaultingSite>>
ProvsanPost::getFaultingAllocMap() {
  std::map<std::string, std::map<uint64_t, FaultingSite>> fault_map;
  // If no path provided, return empty map.
  if (MPKProfilePath.empty())
    return fault_map;

  for (std::string path : getFaultPaths()) {
    auto ParseResult = parseJSONArrayFile(MemoryBuffer::getFile(path));
    if (!ParseResult) {
      errs() << "Error : Failed to parse file at path: " << path << "\n";
      continue;
    }

    for (const auto &Alloc : ParseResult.getValue()) {
      FaultingSite FS;
      if (fromJSON(Alloc, FS)) {
        auto iter = fault_map.find(FS.funcName);
        if (iter == fault_map.end()) {
          fault_map.emplace(FS.funcName, std::map<uint64_t, FaultingSite>());
          iter = fault_map.find(FS.funcName);
        }
        iter->second.emplace(FS.localID, FS);
      } else {
        errs() << "Error getting Allocation Site: " << Alloc << "\n";
      }
    }
  }

  LLVM_DEBUG(errs() << "Returning successful fault_map.\n");
  return fault_map;
}

static bool funcSort(Function *F1, Function *F2) {
  return F1->getName().str() > F2->getName().str();
}

void ProvsanPost::assignLocalIDs(Module &M) {
  std::vector<Function *> WorkList;
  for (Function &F : M) {
    if (!F.isDeclaration())
      WorkList.push_back(&F);
  }

  std::sort(WorkList.begin(), WorkList.end(), funcSort);

  LLVM_DEBUG(errs() << "Search for modified functions!\n");

  auto fault_map = getFaultingAllocMap();

  // Note on ModuleSlotTracker:
  // The MST is used for "naming" BasicBlocks that do not already
  // have a name by getting the module slot associated with a
  // BasicBlock in a given function. From testing and documentation,
  // it appears as though BasicBlocks almost never have names (especially
  // when building optimized binaries).
  //
  // The code for getting the BasicBlock numbers (and building names)
  // is taken from llvm/lib/CodeGen/MIRPrinter.cpp functions:
  // - void MIRPrinter::print(const MachineFunction &MF) and
  // - void MIRPrinter::print(const MachineBasicBlock &MBB)
  ModuleSlotTracker MST(&M, /*shouldInitializeAllMetaData*/ false);

  for (Function *F : WorkList) {
    MST.incorporateFunction(*F);
    ReversePostOrderTraversal<Function *> RPOT(F);
    IDGenerator LocalIDG;
    std::string funcName = F->getName().str();
    auto func_fault_iter = fault_map.find(funcName);

    for (BasicBlock *BB : RPOT) {
      for (Instruction &I : *BB) {
        CallBase *CS = dyn_cast<CallBase>(&I);
        if (!CS) {
          continue;
        }

        Function *hook = CS->getCalledFunction();
        if (!hook)
          continue;

        // Get patch index from map.
        auto index_iter = patchArgIndexMap.find(hook->getName().str());
        if (index_iter == patchArgIndexMap.end())
          continue;
#ifdef MPK_STATS
        auto hookCounter = hookCountMap.find(hook->getName().str());
        if (hookCounter != hookCountMap.end())
          hookCounter->second++;
#endif

        auto index = index_iter->second;
        Instruction *callInst = cast<Instruction>(CS);

#ifdef MPK_STATS
        ++total_hooks;
#endif

        if (RemoveHooks)
          hookList.push_back(callInst);

        // If index == deallocHookIndex, then this is a deallocHook. We can
        // skip the rest of the code since we know we dont need to patch this
        // call and we dont want it to be part of the count either.
        if (index == deallocHookIndex)
          continue;

        // Get (or make) BasicBlock name
        std::string bbName;
        if (BB->getName().str().empty()) {
          bbName = "block" + std::to_string(MST.getLocalSlot(BB));
        } else {
          bbName = BB->getName().str();
        }

        // Set LocalID for hook function
        auto id = LocalIDG.getConstID(M);
        CS->setArgOperand(index, id);
        if (!RemoveHooks) {
          // We only want to create these global strings if they are going to
          // be used in final program execution. When removing the hooks, skip
          // creating (and assigning) the Global String identifiers.
          IRBuilder<> IRB(&*callInst);
          // Set the Basic Block name, which is at index + 1
          CS->setArgOperand(index + 1, IRB.CreateGlobalStringPtr(bbName));
          // Set the Function name, which is at index + 2
          CS->setArgOperand(index + 2, IRB.CreateGlobalStringPtr(funcName));
        }

        // If provided a valid path, modify given instruction
        if (!MPKProfilePath.empty()) {
          // Check to see if this function contains any faults
          if (func_fault_iter == fault_map.end()) {
            continue;
          }

          // Get Call Instr this hook references
          auto allocFunc = CS->getArgOperand(0);
          if (auto *allocInst = dyn_cast<CallBase>(allocFunc)) {

            // Check to see if ID is in fault map for patching
            auto &func_fault_map = func_fault_iter->second;
            auto map_iter = func_fault_map.find(id->getZExtValue());
            if (map_iter == func_fault_map.end()) {
              continue;
            }

            if (bbName.compare(map_iter->second.bbName) != 0) {
              errs() << "ERROR : Faulting allocation site found in "
                        "non-matching BasicBlock:\n"
                     << "AllocSite(" << map_iter->second.localID << ", "
                     << map_iter->second.funcName << ")\n"
                     << "TraceBlock(" << map_iter->second.bbName << ") -> "
                     << "InstrBlock(" << bbName << ")\n";
            }
            LLVM_DEBUG(errs() << "modified callsite:\n");
            LLVM_DEBUG(errs() << *CS << "\n");

            patchList.push_back(allocInst);
            PrintFaultingLocation(M, allocInst);
          } else {
            LLVM_DEBUG(errs()
                       << "Alloc Func expected, found: " << *allocFunc << "\n");
          }
        } else {
          llvm::errs() << "MPKProfilePath was empty\n";
        }
      }
    }
  }
}

void ProvsanPost::patchInstruction(Module &M, CallBase *inst) {
  auto calledFuncName = inst->getCalledFunction()->getName().str();
  auto repl_iter = AllocReplacementMap.find(calledFuncName);
  if (repl_iter == AllocReplacementMap.end())
    return;

  auto replacementFunctionName = repl_iter->second;

  if (MPKVerbosePatching)
    errs() << "Patching instruction: " << *inst << "\n";

  Function *repl_func = M.getFunction(replacementFunctionName);
  if (!repl_func) {
    LLVM_DEBUG(
        errs() << "ERROR while creating patch: Could not find replacement: "
               << replacementFunctionName << "\n");
    return;
  }

  inst->setCalledFunction(repl_func);
  LLVM_DEBUG(errs() << "Modified CallInstruction: " << *inst << "\n");
#ifdef MPK_STATS
  ++modified_inst_count;
#endif
}

// Working under the assumption that all missed cases of Hook calls is
// due to the blocks containing them no longer being reachable, we remove
// those instructions from their respective blocks.
void removeFunctionUsers(Function *F) {
  for (auto user : F->users()) {
    if (auto *inst = dyn_cast<Instruction>(user)) {
      salvageDebugInfo(*inst);
      inst->eraseFromParent();
#ifdef MPK_STATS
      total_hooks++;
      auto trackedHook = hookCountMap.find(F->getName().str());
      if (trackedHook != hookCountMap.end())
        trackedHook->second++;
#endif
    } else {
      errs() << "User not an instruction: " << *user << "\n";
      assert(false && "Function user not an instruction!");
    }
  }
  F->setLinkage(GlobalValue::LinkageTypes::InternalLinkage);
  F->eraseFromParent();
}

void ProvsanPost::getDiagMessage(raw_ostream &OS, const DebugLoc &Loc,
                                 bool first) const {
  auto *Scope = cast<DIScope>(Loc.getScope());

  bool hasInline = false;
  if (DebugLoc InlinedAtDL = Loc.getInlinedAt()) {
    hasInline = true;
  }

  if (!hasInline) {
    OS.changeColor(raw_ostream::Colors::YELLOW, true);
  }

  OS << Scope->getFilename();
  OS << ":" << Loc.getLine();
  if (Loc.getCol() != 0)
    OS << ":" << Loc.getCol();

  if (!hasInline) {
    OS.changeColor(raw_ostream::Colors::WHITE, false);
  }
  if (DebugLoc InlinedAtDL = Loc.getInlinedAt()) {
    OS << (first ? "" : " ]") << "\n\tInlined @[ ";
    getDiagMessage(OS, InlinedAtDL, false);
    OS << (first ? " ]" : "");
  }
}

void ProvsanPost::PrintFaultingLocation(Module &M, CallBase *inst) {
  auto &DI = inst->getDebugLoc();

  WithColor(errs(), HighlightColor::Error) << "Error";
  errs().changeColor(raw_ostream::Colors::WHITE, true)
      << ": Compartment Violation from memory originally allocated at ";
  errs().changeColor(raw_ostream::Colors::WHITE, false);
  getDiagMessage(errs(), DI, true);
  errs() << "\n";
}

void ProvsanPost::removeHooks(Module &M) {
  llvm::errs() << "RemoveHooks called. RemoveHooks = " << RemoveHooks << "\n";
  for (auto inst : hookList) {
    salvageDebugInfo(*inst);
    inst->eraseFromParent();
  }

  auto allocHook = M.getFunction("allocHook");
  if (allocHook)
    removeFunctionUsers(allocHook);

  auto reallocHook = M.getFunction("reallocHook");
  if (reallocHook)
    removeFunctionUsers(reallocHook);

  auto deallocHook = M.getFunction("deallocHook");
  if (deallocHook)
    removeFunctionUsers(deallocHook);
}

#ifdef MPK_STATS
void ProvsanPost::printStats(Module &M) {
  std::string TestDirectory = "TestResults";
  if (!llvm::sys::fs::is_directory(TestDirectory))
    llvm::sys::fs::create_directory(TestDirectory);

  llvm::Expected<llvm::sys::fs::TempFile> PreStats =
      llvm::sys::fs::TempFile::create(TestDirectory +
                                      "/static-post-%%%%%%%.stat");
  if (!PreStats) {
    LLVM_DEBUG(errs() << "Error making unique filename: "
                      << llvm::toString(PreStats.takeError()) << "\n");
    return;
  }

  llvm::raw_fd_ostream OS(PreStats->FD, /* shouldClose */ false);
  OS << "Number of alloc instructions modified to unsafe: "
     << modified_inst_count << "\n"
     << "Total number hooks given a LocalID: " << total_hooks << "\n"
     << "Total allocHooks: " << hookCountMap.find("allocHook")->second << "\n"
     << "Total reallocHooks: " << hookCountMap.find("reallocHook")->second
     << "\n"
     << "Total deallocHooks: " << hookCountMap.find("deallocHook")->second
     << "\n";
  OS.flush();

  if (auto E = PreStats->keep()) {
    LLVM_DEBUG(errs() << "Error keeping pre-stats file: "
                      << llvm::toString(std::move(E)) << "\n");
    return;
  }
}
#endif

// static RegisterPass<DynUntrustedAllocPost>
// X("dyn-untrusted-post",
//"DynUntrustedAlloc: Patch allocation sites with dynamic "
//"function hooks for tracking allocation IDs.",
// false, false);

// static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
//[](const PassManagerBuilder &Builder,
// legacy::PassManagerBase &PM) {
// PM.add(new DynUntrustedAllocPost());
//});

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ProvsanPost", LLVM_VERSION_STRING,
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
                  llvm::errs() << "ProvsanPost\n";
                  MPM.addPass(llvm::ProvsanPost());
                });
          }};
}
