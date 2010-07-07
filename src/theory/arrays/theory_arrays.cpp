/*********************                                                        */
/*! \file theory_arrays.cpp
 ** \verbatim
 ** Original author: barrett
 ** Major contributors: none
 ** Minor contributors (to current version): none
 ** This file is part of the CVC4 prototype.
 ** Copyright (c) 2009, 2010  The Analysis of Computer Systems Group (ACSys)
 ** Courant Institute of Mathematical Sciences
 ** New York University
 ** See the file COPYING in the top-level source directory for licensing
 ** information.\endverbatim
 **
 ** \brief Implementation of the theory of arrays.
 **
 ** Implementation of the theory of arrays.
 **/


#include "theory/arrays/theory_arrays.h"
#include "expr/kind.h"


using namespace std;
using namespace CVC4;
using namespace CVC4::kind;
using namespace CVC4::context;
using namespace CVC4::theory;
using namespace CVC4::theory::arrays;


TheoryArrays::TheoryArrays(int id, Context* c, OutputChannel& out) :
  Theory(id, c, out)
{
}


TheoryArrays::~TheoryArrays() {
}


void TheoryArrays::addSharedTerm(TNode t) {
  Debug("arrays") << "TheoryArrays::addSharedTerm(): "
                  << t << endl;
}


void TheoryArrays::notifyEq(TNode eq) {
  Debug("arrays") << "TheoryArrays::notifyEq(): "
                  << eq << endl;
}


void TheoryArrays::check(Effort e) {
  while(!done()) {
    Node assertion = get();
    Debug("arrays") << "TheoryArrays::check(): " << assertion << endl;
  }
  Debug("arrays") << "TheoryArrays::check(): done" << endl;
}
