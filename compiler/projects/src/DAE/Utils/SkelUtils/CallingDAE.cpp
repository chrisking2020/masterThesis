//===- CallingDAE.cpp - Utility for calling and timing DAE-----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file CallingDAE.cpp
///
/// \brief Utility for calling and timing DAE
///
/// \copyright Eta Scale. Licensed under the DAEDAL Open Source License. See
/// the LICENSE file for details.
//===----------------------------------------------------------------------===//
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;
using namespace std;

#ifndef CallingDAE_
#define CallingDAE_

void insertCallToAccessFunction(Function *F, Function *cF);
void insertCallToAccessFunctionSequential(Function *F, Function *cF);
void insertCallToPAPI(CallInst *access, CallInst *execute);
void insertCallOrigToPAPI(CallInst *execute);
void insertCallInitPAPI(CallInst *mainF);
void mapArgumentsToParams(Function *F, ValueToValueMapTy *VMap);

void insertCallToAccessFunction(Function *F, Function *cF) {
  CallInst *I;
  Instruction *bI;
  std::vector<Value *> Args;
  std::vector<Type *> ArgsTy;
  Module *M = F->getParent();
  std::string name;
  Function *nF, *tF;
  FunctionType *FTy;
  std::stringstream out;

  Value::user_iterator i = F->user_begin(), e = F->user_end();
  while (i != e) {
    Args.clear();
    ArgsTy.clear();
    /*************  C codes  ***********/

    if (isa<CallInst>(*i)) {

      I = dyn_cast<CallInst>(*i);
      // call to the access function F
      Args.push_back(I->getArgOperand(0));
      ArgsTy.push_back(I->getArgOperand(0)->getType());

      // call to the execute function cF
      Args.push_back(cF);
      ArgsTy.push_back(PointerType::get(cF->getFunctionType(), 0));

      unsigned int t;
      for (t = 1; t < I->getNumArgOperands(); t++) {
        Args.push_back(I->getArgOperand(t));
        ArgsTy.push_back(I->getArgOperand(t)->getType());
        // errs() << *(I->getArgOperand(t)) << " is or not " <<
        // isa<GlobalVariable>(I->getArgOperand(t)) << "\n";
      }
      tF = dyn_cast<Function>(I->getCalledFunction());
      FTy = FunctionType::get(tF->getReturnType(), ArgsTy, 0);

      out.str(std::string());
      out << "task_DAE_" << I->getNumArgOperands() - 1;
      nF = (Function *)M->getOrInsertFunction(out.str(), FTy);
      CallInst *ci = CallInst::Create(nF, Args, I->getName(), I);

      i++;
      I->replaceAllUsesWith(ci);
      I->eraseFromParent();
    }
    /*************  C++ codes  ***********/
    else {

      Value::user_iterator bit = (*i)->user_begin(), bite = (*i)->user_end();

      Type *iTy = (*i)->getType();
      i++;
      while (bit != bite) {
        Args.clear();
        ArgsTy.clear();
        I = dyn_cast<CallInst>(*bit);

        bit++;

        // call to the access function F
        Args.push_back(I->getArgOperand(0));
        ArgsTy.push_back(I->getArgOperand(0)->getType());

        // call to the execute function cF
        bI = new BitCastInst(cF, (iTy), "_TPR", I);
        Args.push_back(bI);
        ArgsTy.push_back(bI->getType());

        unsigned int t;
        for (t = 1; t < I->getNumArgOperands(); t++) {
          Args.push_back(I->getArgOperand(t));
          ArgsTy.push_back(I->getArgOperand(t)->getType());
        }
        tF = dyn_cast<Function>(I->getCalledFunction());
        FTy = FunctionType::get(tF->getReturnType(), ArgsTy, 0);

        out.str(std::string());
        out << "task_DAE_" << I->getNumArgOperands() - 1;
        nF = (Function *)M->getOrInsertFunction(out.str(), FTy);
        CallInst *ci = CallInst::Create(nF, Args, I->getName(), I);

        I->replaceAllUsesWith(ci);
        I->eraseFromParent();
      }
    }
  }
}

void insertCallToAccessFunctionSequential(Function *F, Function *cF) {
  CallInst *I;
  BasicBlock *b;

  Value::user_iterator i = F->user_begin(), e = F->user_end();
  while (i != e) {
    if (isa<CallInst>(*i)) {

      I = dyn_cast<CallInst>(*i);
      b = I->getParent();
      BasicBlock::iterator helper(I);
      CallInst *ci = dyn_cast<CallInst>(I->clone());
      ci->setCalledFunction(cF);
      b->getInstList().insertAfter(helper, ci);

      i++;
      I->replaceAllUsesWith(ci);

      insertCallToPAPI(I, ci);
    }
  }
}

void mapArgumentsToParams(Function *F, ValueToValueMapTy *VMap) {
  CallInst *I;
  Instruction *aux = 0;
  Value *vaux;

  Value::user_iterator i = F->user_begin(), e = F->user_end();
  Function::arg_iterator a = F->arg_begin();
  while (i != e) {

    a = F->arg_begin();
    /*************  C codes  ***********/

    if (isa<CallInst>(*i)) {

      I = dyn_cast<CallInst>(*i);
      unsigned int t;
      for (t = 1; t < I->getNumArgOperands(); t++) {
        aux = 0;
        vaux = I->getArgOperand(t);
        while (isa<Instruction>(vaux)) {
          aux = dyn_cast<Instruction>(vaux);
          vaux = aux->getOperand(0);
        }

        // errs() << "ARG "<< *(a) << " ----------------- " << *vaux << "\n";
        Value *argument = &*a;
        VMap->insert(std::pair<Value *, Value *>(argument, &*vaux));
        a++;
      }
      i++;
    }
    /*************  C++ codes  ***********/
    else {

      Value::user_iterator bit = (*i)->user_begin(), bite = (*i)->user_end();
      i++;
      while (bit != bite) {
        I = dyn_cast<CallInst>(*bit);
        a = F->arg_begin();
        bit++;
        unsigned int t;
        for (t = 1; t < I->getNumArgOperands(); t++) {
          aux = 0;
          vaux = I->getArgOperand(t);
          errs() << "vaux=" << *vaux << "    a=" << *a << "\n";
          while ((isa<Instruction>(vaux)) && (!isa<PHINode>(vaux))) {
            aux = dyn_cast<Instruction>(vaux);
            if (!isa<PHINode>(aux))
              vaux = aux->getOperand(0);
            // errs() << "vaux="<<*vaux << "    a="<<*a <<"\n";
          }

          errs() << "ARG " << *(a) << " ----------------- " << *vaux << "\n";
          Value *argument = &*a;
          VMap->insert(std::pair<Value *, Value *>(argument, vaux));
          a++;
        }
      }
    }
  }
}

void insertCallInitPAPI(Function *mainF) {

  Module *M = mainF->getParent();
  FunctionType *FTy =
      FunctionType::get(llvm::Type::getVoidTy(M->getContext()), 0);

  Function *profiler_print_stats =
      cast<Function>(M->getOrInsertFunction("profiler_print_stats", FTy));
  profiler_print_stats->setCallingConv(CallingConv::C);

  IRBuilder<> Builder(mainF->getEntryBlock().getTerminator());

  for (inst_iterator I = inst_begin(mainF), E = inst_end(mainF); I != E; ++I) {
    if (isa<ReturnInst>(*I)) {
      Builder.SetInsertPoint(&(*I));
      Builder.CreateCall(profiler_print_stats);
    }
  }
}

void insertCallToPAPI(CallInst *access, CallInst *execute) {
  Function *caller = access->getParent()->getParent();
  Module *M = caller->getParent();
  IRBuilder<> Builder(access);

  std::vector<llvm::Type *> tid_arg;
  tid_arg.push_back(Builder.getInt64Ty());
  llvm::ArrayRef<llvm::Type *> int64t_ref(tid_arg);

  std::vector<llvm::Type *> profiler_args;
  profiler_args.push_back(Builder.getInt8PtrTy());
  llvm::ArrayRef<llvm::Type *> void_ptr_ref(profiler_args);

  // Declare FunctionType returning Int64 -- no input args
  FunctionType *fT_i64_v =
      FunctionType::get(llvm::Type::getInt64Ty(M->getContext()), false);
  // Declare FunctionType returning void * (vptr) -- one input argument of type:
  // Int64
  FunctionType *fT__vptr_i64 = FunctionType::get(
      llvm::Type::getInt8PtrTy(M->getContext()), int64t_ref, false);
  // Declare FunctionType returning (v)oid -- one input argument of type: void *
  // (vptr)
  FunctionType *fT_v_vptr = FunctionType::get(
      llvm::Type::getVoidTy(M->getContext()), void_ptr_ref, false);

  Function *profiler_get_thread_id = cast<Function>(
      M->getOrInsertFunction("profiler_get_thread_id", fT_i64_v));
  profiler_get_thread_id->setCallingConv(CallingConv::C);
  Function *profiler_get_counters = cast<Function>(
      M->getOrInsertFunction("profiler_get_counters", fT__vptr_i64));
  profiler_get_counters->setCallingConv(CallingConv::C);

  Function *profiler_start_access = cast<Function>(
      M->getOrInsertFunction("profiler_start_access", fT_v_vptr));
  profiler_start_access->setCallingConv(CallingConv::C);
  Function *profiler_end_access =
      cast<Function>(M->getOrInsertFunction("profiler_end_access", fT_v_vptr));
  profiler_end_access->setCallingConv(CallingConv::C);
  Function *profiler_start_execute = cast<Function>(
      M->getOrInsertFunction("profiler_start_execute", fT_v_vptr));
  profiler_start_execute->setCallingConv(CallingConv::C);
  Function *profiler_end_execute =
      cast<Function>(M->getOrInsertFunction("profiler_end_execute", fT_v_vptr));
  profiler_end_execute->setCallingConv(CallingConv::C);
  profiler_end_access->addFnAttr(Attribute::AlwaysInline);

  Value *thread_id =
      Builder.CreateCall(profiler_get_thread_id, None, "thread_id");
  Value *p_counters =
      Builder.CreateCall(profiler_get_counters, thread_id, "p_counters");

  /* insert PAPI calls before the prefetch phase*/
  Builder.CreateCall(profiler_start_access, p_counters);

  /* insert PAPI calls after the prefetch phase and before the execute phase*/
  Builder.SetInsertPoint(execute);
  Builder.CreateCall(profiler_end_access, p_counters);
  Builder.CreateCall(profiler_start_execute, p_counters);

  /* insert PAPI calls after the execute phase*/
  BasicBlock::iterator I(execute);
  I++;
  Builder.SetInsertPoint(&(*I));
  Builder.CreateCall(profiler_end_execute, p_counters);
}

void insertCallOrigToPAPI(CallInst *execute) {

  Function *caller = execute->getParent()->getParent();
  Module *M = caller->getParent();
  IRBuilder<> Builder(execute);

  std::vector<llvm::Type *> tid_arg;
  tid_arg.push_back(Builder.getInt64Ty());
  llvm::ArrayRef<llvm::Type *> int64t_ref(tid_arg);

  std::vector<llvm::Type *> profiler_args;
  profiler_args.push_back(Builder.getInt8PtrTy());
  llvm::ArrayRef<llvm::Type *> void_ptr_ref(profiler_args);

  // Declare FunctionType returning Int64 -- no input args
  FunctionType *fT_i64_v =
      FunctionType::get(llvm::Type::getInt64Ty(M->getContext()), false);
  // Declare FunctionType returning void * (vptr) -- one input argument of type:
  // Int64
  FunctionType *fT__vptr_i64 = FunctionType::get(
      llvm::Type::getInt8PtrTy(M->getContext()), int64t_ref, false);
  // Declare FunctionType returning (v)oid -- one input argument of type: void *
  // (vptr)
  FunctionType *fT_v_vptr = FunctionType::get(
      llvm::Type::getVoidTy(M->getContext()), void_ptr_ref, false);

  Function *profiler_get_thread_id = cast<Function>(
      M->getOrInsertFunction("profiler_get_thread_id", fT_i64_v));
  profiler_get_thread_id->setCallingConv(CallingConv::C);
  Function *profiler_get_counters = cast<Function>(
      M->getOrInsertFunction("profiler_get_counters", fT__vptr_i64));
  profiler_get_counters->setCallingConv(CallingConv::C);
  Function *profiler_start_execute = cast<Function>(
      M->getOrInsertFunction("profiler_start_execute", fT_v_vptr));
  profiler_start_execute->setCallingConv(CallingConv::C);
  Function *profiler_end_execute =
      cast<Function>(M->getOrInsertFunction("profiler_end_execute", fT_v_vptr));
  profiler_end_execute->setCallingConv(CallingConv::C);

  /* insert PAPI calls after the prefetch phase and before the execute phase*/
  Value *thread_id =
      Builder.CreateCall(profiler_get_thread_id, None, "thread_id");
  Value *p_counters =
      Builder.CreateCall(profiler_get_counters, thread_id, "p_counters");

  /* insert PAPI calls after the prefetch phase and before the execute phase*/
  Builder.CreateCall(profiler_start_execute, p_counters);

  /* insert PAPI calls after the execute phase*/
  BasicBlock::iterator I(execute);
  I++;
  Builder.SetInsertPoint(&(*I));
  Builder.CreateCall(profiler_end_execute, p_counters);
}
#endif
