//===- CFGhacking.cpp - Utility for CFG transformations for DAE------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file CFGhacking.cpp
///
/// \brief Utility for CFG transformations of DAE
///
/// \copyright Eta Scale. Licensed under the DAEDAL Open Source License. See
/// the LICENSE file for details.
//===----------------------------------------------------------------------===//

#include "DAE/Utils/SkelUtils/headers.h"
#include "Utils.cpp"

using namespace llvm;
using namespace std;

#ifndef CFGhacking_
#define CFGhacking_
#define GRAN 32

BasicBlock *BuildChunkingBlock(BasicBlock *blk);
void replaceEdgesDecBlocks(BasicBlock *nb, BasicBlock *dcb, LoopInfo *LI);

BasicBlock *BuildChunkingBlock(BasicBlock *blk, Value *vi, Value *lsup,
                               Value *&vi_dcb_val) {
  Function *F = blk->getParent();
  Module *M = F->getParent();
  BasicBlock *dcb = BasicBlock::Create(
      blk->getContext(), Twine(blk->getName() + "_outer_chunking"),
      blk->getParent(), blk);
  Instruction *load_lsup = new LoadInst(lsup, "lsup_value", dcb);

  // store the old lsup in vi and increment lsup
  new StoreInst(load_lsup, vi, dcb);

  Value *granularity =
      new GlobalVariable(*M, llvm::Type::getInt64Ty(M->getContext()), false,
                         GlobalValue::ExternalLinkage,
                         0, // cstInit
                         M->getModuleIdentifier() + "_" + F->getName().str() +
                             "_" + blk->getName().str() + "_granularity");
  declareExternalGlobal(granularity, GRAN);

  Instruction *load_granularity =
      new LoadInst(granularity, "granularity_value", dcb);
  BinaryOperator *add =
      BinaryOperator::CreateAdd(load_lsup, load_granularity, "new_lsup", dcb);
  new StoreInst(add, lsup, dcb);

  LoadInst *load_vi_tmp = new LoadInst(vi, "outer_vi", dcb);
  vi_dcb_val = load_vi_tmp;
  return dcb;
}

void replaceEdgesDecBlocks(BasicBlock *H, BasicBlock *dcb, LoopInfo *LI) {

  Loop *L = LI->getLoopFor(H);
  Loop *Lparent = 0;
  BasicBlock *parent = 0;

  for (Value::use_iterator i = H->use_begin(), e = H->use_end(); i != e; ++i) {
    Instruction *Inst = dyn_cast<Instruction>(*i);

    if (Inst && isa<TerminatorInst>(Inst)) {
      parent = Inst->getParent();
      Lparent = LI->getLoopFor(parent);
      if ((Lparent == 0) || (Lparent != L)) {
        Inst->replaceUsesOfWith(H, dcb);
        if (isa<IndirectBrInst>(Inst)) {
          IndirectBrInst *IndBr = dyn_cast<IndirectBrInst>(Inst);
          BlockAddress *badd = llvm::BlockAddress::get(dcb);
          IndBr->setAddress(badd);
        }
      }
    }
  }

  BranchInst::Create(H, dcb);
}

#endif
