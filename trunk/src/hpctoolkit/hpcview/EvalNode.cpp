// $Id$
// -*-C++-*-
// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// 
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// 
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage. 
// 
// ******************************************************* EndRiceCopyright *

// ----------------------------------------------------------------------
//
// EvalNode.C
//   the implementation of evaluation tree nodes
//
// ----------------------------------------------------------------------

//************************ System Include Files ******************************

#include <iostream> 

#ifdef NO_STD_CHEADERS
# include <math.h>
#else
# include <cmath>
using namespace std; // For compatibility with non-std C headers
#endif

//************************* User Include Files *******************************

#include <include/general.h>

#include "EvalNode.h"
#include <lib/support/Nan.h>
#include <lib/support/Trace.h>

//************************ Forward Declarations ******************************

using std::cout;
using std::endl;

//****************************************************************************

// ----------------------------------------------------------------------
// class Const
// ----------------------------------------------------------------------

Const::Const(double c) : val(c) { }

Const::~Const() { }

double Const::eval(const ScopeInfo *si) {
  IFTRACE << "constant=" << val << endl; 
  return val;
}

void Const::print() { cout << "(" << val << ")"; }

// ----------------------------------------------------------------------
// class Neg
// ----------------------------------------------------------------------

Neg::Neg(EvalNode* aNode) { node = aNode; }

Neg::~Neg() { delete node; }

double Neg::eval(const ScopeInfo *si) {
  double tmp;
  if (node == NULL || IsNaNorInfinity(tmp = node->eval(si)))
    return NaNVal;
  IFTRACE << "neg=" << -tmp << endl; 
  return -tmp;
}

void Neg::print() { cout << "(-"; node->print(); cout << ")"; }

// ----------------------------------------------------------------------
// class Var
// ----------------------------------------------------------------------

Var::Var(String n, int i) : name(n), index(i) { }

Var::~Var() { }

double Var::eval(const ScopeInfo *si) {
  return si->PerfData(index);
}

void Var::print() { cout << name; }

// ----------------------------------------------------------------------
// class Power
// ----------------------------------------------------------------------

Power::Power(EvalNode* b, EvalNode* e) : base(b), exponent(e) { }

Power::~Power() { delete base; delete exponent; }

double Power::eval(const ScopeInfo *si) {
  if (base == NULL || exponent == NULL)
    return NaNVal;
  double b = base->eval(si);
  double e = exponent->eval(si);
  if (IsNaNorInfinity(b) || IsNaNorInfinity(e))
    return NaNVal;
  IFTRACE << "pow=" << pow(b, e) << endl; 
  return pow(b, e);
}

void Power::print() {
  cout << "("; base->print(); cout << "**"; exponent->print(); cout << ")";
}

// ----------------------------------------------------------------------
// class Divide
// ----------------------------------------------------------------------

Divide::Divide(EvalNode* num, EvalNode* denom)
    : numerator(num), denominator(denom) { }

Divide::~Divide() { delete numerator; delete denominator; }

double Divide::eval(const ScopeInfo *si) {
  if (numerator == NULL || denominator == NULL)
    return NaNVal;
  double n = numerator->eval(si);
  double d = denominator->eval(si);
  if (d == 0.0 || IsNaNorInfinity(d) || IsNaNorInfinity(n))
    return NaNVal;
  IFTRACE << "divident=" << n/d << endl; 
  return n / d;
}

void Divide::print() {
  cout << "("; numerator->print(); cout << "/"; 
  denominator->print(); cout << ")";
}

// ----------------------------------------------------------------------
// class Minus
// ----------------------------------------------------------------------

Minus::Minus(EvalNode* m, EvalNode* s) : minuend(m), subtrahend(s) { }

Minus::~Minus() { delete minuend; delete subtrahend; }

double Minus::eval(const ScopeInfo *si) {
  if (minuend == NULL || subtrahend == NULL)
    return NaNVal;
  double m = minuend->eval(si);
  double s = subtrahend->eval(si);
  if (IsNaNorInfinity(m) || IsNaNorInfinity(s))
    return NaNVal;
  IFTRACE << "diff=" << m-s << endl; 
  return m - s;
}

void Minus::print() {
  cout << "("; minuend->print(); cout << "-"; subtrahend->print(); cout << ")";
}

// ----------------------------------------------------------------------
// class Plus
// ----------------------------------------------------------------------

Plus::Plus(EvalNode** oprnds, int numOprnds) : nodes(oprnds), n(numOprnds) { }

Plus::~Plus() {
  for (int i = 0; i < n; i++)
    delete nodes[i];
  delete[] nodes;
}

double Plus::eval(const ScopeInfo *si) {
  double sum = 0.0;
  for (int i = 0; i < n; i++) {
    if (nodes[i] == NULL)
      return NaNVal;
    double tmp = nodes[i]->eval(si);
    if (IsNaNorInfinity(tmp))
      return NaNVal;
    sum += tmp;
  }
  IFTRACE << "sum=" << sum << endl; 
  return sum;
}

void Plus::print() {
  cout << "(";
  for (int i = 0; i < n; i++) {
      nodes[i]->print();
      if (i < n-1) cout << "+";
  }
  cout << ")";
}

// ----------------------------------------------------------------------
// class Times
// ----------------------------------------------------------------------

Times::Times(EvalNode** oprnds, int numOprnds) :
  nodes(oprnds), n(numOprnds) { }

Times::~Times() {
  for (int i = 0; i < n; i++)
    delete nodes[i];
  delete[] nodes;
}

double Times::eval(const ScopeInfo *si) {
  double product = 1.0;
  for (int i = 0; i < n; i++) {
    double tmp = nodes[i]->eval(si);
    if (IsNaNorInfinity(tmp))
      return NaNVal;
    product *= tmp;
  }
  IFTRACE << "product=" << product << endl; 
  return product;
}

void Times::print() {
  cout << "(";
  for (int i = 0; i < n; i++) {
      nodes[i]->print();
      if (i < n-1) cout << "*";
  }
  cout << ")";
}


// ----------------------------------------------------------------------
// class Min
// ----------------------------------------------------------------------

Min::Min(EvalNode** oprnds, int numOprnds) :
  nodes(oprnds), n(numOprnds) { }

Min::~Min() {
  for (int i = 0; i < n; i++)
    delete nodes[i];
  delete[] nodes;
}

double Min::eval(const ScopeInfo *si) {
  double result = nodes[0]->eval(si);
  if (IsNaNorInfinity(result)) return NaNVal;
  for (int i = 1; i < n; i++) {
    double tmp = nodes[i]->eval(si);
    if (IsNaNorInfinity(tmp))
      return NaNVal;
    result = MIN(result,tmp);
  }
  IFTRACE << "min=" << result << endl; 
  return result;
}

void Min::print() {
  cout << "min(";
  for (int i = 0; i < n; i++) {
      nodes[i]->print();
      if (i < n-1) cout << ",";
  }
  cout << ")";
}

// ----------------------------------------------------------------------
// class Max
// ----------------------------------------------------------------------

Max::Max(EvalNode** oprnds, int numOprnds) :
  nodes(oprnds), n(numOprnds) { }

Max::~Max() {
  for (int i = 0; i < n; i++)
    delete nodes[i];
  delete[] nodes;
}

double Max::eval(const ScopeInfo *si) {
  double result = nodes[0]->eval(si);
  if (IsNaNorInfinity(result)) return NaNVal;
  for (int i = 1; i < n; i++) {
    double tmp = nodes[i]->eval(si);
    if (IsNaNorInfinity(tmp))
      return NaNVal;
    result = MAX(result, tmp);
  }
  IFTRACE << "max=" << result << endl; 
  return result;
}

void Max::print() {
  cout << "max(";

  for (int i = 0; i < n; i++) {
      nodes[i]->print();
      if (i < n-1) cout << ",";
  }
  cout << ")";
}
