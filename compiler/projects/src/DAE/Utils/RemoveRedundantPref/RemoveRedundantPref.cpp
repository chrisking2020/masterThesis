//===- RemoveRedundantPref.cpp - Pass to remove redundant
//prefetches--------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file RemoveRedundantPref.cpp
///
/// \brief Pass to remove redundant prefetches
///
/// \copyright Eta Scale. Licensed under the DAEDAL Open Source License. See
/// the LICENSE file for details.
//===----------------------------------------------------------------------===//
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include <fstream>
#include <iostream>
#include <set>

using namespace llvm;
using namespace std;

/* Intrinsics.gen*/
#define PREFETCH_ID 1610

namespace {
struct RemoveRedundantPref : public FunctionPass {

  static char ID;
  RemoveRedundantPref() : FunctionPass(ID) {}

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {}

  virtual bool runOnFunction(Function &F) {

    std::set<Value *> prefTargets;
    std::vector<Instruction *> toBeRemoved;
    for (inst_iterator I = inst_begin(&F), E = inst_end(&F); I != E; ++I) {
      // prefetch instructions
      if (isa<CallInst>(&(*I))) {
        CallInst *ci = dyn_cast<CallInst>(&(*I));
        if (isa<IntrinsicInst>(ci)) {
          IntrinsicInst *intr = dyn_cast<IntrinsicInst>(ci);
          if (intr->getIntrinsicID() == PREFETCH_ID) {
            Value *target = ci->getArgOperand(0);
            if (prefTargets.find(target) != prefTargets.end()) {
              // already prefetched
              toBeRemoved.push_back(ci);
            } else {
              // first time prefetched
              prefTargets.insert(target);
            }
          }
        }
      }
    }

    bool change = !toBeRemoved.empty();

    std::vector<Instruction *>::iterator it = toBeRemoved.begin(),
                                         et = toBeRemoved.end();
    while (it != et) {
      (*it)->eraseFromParent();
      it++;
    }

    return change;
  }
};
}

char RemoveRedundantPref::ID = 1;
static RegisterPass<RemoveRedundantPref>
    X("rrp", "Remove Redundant Prefetch instructions Pass", true, true);
