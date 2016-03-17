//===- TimeOrig.cpp - Inserts timing information for CAE ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file TimeOrig.cpp
///
/// \brief Inserts timing information for CAE
///
/// \copyright Eta Scale. Licensed under the DAEDAL Open Source License. See
/// the LICENSE file for details.
//===----------------------------------------------------------------------===//
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "../SkelUtils/CallingDAE.cpp"
#include "../SkelUtils/Utils.cpp"

using namespace llvm;

namespace {
struct TimeOrig : public FunctionPass {

  static char ID;
  TimeOrig() : FunctionPass(ID) {}

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {}

  virtual bool runOnFunction(Function &F) {
    if (isDAEkernel(&F)) {
      for (llvm::Value::user_iterator II = F.user_begin(), EE = F.user_end();
           II != EE; ++II) {
        if (llvm::Instruction *user = llvm::dyn_cast<llvm::Instruction>(*II))
          if (isa<CallInst>(user)) {
            insertCallOrigToPAPI(llvm::dyn_cast<llvm::CallInst>(user));
          }
      }
    } else {
      if (isMain(&F)) {
        insertCallInitPAPI(&F);
      }
    }
    return false;
  }
};
}

char TimeOrig::ID = 1;
static RegisterPass<TimeOrig> X("papi-orig", "Papi timers Pass", true, true);
