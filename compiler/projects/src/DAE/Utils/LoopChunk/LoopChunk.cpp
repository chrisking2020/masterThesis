//===- LoopChunk.cpp - Transforms a loop into a doubly nested loop---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file LoopChunk.cpp
///
/// \brief Transforms a loop into a doubly nested loop (strip mining)
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//===----------------------------------------------------------------------===//
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/Intrinsics.h"

#include <fstream>
#include <iostream>

#include "../SkelUtils/CFGhacking.cpp"
#include "../SkelUtils/LoopUtils.cpp"
#include "../SkelUtils/Utils.cpp"
#include "Util/Annotation/MetadataInfo.h"

using namespace llvm;

#define F_KERNEL_SUBSTR "__kernel__"

static cl::opt<std::string> BenchName("bench-name",
                                      cl::desc("The benchmark name"),
                                      cl::value_desc("name"));

namespace {
struct LoopChunk : public LoopPass {

  static char ID;
  LoopChunk() : LoopPass(ID) {}

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
  }

  bool runOnLoop(Loop *L, LPPassManager &) {
    BasicBlock *h = L->getHeader();
    Function *F = h->getParent();

    if (L->getHeader()->getName().str().find(F_KERNEL_SUBSTR) != string::npos) {
      LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
      Value *vi, *ub;
      Instruction *lvi;
      PHINode *phi_vi;

      insertVirtualIterator(L, vi, ub);
      if (L->getLoopDepth() > 1) {
        initializeVIinParentHeader(L, vi);
      }

      Value *vi_dcb_val;
      BasicBlock *dcb = BuildChunkingBlock(h, vi, ub, vi_dcb_val);
      if (L->getLoopDepth() > 1) {
        Loop *Lp = L->getParentLoop();
        if (Lp) {
          Lp->addBasicBlockToLoop(dcb, *LI);
        }
      }
      BasicBlock *ch = insertChunkCond(L, LI, vi, ub, dcb, vi_dcb_val, phi_vi);
      BasicBlock *latch = L->getLoopLatch();

      incrementVirtualIteratorSpec(latch, vi, phi_vi);

      replaceEdgesDecBlocks(ch, dcb, LI);
      return true;
    }
    return false;
  }
};
}

char LoopChunk::ID = 1;
static RegisterPass<LoopChunk> X("loop-chunk", "Loop chunking Pass", true,
                                 true);
