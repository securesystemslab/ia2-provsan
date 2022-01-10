//===-- ProvSanCFGHash.cpp - CFGHash Infrastructure ---------------===//
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

#include "ProvSanCFGHash.h"
#include "llvm/include/llvm/Support/Endian.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Transforms/Instrumentation/CFGMST.h"
#include "llvm/Transforms/Instrumentation/ValueProfileCollector.h"

using namespace llvm;
using VPCandidateInfo = ValueProfileCollector::CandidateInfo;

/// The select instruction visitor plays three roles specified
/// by the mode. In \c VM_counting mode, it simply counts the number of
/// select instructions. In \c VM_instrument mode, it inserts code to count
/// the number times TrueValue of select is taken. In \c VM_annotate mode,
/// it reads the profile data and annotate the select instruction with metadata.
enum VisitMode { VM_counting, VM_instrument, VM_annotate };

// TODO : remove if unused.
//class PGOUseFunc;

/// Instruction Visitor class to visit select instructions. Pulled from PGO.
struct SelectInstVisitor : public InstVisitor<SelectInstVisitor> {
    Function &F;
    unsigned NSIs = 0;             // Number of select instructions instrumented.
    VisitMode Mode = VM_counting;  // Visiting mode.
    unsigned *CurCtrIdx = nullptr; // Pointer to current counter index.
    unsigned TotalNumCtrs = 0;     // Total number of counters
    GlobalVariable *FuncNameVar = nullptr;
    uint64_t FuncHash = 0;
    // TODO : Dont think this particular class is useful for hashing, remove if unused.
    //PGOUseFunc *UseFunc = nullptr;
  
    SelectInstVisitor(Function &Func) : F(Func) {
        // In PGO, they caution to always call countSelects before getNumOfSelectInsts,
        // so we call it as part of the constructor as we will always use it in counting
        // mode.
        countSelects();
    }

    void countSelects() {
        NSIs = 0;
        Mode = VM_counting;
        visit(F);
    }

    // Visit \p SI instruction and perform tasks according to visit mode.
    void visitSelectInst(SelectInst &SI) {
        // PGO does not handle this case.
        if (SI.getCondition()->getType()->isVectorTy())
            return;
        // We only care about counting mode for function hashing.
        if (Mode == VM_counting)
            NSIs++;
        return;
    }
  
    // Return the number of select instructions. This needs be called after
    // countSelects().
    unsigned getNumOfSelectInsts() const { return NSIs; }
};

// Return the auxiliary BB information if available.
BBInfo *findBBInfo(const BasicBlock *BB, CFGMST &MST) const { return MST.findBBInfo(BB); }

// Compute Hash value for the function's CFG: Upper 32 bits are the CRC32 of
// the index value of each BB in the CFG. The lower 32 bits are split between
// the upper 16 bits (number of edges) and lower 16 bits reserved for each
// allocation ID.
uint64_t computeCFGHash(Function &F) {
    SelectInstVisitor SIVisitor(F);
    std::vector<std::vector<VPCandidateInfo>> ValueSites;
    CFGMST MST(F);
    std::vector<char> Indexes;
    JamCRC JC;
    for (auto &BB : F) {
        const Instruction *TI = BB.getTerminator();
        for (auto I = 0u, E = TI->getNumSuccessors(); I != E; ++I) {
            BasicBlock *Succ = TI->getSuccessor(I);
            auto BI = findBBInfo(Succ, MST);
            if (BI == nullptr)
                continue;
            uint32_t Index = BI->Index;
            for (auto J = 0u; J < 4; J++) {
                Indexes.push_back((char)(Index >> (J * 8)));
            }
        }
    }
    JC.update(Indexes);

    JamCRC JCH;
    auto updateJCH = [&JCH](uint_64 Num) {
        uint8_t Data[8];
        write64le(Data, Num);
        JCH.update(Data);
    };
    updateJCH((uint64_t)SIVisitor.getNumofSelectInts());
    updateJCH((uint64_t)ValueSites[IPVK_IndirectCallTarget].size());
    updateJCH();
    updateJCH();
}
