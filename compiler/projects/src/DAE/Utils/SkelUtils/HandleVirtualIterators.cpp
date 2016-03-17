//===- HandleVirtualIterators.cpp - Utility to add virtual iterator -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file HandleVirtualIterators.cpp
///
/// \brief Utility to add virtual iterator
///
/// \copyright Eta Scale. Licensed under the DAEDAL Open Source License. See
/// the LICENSE file for details.
//
// This file provides the functions for inserting a virtual
// iterator for each loop and maintaing it during execution
//
//===----------------------------------------------------------------------===//
#include "DAE/Utils/SkelUtils/headers.h"
#include "Util/Annotation/MetadataInfo.h"
#include "Utils.cpp"

using namespace llvm;
using namespace std;
using namespace util;

#ifndef HandleVirtualIterators_
#define HandleVirtualIterators_

#define MAX_SUP 32

void insertVirtualIterator(Loop *&L, Value *&vi, Value *&lsup);
void incrementVirtualIteratorSpec(BasicBlock *BB, Value *vi, Instruction *lvi);
void initializeVIinParentHeader(Loop *L, Value *vi);
BasicBlock *insertChunkCond(Loop *&L, LoopInfo *LI, Value *vi, Value *lsup,
                            Instruction *&load_vi, BasicBlock *dcb);
std::string replaceAllOccurences(std::string &str, std::string oldstr,
                                 std::string newstr);
bool belongs(std::vector<BasicBlock *> cloned_code, BasicBlock *bb);
Function *createDBfunction(LLVMContext &C, Module *M, std::string ID);
void updateDT(BasicBlock *oldB, BasicBlock *newB, DominatorTree *DT);
Instruction *getLoopVirtualIterator(Loop *L);
BasicBlock *getCaller(Function *F);
void updateDominatorTree(Loop *L, BasicBlock *bb);
void addBBToL(BasicBlock *bb, Loop *L, LoopInfo *loopInfo);
Value *FindFunctionArgumentOfInstr(Instruction *I, Function *F);

/*
  to execute the loop by chuncks we insert a virtual iterator that
  takes values between a lower and an upper limit set by the VM
*/
void insertVirtualIterator(Loop *&L, Value *&vi, Value *&lsup) {

  BasicBlock *H = L->getHeader();
  Function *F = H->getParent();
  Module *M = F->getParent();

  /* declare V.I. and the bounds of the chunk*/
  vi = new GlobalVariable(*(F->getParent()),
                          llvm::Type::getInt64Ty(F->getContext()), false,
                          GlobalValue::ExternalLinkage,
                          0, // cstInit
                          M->getModuleIdentifier() + "_" + F->getName().str() +
                              "_" + H->getName().str() + "_vi");
  declareExternalGlobal(vi, 0);
  lsup = new GlobalVariable(
      *(F->getParent()), llvm::Type::getInt64Ty(F->getContext()), false,
      GlobalValue::ExternalLinkage,
      0, // cstInit
      M->getModuleIdentifier() + "_" + F->getName().str() + "_" +
          H->getName().str() + "_lsup");
  declareExternalGlobal(lsup, 0);
}

/* vi of subloops must be re-initialized in the header of the parent loop*/
void initializeVIinParentHeader(Loop *L, Value *vi) {
  if (!L->getParentLoop())
    errs() << "ERROR: Subloop expected! \n\n";
  ConstantInt *zero =
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(vi->getContext()), 0);
  new StoreInst(zero, vi, L->getParentLoop()->getHeader()->getTerminator());
}

/*find and return the virtual iterator of the loop*/
Instruction *getLoopVirtualIterator(Loop *L) {
  BasicBlock *H =
      &(getCaller(L->getHeader()->getParent())->getParent()->getEntryBlock());
  BasicBlock::iterator it = H->begin(), et = H->end();

  while (it != et) {
    if (InstrhasMetadata(&*it, "VirtualIt", "currentVI"))
      return &*it;
    it++;
  }

  return 0;
}

/*find and return the virtual upper bound of the loop*/
Instruction *getLoopVirtualUpperBound(Loop *L) {
  BasicBlock *H =
      &(getCaller(L->getHeader()->getParent())->getParent()->getEntryBlock());
  BasicBlock::iterator it = H->begin(), et = H->end();

  while (it != et) {
    if (InstrhasMetadata(&*it, "VirtualIt", "upperBound"))
      return &*it;
    it++;
  }

  return 0;
}

/*
  in addition to the original condition of the loop, insert one cond
  on the virtual iterator, given the linf and lsup bounds for the chunk
  this cond exits and returns to the decision block to start a new chunk
*/
BasicBlock *insertChunkCond(Loop *&L, LoopInfo *LI, Value *vi, Value *lsup,
                            BasicBlock *dcb, Value *vi_dcb_val,
                            PHINode *&phi_vi) {

  BasicBlock *H = L->getHeader();

  BasicBlock *newCond = BasicBlock::Create(
      H->getContext(), Twine("__kernel__" + H->getName().str() + "_viCond"),
      H->getParent(), H);

  std::vector<BasicBlock *> Lblocks = L->getBlocks();

  BasicBlock *exitBlock = BasicBlock::Create(
      H->getContext(), Twine(H->getName().str() + "_exitChunk"), H->getParent(),
      H);
  BranchInst::Create(dcb, exitBlock);

  phi_vi = PHINode::Create(Type::getInt64Ty(H->getContext()), 2, "vi_value",
                           newCond);
  phi_vi->addIncoming(vi_dcb_val, dcb);

  LoadInst *load_lsup = new LoadInst(lsup, "lsup_value", newCond);

  ICmpInst *cmp = new ICmpInst(*newCond, ICmpInst::ICMP_SLT, phi_vi, load_lsup,
                               vi->getName() + "_cmp");

  // Make sure all predecessors now go to our new condition
  std::vector<TerminatorInst *> termInstrs;
  BasicBlock *lp = L->getLoopPredecessor();

  for (auto it = pred_begin(H), end = pred_end(H); it != end; ++it) {
    if ((*it) == lp) {
      // Original entry should be redirected to dcb
      TerminatorInst *tinstr = (*it)->getTerminator();
      for (auto it = tinstr->op_begin(), end = tinstr->op_end(); it != end;
           ++it) {
        Use *use = &*it;
        if (use->get() == H) {
          use->set(dcb);
        }
      }
    } else {
      termInstrs.push_back((*it)->getTerminator());
    }
  }

  for (auto &tinstr : termInstrs) {
    for (auto it = tinstr->op_begin(), end = tinstr->op_end(); it != end;
         ++it) {
      Use *use = &*it;
      if (use->get() == H) {
        use->set(newCond);
      }
    }
  }

  BranchInst::Create(H, exitBlock, cmp, newCond);

  // update loop info
  if (L != LI->getLoopFor(newCond)) {
    L->addBasicBlockToLoop(newCond, *LI);
  }

  L->moveToHeader(newCond);

  Loop *Lp = L->getParentLoop();
  if (Lp)
    Lp->addBasicBlockToLoop(exitBlock, *LI);
  return newCond;
}

void incrementVirtualIteratorSpec(BasicBlock *BB, Value *vi, PHINode *phi_vi) {
  assert((BB != 0) &&
         "WARNING: Loop has no unique latch! Try simplify-loop pass first.\n");

  ConstantInt *one =
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(vi->getContext()), 1);

  BinaryOperator *add = BinaryOperator::CreateAdd(
      phi_vi, // new LoadInst(vi,"vi_load_add",  BB->getTerminator()),
      one, vi->getName() + "_inc", BB->getTerminator());

  StoreInst *store_vi_inc = new StoreInst(add, vi, BB->getTerminator());
  phi_vi->addIncoming(store_vi_inc->getOperand(0), BB);

  return;
}

/* create the function for the decision block*/
Function *createDBfunction(LLVMContext &C, Module *M, std::string ID) {
  std::vector<Type *> Params;
  Params.push_back(llvm::PointerType::get(llvm::Type::getInt32Ty(C), 0));
  Params.push_back(llvm::PointerType::get(llvm::Type::getInt32Ty(C), 0));
  llvm::FunctionType *FTy = llvm::FunctionType::get(
      llvm::Type::getInt32Ty(C), makeArrayRef(Params), false);
  Function *newFunction = Function::Create(FTy, llvm::Function::ExternalLinkage,
                                           "decisionBlock" + ID, M);
  return newFunction;
}

/* replace all occurences of a substring in a string*/
std::string replaceAllOccurences(std::string &str, std::string subStr,
                                 std::string newsubstr) {
  size_t found;

  found = str.find(subStr);
  while (found != std::string::npos) {
    str.replace(str.find(subStr), subStr.length(), newsubstr);
    found = str.find(subStr);
  }
  return str;
}

/* check if the vector contains the element*/
bool belongs(std::vector<BasicBlock *> cloned_code, BasicBlock *bb) {
  bool bel = false;
  unsigned i = 0;
  while (i < cloned_code.size() && !bel) {
    if (cloned_code[i] == bb)
      bel = true;
    i++;
  }

  return bel;
}

BasicBlock *getCaller(Function *F) {
  for (Value::use_iterator I = F->use_begin(), E = F->use_end(); I != E; ++I) {
    if (isa<CallInst>(*I) || isa<InvokeInst>(*I)) {
      Instruction *User = dyn_cast<Instruction>(*I);
      return User->getParent();
    }
  }
  return 0;
}

/* for the vi and lsup, find their corresponding argument*/
Value *FindFunctionArgumentOfInstr(Instruction *I, Function *F) {
  int notFound = 1;
  Value *via = 0;

  Function::arg_iterator FI = F->arg_begin(), FE = F->arg_end();
  while (FI != FE && notFound) {
    if (FI->getName() == I->getName()) {
      via = &*FI;
      notFound = 0;
    }
    ++FI;
  }
  return via;
}

/*update the dominator tree when a new block is created*/
void updateDT(BasicBlock *oldB, BasicBlock *newB, DominatorTree *DT) {
  errs() << "\n -----------------------" << oldB->getName()
         << "\n  dominated by  \n"
         << newB->getName() << "\n";

  DomTreeNode *OldNode = DT->getNode(oldB);
  if (OldNode) {
    std::vector<DomTreeNode *> Children;
    for (DomTreeNode::iterator I = OldNode->begin(), E = OldNode->end(); I != E;
         ++I)
      Children.push_back(*I);

    DomTreeNode *inDT = DT->getNode(newB);
    DomTreeNode *NewNode;

    if (inDT == 0)
      NewNode = DT->addNewBlock(newB, oldB);
    else
      NewNode = DT->getNode(newB);

    for (std::vector<DomTreeNode *>::iterator I = Children.begin(),
                                              E = Children.end();
         I != E; ++I)
      DT->changeImmediateDominator(*I, NewNode);
  }
}
#endif
