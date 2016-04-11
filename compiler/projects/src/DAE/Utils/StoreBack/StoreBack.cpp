//===- StoreBack.cpp - Create backups for problematic stores---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file StoreBack.cpp
///
/// \brief Create backups for problematic stores
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//===----------------------------------------------------------------------===//

#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <string>

#include <llvm/Analysis/BasicAliasAnalysis.h>

#include "llvm/Transforms/Utils/Cloning.h"

#include "llvm/IR/IRBuilder.h"

#include "Util/Annotation/MetadataInfo.h"
#include "llvm/IR/InstIterator.h"

#define LIBRARYNAME "StoreBack"
#define PRINTSTREAM errs() // raw_ostream

using namespace llvm;
using namespace std;
using namespace util;

namespace {
struct StoreBack : public ModulePass {
  static char ID;
  StoreBack() : ModulePass(ID) {}

public:
  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AssumptionCacheTracker>();
    AU.addRequired<AAResultsWrapperPass>();
    AU.addRequired<TargetLibraryInfoWrapperPass>();
  }

  virtual bool runOnModule(Module &M) {
    bool change = false;

    for (Module::iterator fI = M.begin(), fE = M.end(); fI != fE; ++fI) {

      BasicAAResult BAR(createLegacyPMBasicAAResult(*this, *fI));
      AAResults AAR(createLegacyPMAAResults(*this, *fI, BAR));
      AA = &AAR;

      // What to do:
      // - Search function for Global aliases (in stores) of type
      //     MustAlias, PartialAlias, and MayAlias.
      // - Find which of these pointers that are availabe no later
      //     than in the entry block of the function.
      //   > Take the pointer value, whenever the value is not an
      //       istruction (i.e. global) or then the instruction
      //       is in the entry block it can be used, otherwise some
      //       searching is required;
      //       if it is not an instruction it sould be okay;
      //       if it is a cast, follow the cast repacing the old pointer; ** may
      //       be required for the load, should be in the entry **
      //       NO, WRONG: if it is an index calculation, ok if in entry,
      //       otherwise discard;
      //       NO, WRONG: if it is another type of instrution, discard.
      //       if it is another instruction, , ok if in entry, otherwise
      //       discard.
      // - For every approved address:
      //     > insert a load from that address in the entry block
      //       as soon as possible, store the value to a new alloca.
      //     > insert a backup restore (load from new alloca,
      //       store to original) before every return statement.
      //     > remove or change the metadata for fixed stores.

      list<StoreInst *> gaStores;
      findGAStores(*fI, gaStores);
      if (gaStores.empty()) {
        continue;
      }

      printStart() << "Function " << (*fI).getName() << ", filtering:\n";

      list<pair<StoreInst *, Value *>> gaStoreSrc;
      filterGAStores(*fI, gaStores, gaStoreSrc);
      if (gaStoreSrc.empty()) {
        continue;
      }

      printStart() << "Function " << (*fI).getName() << ", creating backups:\n";

      change = createBackups(*fI, gaStoreSrc) || change;
    }

    return change;
  }

private:
  AliasAnalysis *AA;

  void findGAStores(Function &F, list<StoreInst *> &gaStores) {
    // - Search function for Global aliases (in stores) of type
    //     MustAlias, PartialAlias, and MayAlias.

    for (inst_iterator iI = inst_begin(F), iE = inst_end(F); iI != iE; ++iI) {
      Instruction *inst = &(*iI);
      if (InstrhasMetadata(inst, "GlobalAlias", "MustAlias") ||
          InstrhasMetadata(inst, "GlobalAlias", "PartialAlias") ||
          InstrhasMetadata(inst, "GlobalAlias", "MayAlias")) {
        assert(StoreInst::classof(inst));
        gaStores.push_back((StoreInst *)inst);
      }
    }
  }

  void filterGAStores(Function &F, list<StoreInst *> &gaStores,
                      list<pair<StoreInst *, Value *>> &gaStoreSrc) {
    // - Find which of these pointers that are availabe no later
    //     than in the entry block of the function.
    //   > Take the pointer value, whenever the value is not an
    //       istruction (i.e. global) or then the instruction
    //       is in the entry block it can be used, otherwise some
    //       searching is required;
    //       if it is not an instruction it sould be okay;
    //       if it is a cast, follow the cast repacing the old pointer; ** may
    //       be required for the load, should be in the entry **
    //       NO, WRONG: if it is an index calculation, ok if in entry, otherwise
    //       discard;
    //       NO, WRONG: if it is another type of instrution, discard.
    //       if it is another instruction, , ok if in entry, otherwise discard.

    BasicBlock *entry = &(F.getEntryBlock());

    for (list<StoreInst *>::iterator sI = gaStores.begin();
         sI != gaStores.end(); ++sI) {
      StoreInst *store = *sI;
      Value *ptr = store->getPointerOperand();

      printStart() << "  Instr: " << *store << "\n";

      assert(store->getParent()->getParent() == &F);

      bool viableVal = true;

      // could possiby use llvm::Value::stripPointerCasts() instead
      while (viableVal && Instruction::classof(ptr) &&
             ((Instruction *)ptr)->getParent() != entry) {
        printStart() << "    Value: " << *ptr << "\n";
        if (CastInst::classof(ptr)) {
          ptr = ((CastInst *)ptr)->getOperand(0);
          assert(ptr != NULL);
        } else {
          viableVal = false;
        }
      }

      if (viableVal) {
        gaStoreSrc.push_back(make_pair(store, ptr));

        if (Instruction::classof(ptr)) {
          assert(((Instruction *)ptr)->getParent() == entry);
          printStart() << "    Value: " << *ptr << "\n";
          printStart() << "    VVVVVV In entry block VVVVVV\n";
        } else {
          printStart() << "    Value: " << *ptr << "\n";
          printStart() << "    VVVVVV Is global VVVVVV\n";
        }
      } else {
        printStart()
            << "    XXXXXX Non-acceptable instruction (not in entry) XXXXXX\n";
        printStart() << "    !!!!!! Warning: problematic store will not be "
                        "fixed !!!!!!\n";
      }
    }
  }

  bool createBackups(Function &F,
                     list<pair<StoreInst *, Value *>> &gaStoreSrc) {
    // - For every approved address:
    //     > insert a load from that address in the entry block
    //       as soon as possible, store the value to a new alloca.
    //     > insert a backup restore (load from new alloca,
    //       store to original) before every return statement.
    //     > remove or change the metadata for fixed stores.

    BasicBlock *entry = &(F.getEntryBlock());
    set<ReturnInst *> retPoints;
    findRetPoints(F, retPoints);

    bool change = !gaStoreSrc.empty();

    for (list<pair<StoreInst *, Value *>>::iterator svI = gaStoreSrc.begin();
         svI != gaStoreSrc.end(); ++svI) {
      StoreInst *store = (*svI).first;
      Value *ptr = (*svI).second;

      printStart() << "  Store: " << *store << "\n";
      // create backup space
      AllocaInst *newAlloca = new AllocaInst(store->getOperand(0)->getType());
      newAlloca->insertBefore(&*(entry->getFirstInsertionPt()));
      printStart() << "    Alloca: " << *newAlloca << "\n";
      // do backup
      LoadInst *backLoad = new LoadInst(ptr);
      if (Instruction::classof(ptr)) {
        backLoad->insertAfter((Instruction *)ptr);
      } else {
        backLoad->insertAfter(newAlloca);
      }
      printStart() << "    BackLoad: " << *backLoad << "\n";
      StoreInst *backStore = new StoreInst(backLoad, newAlloca);
      backStore->insertAfter(backLoad);
      printStart() << "    BackStore: " << *backStore << "\n";
      // do restore
      for (set<ReturnInst *>::iterator rI = retPoints.begin();
           rI != retPoints.end(); ++rI) {
        ReturnInst *rp = *rI;
        printStart() << "    RetPoint: " << *rp << "\n";
        LoadInst *restLoad = new LoadInst(newAlloca);
        restLoad->insertBefore(rp);
        printStart() << "      RestLoad: " << *restLoad << "\n";
        StoreInst *restStore = new StoreInst(restLoad, ptr);
        restStore->insertAfter(restLoad);
        printStart() << "      RestStore: " << *restStore << "\n";
      }
      // mark as fixed
      AttachMetadata(store, "Backup", "Backup");
    }

    return change;
  }

  void findRetPoints(Function &F, set<ReturnInst *> &retPoints) {
    for (inst_iterator iI = inst_begin(F), iE = inst_end(F); iI != iE; ++iI) {
      Instruction *inst = &(*iI);
      if (ReturnInst::classof(inst)) {
        retPoints.insert((ReturnInst *)inst);
      }
    }
  }

  raw_ostream &printStart() { return (PRINTSTREAM << LIBRARYNAME << ": "); }
};
}

char StoreBack::ID = 0;
static RegisterPass<StoreBack> X("store-back", "Backup for problematic stores",
                                 false, false);
