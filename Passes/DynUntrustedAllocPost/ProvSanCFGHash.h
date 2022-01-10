//===- Transforms/ProvSanCFGHash.h - ProvSan CFG Hasher ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines a hasher for CFGs of functions for the MPK provenance
// sanitizer.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_PROV_SAN_CFG_HASH_MPK_H
#define LLVM_PROV_SAN_CFG_HASH_MPK_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/JamCRC.h"
#include <cstdint>
#include <vector>

namespace llvm {

uint64_t computeCFGHash(Function &F);

} // namespace llvm
#endif
