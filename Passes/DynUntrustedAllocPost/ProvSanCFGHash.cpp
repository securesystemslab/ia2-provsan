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

using namespace llvm;

// Return the auxiliary BB information if available.
BBInfo *findBBInfo(const BasicBlock *BB, CFGMST &MST) const { return MST.findBBInfo(BB); }

// Compute Hash value for the function's CFG: Upper 32 bits are the CRC32 of
// the index value of each BB in the CFG. The lower 32 bits are split between
// the upper 16 bits (number of edges) and lower 16 bits reserved for each
// allocation ID.
uint64_t computeCFGHash(Function &F) {
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
}