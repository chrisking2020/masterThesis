//===- Utils.cpp - Determines which functions to target for DAE------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file Utils.cpp
///
/// \brief Determines which functions to target for DAE
///
/// \copyright Eta Scale. Licensed under the DAEDAL Open Source License. See
/// the LICENSE file for details.
//===----------------------------------------------------------------------===//
using namespace llvm;
using namespace std;

#ifndef Utils_
#define Utils_

#include "DAE/Utils/SkelUtils/headers.h"
#include <algorithm>
#include <llvm/IR/BasicBlock.h>

void declareExternalGlobal(Value *v, int val);
bool toBeDAE(Function *F);
bool isDAEkernel(Function *F);
bool isMain(Function *F);

bool isDAEkernel(Function *F) {
  bool ok = false;
  size_t found = F->getName().str().find("_clone");
  size_t found1 = F->getName().str().find("__kernel__");
  if ((found == std::string::npos) && (found1 != std::string::npos))
    ok = true;
  return ok;
}

void declareExternalGlobal(Value *v, int val) {
  std::string path = "Globals.ll";
  std::error_code err;
  llvm::raw_fd_ostream out(path.c_str(), err, llvm::sys::fs::F_Append);

  out << "\n@\"" << v->getName() << "\" = global i64 " << val << "  \n";
  out.close();
}

bool isMain(Function *F) { return F->getName().str().compare("main") == 0; }

bool toBeDAE(Function *F) {
  bool ok = false;

  /*401.bzip*/
  if (F->getName().str().compare("generateMTFValues") == 0)
    ok = true;
  if (F->getName().str().compare("BZ2_decompress") == 0)
    ok = true;

  /*429.mcf*/
  if (F->getName().str().compare("primal_bea_mpp") == 0)
    ok = true;

  /*433.milc*/
  if (F->getName().str().compare("mult_su3_na") == 0)
    ok = true;

  /*450.soplex*/
  if (F->getName().str().compare(
          "_ZN6soplex8SSVector20assign2product4setupERKNS_5SVSetERKS0_") == 0)
    ok = true;
  if (F->getName().str().compare(
          "_ZN6soplex10SPxSteepPR9entered4XENS_5SPxIdEiiiii") == 0)
    ok = true;
  if (F->getName().str().compare("_ZN6soplex8SSVector5setupEv") == 0)
    ok = true;

  /*456.hmmer*/
  if (F->getName().str().compare("P7Viterbi") == 0)
    ok = true;

  /*458.sjeng*/
  if (F->getName().str().compare("std_eval") == 0)
    ok = true;

  /*462.libQ*/
  if (F->getName().str().compare("quantum_toffoli") == 0)
    ok = true;

  /*470.lbm*/
  if (F->getName().str().compare("LBM_performStreamCollide") == 0)
    ok = true;

  /*464.h264ref*/
  if (F->getName().str().compare("SetupFastFullPelSearch") == 0)
    ok = true;

  /*473.astar*/
  if (F->getName().str().compare("_ZN7way2obj12releaseboundEv") == 0)
    ok = true;
  if (F->getName().str().compare("_ZN6wayobj10makebound2EPiiS0_") == 0)
    ok = true;
  if (F->getName().str().compare(
          "_ZN9regwayobj10makebound2ER9flexarrayIP6regobjES4_") == 0)
    ok = true;

  return ok;
}

#endif
