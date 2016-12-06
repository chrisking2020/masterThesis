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
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//===----------------------------------------------------------------------===//
using namespace llvm;
using namespace std;

#ifndef Utils_
#define Utils_

#include "llvm/Analysis/LoopAccessAnalysis.h"
#include "DAE/Utils/SkelUtils/headers.h"
#include <algorithm>
#include <llvm/IR/BasicBlock.h>

void declareExternalGlobal(Value *v, int val);
bool loopToBeDAE(Loop *L, std::string benchmarkName);
bool isDAEkernel(Function *F);
bool isMain(Function *F);

/////////////////////////////////////////////////////////////
//
//              From LoopVectorize.cpp
//
/////////////////////////////////////////////////////////////


/// Utility class for getting and setting loop vectorizer hints in the form
/// of loop metadata.
/// This class keeps a number of loop annotations locally (as member variables)
/// and can, upon request, write them back as metadata on the loop. It will
/// initially scan the loop for existing metadata, and will update the local
/// values based on information in the loop.
/// We cannot write all values to metadata, as the mere presence of some info,
/// for example 'force', means a decision has been made. So, we need to be
/// careful NOT to add them if the user hasn't specifically asked so.
class LoopVectorizeHints {
  enum HintKind {
    HK_WIDTH,
    HK_UNROLL,
    HK_FORCE
  };

  /// Hint - associates name and validation with the hint value.
  struct Hint {
    const char * Name;
    unsigned Value; // This may have to change for non-numeric values.
    HintKind Kind;

    Hint(const char * Name, unsigned Value, HintKind Kind)
      : Name(Name), Value(Value), Kind(Kind) { }

    bool validate(unsigned Val) {
      switch (Kind) {
      case HK_WIDTH:
        return true; //isPowerOf2_32(Val) && Val <= VectorizerParams::MaxVectorWidth;
      case HK_UNROLL:
        return true; //isPowerOf2_32(Val) && Val <= MaxInterleaveFactor;
      case HK_FORCE:
        return (Val <= 1);
      }
      return false;
    }

  };

  /// Vectorization width.
  Hint Width;
  /// Vectorization interleave factor.
  Hint Interleave;
  /// Vectorization forced
  Hint Force;

  /// Return the loop metadata prefix.
  static StringRef Prefix() { return "llvm.loop."; }

public:
  enum ForceKind {
    FK_Undefined = -1, ///< Not selected.
    FK_Disabled = 0,   ///< Forcing disabled.
    FK_Enabled = 1,    ///< Forcing enabled.
  };

  LoopVectorizeHints(const Loop *L, bool DisableInterleaving)
      : Width("vectorize.width", VectorizerParams::VectorizationFactor,
              HK_WIDTH),
        Interleave("interleave.count", DisableInterleaving, HK_UNROLL),
        Force("vectorize.enable", FK_Undefined, HK_FORCE),
        TheLoop(L) {
    // Populate values with existing loop metadata.
    getHintsFromMetadata();

    // force-vector-interleave overrides DisableInterleaving.
    if (VectorizerParams::isInterleaveForced())
      Interleave.Value = VectorizerParams::VectorizationInterleave;

    //DEBUG(if (DisableInterleaving && Interleave.Value == 1) dbgs()
    //      << "LV: Interleaving disabled by the pass manager\n");
  }

  unsigned getWidth() const { return Width.Value; }
  unsigned getInterleave() const { return Interleave.Value; }
  enum ForceKind getForce() const { return (ForceKind)Force.Value; }
private:
  /// Find hints specified in the loop metadata and update local values.
  void getHintsFromMetadata() {
    MDNode *LoopID = TheLoop->getLoopID();
    if (!LoopID)
      return;

    // First operand should refer to the loop id itself.
    assert(LoopID->getNumOperands() > 0 && "requires at least one operand");
    assert(LoopID->getOperand(0) == LoopID && "invalid loop id");

    for (unsigned i = 1, ie = LoopID->getNumOperands(); i < ie; ++i) {
      const MDString *S = nullptr;
      SmallVector<Metadata *, 4> Args;

      // The expected hint is either a MDString or a MDNode with the first
      // operand a MDString.
      if (const MDNode *MD = dyn_cast<MDNode>(LoopID->getOperand(i))) {
        if (!MD || MD->getNumOperands() == 0)
          continue;
        S = dyn_cast<MDString>(MD->getOperand(0));
        for (unsigned i = 1, ie = MD->getNumOperands(); i < ie; ++i)
          Args.push_back(MD->getOperand(i));
      } else {
        S = dyn_cast<MDString>(LoopID->getOperand(i));
        assert(Args.size() == 0 && "too many arguments for MDString");
      }

      if (!S)
        continue;

      // Check if the hint starts with the loop metadata prefix.
      StringRef Name = S->getString();
      if (Args.size() == 1)
        setHint(Name, Args[0]);
    }
  }

  // /// Checks string hint with one operand and set value if valid.
  void setHint(StringRef Name, Metadata *Arg) {
    if (!Name.startswith(Prefix()))
      return;
    Name = Name.substr(Prefix().size(), StringRef::npos);

    const ConstantInt *C = mdconst::dyn_extract<ConstantInt>(Arg);
    if (!C) return;
    unsigned Val = C->getZExtValue();

    Hint *Hints[] = {&Width, &Interleave, &Force};
    for (auto H : Hints) {
      if (Name == H->Name) {
        if (H->validate(Val))
          H->Value = Val;
        else
          // DEBUG(dbgs() << "LV: ignoring invalid hint '" << Name << "'\n");
        break;
      }
    }
  }

  /// The loop these hints belong to.
  const Loop *TheLoop;
}; // end LoopVectorizeHints

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

bool loopToBeDAE(Loop *L, std::string benchmarkName) {
  int MAGIC_TRANSFORM = 1337;

  LoopVectorizeHints Hints(L, false);
  if (Hints.getWidth() >= MAGIC_TRANSFORM) {
      return true;
  }
  
  return false;
}


#endif
