// -*-Mode: C++;-*-
// $Id$
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

//***************************************************************************
//
// File:
//    $Source$
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <map> // STL

//*************************** User Include Files ****************************

#include <include/general.h>

#include "PgmScopeTree.h"
#include <lib/support/VectorTmpl.h>
#include <lib/support/Assertion.h>
#include <lib/support/SrcFile.h>
#include <lib/support/PtrSetIterator.h>
#include <lib/support/Trace.h>
#include <lib/support/realpath.h>
#include <lib/xml/xml.h>

//*************************** Forward Declarations ***************************

// Enabling this flag (not allowing duplicate function names in a
// file) can cause problems if proper file scoping is not maintained.
// This can happen when static functions are lumped together b/c a
// lack of debugging information has made file identification
// impossible
//#define SCOPE_TREE_DISALLOW_DUPLICATE_FUNCTION_NAMES_IN_FILE

int SimpleLineCmp(suint x, suint y);

using namespace xml;

using std::ostream;
using std::endl;

//****************************************************************************

/*****************************************************************************/
// template classes 
/*****************************************************************************/

class DoubleVector : public VectorTmpl<double> {
public: 
  DoubleVector() : VectorTmpl<double>(NaNVal, true) { }; 
}; 

class LoadModScopeMap : public std::map<String, LoadModScope*, StringLt> { };
class ProcScopeMap    : public std::map<String, ProcScope*, StringLt> { }; 
class FileScopeMap    : public std::map<String, FileScope*, StringLt> { }; 

//***************************************************************************
// PgmScopeTree
//***************************************************************************

PgmScopeTree::PgmScopeTree(PgmScope* _root)
  : root(_root)
{
}

PgmScopeTree::~PgmScopeTree()
{
  delete root; 
}

void 
PgmScopeTree::Dump(std::ostream& os, int dmpFlags) const
{
  if (root) {
    root->DumpLineSorted(os, dmpFlags);
  }
}

void 
PgmScopeTree::DDump() const
{
  Dump();
}

/*****************************************************************************/
// ScopeType `methods' (could completely replace with dynamic typing)
/*****************************************************************************/

const char* ScopeInfo::ScopeNames[ScopeInfo::NUMBER_OF_SCOPES]  = {
  "PGM", "G", "LM", "F", "P", "L", "S", "ANY"
};

const char* 
ScopeInfo::ScopeTypeToName(ScopeType tp) 
{
  return ScopeNames[tp]; 
}

ScopeInfo::ScopeType 
ScopeInfo::IntToScopeType(long i) 
{
  BriefAssertion((i >= 0) && (i < NUMBER_OF_SCOPES)); 
  return (ScopeType) i; 
}

//***************************************************************************
// ScopeInfo, etc: constructors/destructors
//***************************************************************************

ScopeInfo::ScopeInfo(ScopeType t, ScopeInfo* mom) 
  : NonUniformDegreeTreeNode(mom), type(t)
{ 
  // trace = 1; 
  BriefAssertion((type == PGM) || (Pgm() == NULL) || !Pgm()->IsFrozen());
  static unsigned int uniqueId = 0; 
  uid = uniqueId++; 
}

static bool
OkToDelete(ScopeInfo *si) 
{
  PgmScope *pgm = si->Pgm(); 
  return ((pgm == NULL) || !(pgm->IsFrozen())); 
} 

ScopeInfo::~ScopeInfo() 
{
  BriefAssertion(OkToDelete(this)); 
  IFTRACE << "~ScopeInfo " << this << " " << ToString() << endl; 
}


CodeInfo::CodeInfo(ScopeType t, ScopeInfo* mom, suint begLn, suint endLn) 
  : ScopeInfo(t, mom), begLine(UNDEF_LINE), endLine(UNDEF_LINE)
{ 
  SetLineRange(begLn, endLn); 
}

CodeInfo::~CodeInfo() 
{
}


PgmScope::PgmScope() 
  : ScopeInfo(PGM, NULL) 
{ 
  frozen = false;
  name = (char *) NULL; 
  lmMap = new LoadModScopeMap();
}

PgmScope::~PgmScope() 
{
  frozen = false;
  delete lmMap;
}


GroupScope::GroupScope(const char* grpName,
		       CodeInfo *mom, int begLn, int endLn) 
  : CodeInfo(GROUP, mom, begLn, endLn)
{
  BriefAssertion(grpName);
  ScopeType t = (mom) ? mom->Type() : ANY;
  BriefAssertion((mom == NULL) || (t == PGM) || (t == GROUP) || (t == LM) 
		 || (t == FILE) || (t == PROC) || (t == LOOP));
  name = grpName;
}

GroupScope::~GroupScope()
{
}


LoadModScope::LoadModScope(const char* lmName, ScopeInfo* mom) 
  : CodeInfo(LM, mom, UNDEF_LINE, UNDEF_LINE) // ScopeInfo
{ 
  BriefAssertion(lmName);
  ScopeType t = (mom) ? mom->Type() : ANY;
  BriefAssertion((mom == NULL) || (t == PGM) || (t == GROUP));

  name = lmName;
  fileMap = new FileScopeMap(); 
}

LoadModScope::~LoadModScope() 
{
  delete fileMap;
}


FileScope::FileScope(const char* srcFileWithPath, 
		     bool srcIsReadble_, 
		     ScopeInfo *mom, suint begLn, suint endLn)
  : CodeInfo(FILE, mom, begLn, endLn)
{
  BriefAssertion(srcFileWithPath);
  ScopeType t = (mom) ? mom->Type() : ANY;
  BriefAssertion((mom == NULL) || (t == PGM) || (t == GROUP) || (t == LM));

  srcIsReadable = srcIsReadble_; 
  name = srcFileWithPath; 
  LoadMod()->AddToFileMap(*this); 
  procMap = new ProcScopeMap(); 
}

FileScope::~FileScope() 
{
  delete procMap;
}


ProcScope::ProcScope(const char* n, CodeInfo *mom, const char* ln, 
		     suint begLn, suint endLn) 
  : CodeInfo(PROC, mom, begLn, endLn)
{
  BriefAssertion(n);
  ScopeType t = (mom) ? mom->Type() : ANY;
  BriefAssertion((mom == NULL) || (t == GROUP) || (t == FILE));

  name = n; 
  linkname = ln;
  if (File()) File()->AddToProcMap(*this); 
}

ProcScope::~ProcScope() 
{
}


LoopScope::LoopScope(CodeInfo *mom, int begLn, int endLn, int _id) 
  : CodeInfo(LOOP, mom, begLn, endLn), id(_id)
{
  ScopeType t = (mom) ? mom->Type() : ANY;
  BriefAssertion((mom == NULL) || (t == GROUP) || (t == FILE) || (t == PROC) 
		 || (t == LOOP)); 
}

LoopScope::~LoopScope()
{
}

#if 0 // deprecated
bool LoopScope::ContainsLoop(loop *otherLoop)
{
  bb* h1 = eel_loop->loop_head();
  bb* h2 = otherLoop->loop_head();  
  mark m = new_mark();

  if (h1->dominates(h2)) {
    h1->visit(m); // When marking the loop, do not move out of this loop
    edge *e;
    FOREACH_EDGE(e, eel_loop->back_edges()) {
      e->mark_natural_loop(m);
    }
    if (h2->visited(m)) {
      return true;
    }
  }
  return false;
}
#endif


StmtRangeScope::StmtRangeScope(CodeInfo *mom, int begLn, int endLn, int _id)
  : CodeInfo(STMT_RANGE, mom, begLn, endLn), id(_id)
{
  ScopeType t = (mom) ? mom->Type() : ANY;
  BriefAssertion((mom == NULL) || (t == GROUP) || (t == FILE) || (t == PROC)
		 || (t == LOOP));
}

StmtRangeScope::~StmtRangeScope()
{
}

//***************************************************************************
// ScopeInfo: Ancestor
//***************************************************************************

#define dyn_cast_return(base, derived, expr) \
    { base* ptr = expr;  \
      if (ptr == NULL) {  \
        return NULL;  \
      } else {  \
	return dynamic_cast<derived*>(ptr);  \
      } \
    }

CodeInfo *
ScopeInfo::CodeInfoParent() const
{
  dyn_cast_return(ScopeInfo, CodeInfo, Parent()); 
}

ScopeInfo *
ScopeInfo::Ancestor(ScopeType tp) const
{
  const ScopeInfo *s = this; 
  while (s && s->Type() != tp) {
    s = s->Parent(); 
  } 
  return (ScopeInfo*) s;
} 

#if 0

int IsAncestorOf(ScopeInfo *parent, ScopeInfo *son, int difference)
{
  ScopeInfo *iter = son;
  while (iter && difference > 0 && iter != parent) {
    iter = iter->Parent();
    difference--;
  }
  if (iter && iter == parent)
     return 1;
  return 0;
}

#endif

ScopeInfo* 
ScopeInfo::LeastCommonAncestor(ScopeInfo* n1, ScopeInfo* n2)
{
  // Collect all ancestors of n1 and n2.  The root will be at the front
  // of the ancestor list.
  ScopeInfoList anc1, anc2;
  for (ScopeInfo* a = n1->Parent(); (a); a = a->Parent()) {
    anc1.push_front(a);
  }
  for (ScopeInfo* a = n2->Parent(); (a); a = a->Parent()) {
    anc2.push_front(a);
  }
  
  // Find the most deeply nested common ancestor
  ScopeInfo* lca = NULL;
  while ( (!anc1.empty() && !anc2.empty()) && (anc1.front() == anc2.front())) {
    lca = anc1.front();
    anc1.pop_front();
    anc2.pop_front();
  }
  
  return lca;
}

PgmScope*
ScopeInfo::Pgm() const 
{
  // iff this is called during ScopeInfo construction within the PgmScope 
  // construction dyn_cast does not do the correct thing
  if (Parent() == NULL) {
    // eraxxon: This cannot be a good thing to do!  Pgm() was being
    //   called on a LoopInfo object; then the resulting pointer
    //   (the LoopInfo itself) was queried for PgmInfo member data.  Ouch!
    // eraxxon: return (PgmScope*) this;
    return NULL;
  } else { 
    dyn_cast_return(ScopeInfo, PgmScope, Ancestor(PGM));
  }
}

GroupScope*
ScopeInfo::Group() const 
{
  dyn_cast_return(ScopeInfo, GroupScope, Ancestor(GROUP)); 
}

LoadModScope*
ScopeInfo::LoadMod() const 
{
  dyn_cast_return(ScopeInfo, LoadModScope, Ancestor(LM)); 
}

FileScope*
ScopeInfo::File() const 
{
  dyn_cast_return(ScopeInfo, FileScope, Ancestor(FILE));
}

ProcScope*
ScopeInfo::Proc() const 
{
  dyn_cast_return(ScopeInfo, ProcScope, Ancestor(PROC));
}

LoopScope*
ScopeInfo::Loop() const 
{
  dyn_cast_return(ScopeInfo, LoopScope, Ancestor(LOOP));
}

StmtRangeScope*
ScopeInfo::StmtRange() const 
{
  dyn_cast_return(ScopeInfo, StmtRangeScope, Ancestor(STMT_RANGE));
}


//***************************************************************************
// ScopeInfo: Tree Navigation
//***************************************************************************

CodeInfo* 
ScopeInfo::FirstEnclScope() const
{
  dyn_cast_return(NonUniformDegreeTreeNode, CodeInfo, FirstChild()); 
}

CodeInfo*
ScopeInfo::LastEnclScope() const
{
  dyn_cast_return(NonUniformDegreeTreeNode, CodeInfo, LastChild()); 
}

// ----------------------------------------------------------------------
// siblings are linked in a circular list
// Parent()->FirstChild()->PrevSibling() == Parent->LastChild() and 
// Parent()->LastChild()->NextSibling()  == Parent->FirstChild()
// ----------------------------------------------------------------------

CodeInfo*
ScopeInfo::NextScope() const
{
  // siblings are linked in a circular list, 
  NonUniformDegreeTreeNode* next = NULL; 
  if ((Parent()->LastChild() != this)) { 
    next = NextSibling(); 
  } 
  if (next) { 
    CodeInfo *ci = dynamic_cast<CodeInfo*>(next); 
    BriefAssertion(ci); 
    return ci; 
  }
  return NULL;  
}

CodeInfo*
ScopeInfo::PrevScope() const
{
  NonUniformDegreeTreeNode* prev = NULL; 
  if ((Parent()->FirstChild() != this)) { 
    prev = PrevSibling(); 
  } 
  if (prev) { 
    CodeInfo *ci = dynamic_cast<CodeInfo*>(prev); 
    BriefAssertion(ci); 
    return ci; 
  }
  return NULL; 
}

//***************************************************************************
// ScopeInfo: Paths and Merging
//***************************************************************************

int 
ScopeInfo::Distance(ScopeInfo* anc, ScopeInfo* desc)
{
  int distance = 0;
  for (ScopeInfo* x = desc; x != NULL; x = x->Parent()) {
    if (x == anc) {
      return distance;
    }
    ++distance;
  }

  // If we arrive here, there was no path between 'anc' and 'desc'
  return -1;
}

bool 
ScopeInfo::ArePathsOverlapping(ScopeInfo* lca, ScopeInfo* desc1, 
			       ScopeInfo* desc2)
{
  // Ensure that d1 is on the longest path
  ScopeInfo* d1 = desc1, *d2 = desc2;
  int dist1 = Distance(lca, d1);
  int dist2 = Distance(lca, d2);
  if (dist2 > dist1) {
    ScopeInfo* t = d1;
    d1 = d2;
    d2 = t;
  } 
  
  // Iterate over the longest path (d1 -> lca) searching for d2.  Stop
  // when x is NULL or lca.
  for (ScopeInfo* x = d1; (x && x != lca); x = x->Parent()) {
    if (x == d2) { 
      return true;
    }
  }
  
  // If we arrive here, we did not encounter d2.  Divergent.
  return false; 
}

bool
ScopeInfo::MergePaths(ScopeInfo* lca, ScopeInfo* toDesc, ScopeInfo* fromDesc)
{
  bool merged = false;
  // Should we verify that lca is really the lca?
  
  // Collect nodes along paths between 'lca' and 'toDesc', 'fromDesc'.
  // The descendent nodes will be at the end of the list.
  ScopeInfoList toPath, fromPath;
  for (ScopeInfo* x = toDesc; x != lca; x = x->Parent()) {
    toPath.push_front(x);
  }
  for (ScopeInfo* x = fromDesc; x != lca; x = x->Parent()) {
    fromPath.push_front(x);
  }
  BriefAssertion(toPath.size() > 0 && fromPath.size() > 0);
  
  // We merge from the deepest _common_ level of nesting out to lca
  // (shallowest).  
  ScopeInfoList::reverse_iterator toPathIt = toPath.rbegin();
  ScopeInfoList::reverse_iterator fromPathIt = fromPath.rbegin();
  
  // Determine which path is longer (of course, they can be equal) so
  // we can properly set up the iterators
  ScopeInfoList::reverse_iterator* lPathIt = &toPathIt; // long path iter
  int difference = toPath.size() - fromPath.size();
  if (difference < 0) {
    lPathIt = &fromPathIt;
    difference = -difference;
  }
  
  // Align the iterators
  for (int i = 0; i < difference; ++i) { (*lPathIt)++; }
  
  // Now merge the nodes in 'fromPath' into 'toPath'
  for ( ; fromPathIt != fromPath.rend(); ++fromPathIt, ++toPathIt) {
    ScopeInfo* from = (*fromPathIt);
    ScopeInfo* to = (*toPathIt);
    if (IsMergable(to, from)) {
      merged |= Merge(to, from);
    }
  }
  
  return merged;
}

bool 
ScopeInfo::Merge(ScopeInfo* toNode, ScopeInfo* fromNode)
{
  if (!IsMergable(toNode, fromNode)) {
    return false;
  }
  
  // Perform the merge
  // 1. Move all children of 'fromNode' into 'toNode'
  for (ScopeInfoChildIterator it(fromNode); it.Current(); /* */) {
    ScopeInfo* child = dynamic_cast<ScopeInfo*>(it.Current());
    it++; // advance iterator -- it is pointing at 'child'
    
    child->Unlink();
    child->Link(toNode);
  }
  
  // 2. If merging CodeInfos, update line ranges
  CodeInfo* toCI = dynamic_cast<CodeInfo*>(toNode);
  CodeInfo* fromCI = dynamic_cast<CodeInfo*>(fromNode);
  if (toCI && fromCI) {
    suint begLn = MIN(toCI->BegLine(), fromCI->BegLine());
    suint endLn = MAX(toCI->EndLine(), fromCI->EndLine());
    toCI->SetLineRange(begLn, endLn);
  }
  
  // 3. Unlink 'fromNode' from the tree and delete it
  fromNode->Unlink();
  delete fromNode;
  
  return true;
}

bool 
ScopeInfo::IsMergable(ScopeInfo* toNode, ScopeInfo* fromNode)
{
  // For now, merges are only defined on LOOPs and GROUPs
  ScopeInfo::ScopeType toTy = toNode->Type(), fromTy = fromNode->Type();
  bool goodTy = (toTy == ScopeInfo::LOOP || toTy == ScopeInfo::GROUP);
  return ((toTy == fromTy) && goodTy);
}


//***************************************************************************
// ScopeInfo, etc: Names, Name Maps, and Retrieval by Name
//***************************************************************************

void 
PgmScope::AddToLoadModMap(LoadModScope &lm) 
{
  String lmName = RealPath(lm.Name());
  // STL::map is a Unique Associative Container  
  BriefAssertion((*lmMap).count(lmName) == 0); 
  (*lmMap)[lmName] = &lm; 
}

void 
LoadModScope::AddToFileMap(FileScope &f) 
{
  String fName = RealPath(f.Name());
  // STL::map is a Unique Associative Container  
  BriefAssertion((*fileMap).count(fName) == 0); 
  (*fileMap)[fName] = &f; 
} 

void 
FileScope::AddToProcMap(ProcScope &p) 
{
  // STL::map is a Unique Associative Container
  bool duplicate = ((*procMap).count(p.Name()) != 0);
#ifdef SCOPE_TREE_DISALLOW_DUPLICATE_FUNCTION_NAMES_IN_FILE
  // We cannot tolerate any duplicates
  BriefAssertion(!duplicate && "Duplicate procedure added to file!");
#endif  
  if (!duplicate) { 
    (*procMap)[p.Name()] = &p;
  }
} 

LoadModScope*
PgmScope::FindLoadMod(const char* nm) const
{
  String lmName = RealPath(nm); 
  if ((*lmMap).count(lmName) != 0) 
    return (*lmMap)[lmName]; 
  return NULL; 
}

FileScope*
LoadModScope::FindFile(const char* nm) const
{
  String fName = RealPath(nm); 
  if ((*fileMap).count(fName) != 0) 
    return (*fileMap)[fName]; 
  return NULL; 
}

ProcScope*
FileScope::FindProc(const char* nm)  const
{
  if ((*procMap).count(nm) != 0) 
    return (*procMap)[nm]; 
  return NULL;
}

//***************************************************************************
// CodeInfo, etc: CodeName methods
//***************************************************************************

String
CodeInfo::CodeName() const
{
  FileScope *f = File(); 
  String name; 
  if (f != NULL) { 
    name = File()->BaseName() + ": " ;
  } 
  name += String(begLine);
  if (begLine != endLine) {
    name += "-" +  String(endLine) ; 
  }
  return name;
} 

String
GroupScope::CodeName() const 
{
  String result =  ScopeTypeToName(Type());
  result += " " + CodeInfo::CodeName();
  return result;
}

String
LoadModScope::CodeName() const 
{
  String result =  ScopeTypeToName(Type());
  result += " " + CodeInfo::CodeName();
  return result;
}

String
FileScope::CodeName() const 
{
  return BaseName(); 
}

String
ProcScope::CodeName() const 
{
  FileScope *f = File(); 
  String  cName = name + " ("; 
  if (f != NULL) { 
     cName += File()->BaseName() + ":" ;
  } 
  cName += String((long)begLine) + ")";
  return cName; 
} 

String
LoopScope::CodeName() const 
{
  String result =  ScopeTypeToName(Type());
  result += " " + CodeInfo::CodeName();
  return result;
} 

String
StmtRangeScope::CodeName() const 
{
  String result =  ScopeTypeToName(Type());
  result += " " + CodeInfo::CodeName();
  return result;
}

//***************************************************************************
// ScopeInfo, etc: Output and Debugging support 
//***************************************************************************

String 
ScopeInfo::ToDumpString(int dmpFlag)  const
{ 
  String self;
  self = String(ScopeTypeToName(Type()));
  if ((dmpFlag & PgmScopeTree::XML_TRUE) == PgmScopeTree::XML_FALSE) {
    self = self + " uid=" + String((long)UniqueId());
  }
  return self; 
}

String
CodeInfo::DumpLineRange(int dmpFlag) const
{
  String self = "b" + MakeAttrNum(begLine) + " e" + MakeAttrNum(endLine);
  return self;
}

String
CodeInfo::ToDumpString(int dmpFlag)  const
{ 
  String self = ScopeInfo::ToDumpString(dmpFlag) + " "+ DumpLineRange(dmpFlag);
  return self;
}

String
PgmScope::ToDumpString(int dmpFlag)  const
{ 
  String self = ScopeInfo::ToDumpString(dmpFlag) + " version=\"4.0\"";
  return self; 
}

String 
GroupScope::ToDumpString(int dmpFlag)  const
{
  String self = ScopeInfo::ToDumpString(dmpFlag) + " n" +
    MakeAttrStr(name, AddXMLEscapeChars(dmpFlag));
  return self;
}

String 
LoadModScope::ToDumpString(int dmpFlag)  const
{
  String self = ScopeInfo::ToDumpString(dmpFlag) + " n" +
    MakeAttrStr(name, AddXMLEscapeChars(dmpFlag));
  return self;
}

String
FileScope::ToDumpString(int dmpFlag)  const
{
  String self;
  if (dmpFlag & PgmScopeTree::XML_TRUE) {
    self = ScopeInfo::ToDumpString(dmpFlag); 
  } else {
    self = CodeInfo::ToDumpString(dmpFlag); 
  }
  self = self + " n" + MakeAttrStr(name, AddXMLEscapeChars(dmpFlag));
  return self; 
}

String 
ProcScope::ToDumpString(int dmpFlag)  const
{ 
  String self = ScopeInfo::ToDumpString(dmpFlag) +
    " n" + MakeAttrStr(name, AddXMLEscapeChars(dmpFlag));
  if (strcmp(name, linkname) != 0) { // if different, print both
    self = self + " ln" + MakeAttrStr(linkname, AddXMLEscapeChars(dmpFlag));
  }
  self = self + " " + DumpLineRange(dmpFlag);
  return self;
}

String 
LoopScope::ToDumpString(int dmpFlag)  const
{
  String self = CodeInfo::ToDumpString(dmpFlag); // + " i" + MakeAttr(id);
  return self;
}

String
StmtRangeScope::ToDumpString(int dmpFlag)  const
{
  String self = CodeInfo::ToDumpString(dmpFlag); // + " i" + MakeAttr(id);
  return self;
}

void 
ScopeInfo::DumpSelfBefore(ostream &os, int dmpFlag, const char *prefix) const
{
  os << prefix << "<" << ToDumpString(dmpFlag);
  if ((dmpFlag & PgmScopeTree::XML_TRUE) 
      && (dmpFlag & PgmScopeTree::XML_EMPTY_TAG)) { 
    os << "/>"; 
  } else { 
    os << ">"; 
  }
  if (!(dmpFlag & PgmScopeTree::COMPRESSED_OUTPUT)) { os << endl; }
}

void
ScopeInfo::DumpSelfAfter(ostream &os, int dmpFlag, const char *prefix) const
{
  if ((dmpFlag & PgmScopeTree::XML_TRUE) 
      && !(dmpFlag & PgmScopeTree::XML_EMPTY_TAG)) {
    os << prefix << "</" << String(ScopeTypeToName(Type())) << ">";
    if (!(dmpFlag & PgmScopeTree::COMPRESSED_OUTPUT)) { os << endl; }
  } 
}

void
ScopeInfo::Dump(ostream &os, int dmpFlag, const char *pre) const 
{
  String indent = "   ";
  if (dmpFlag & PgmScopeTree::COMPRESSED_OUTPUT) { pre = ""; indent = ""; }  
  if ((dmpFlag & PgmScopeTree::XML_TRUE) && IsLeaf()) { 
    dmpFlag |= PgmScopeTree::XML_EMPTY_TAG; 
  }
  
  DumpSelfBefore(os, dmpFlag, pre); 
  String prefix = String(pre) + indent;
  for (ScopeInfoChildIterator it(this); it.Current(); it++) {
    it.CurScope()->Dump(os, dmpFlag, prefix); 
  }
  DumpSelfAfter(os, dmpFlag, pre);
}

// circumvent pain caused by debuggers that choke on default arguments
// or that remove all traces of functions defined in the class declaration.
void
ScopeInfo::DDump()
{ 
  Dump(std::cerr, PgmScopeTree::XML_TRUE, "");
}

void
ScopeInfo::DDumpSort() 
{ 
  DumpLineSorted(std::cerr, PgmScopeTree::XML_TRUE, "");
}

void
ScopeInfo::DumpLineSorted(ostream &os, int dmpFlag, const char *pre) const 
{
  String indent = "   ";
  if (dmpFlag & PgmScopeTree::COMPRESSED_OUTPUT) { pre = ""; indent = ""; }  
  if ((dmpFlag & PgmScopeTree::XML_TRUE) && IsLeaf()) { 
    dmpFlag |= PgmScopeTree::XML_EMPTY_TAG; 
  }
  
  DumpSelfBefore(os, dmpFlag, pre); 
  String prefix = String(pre) + indent;
  for (ScopeInfoLineSortedChildIterator it(this); it.Current(); it++) {
    it.Current()->DumpLineSorted(os, dmpFlag, prefix);
  }   
  DumpSelfAfter(os, dmpFlag, pre);
}

void
PgmScope::DumpLineSorted(ostream &os, int dmpFlag, const char *pre) const
{
  String indent = "   ";
  if (dmpFlag & PgmScopeTree::COMPRESSED_OUTPUT) { pre = ""; indent = ""; }  

  ScopeInfo::DumpSelfBefore(os, dmpFlag, pre);
  String prefix = String(pre) + indent;  
  for (ScopeInfoNameSortedChildIterator pgmIt(this); pgmIt.Current(); 
       pgmIt++) { 
#if 0
    // FIXME: The following will be true for 'bloop', but not
    // necessarily for all possible trees (e.g., this could be a GroupScope)
    FileScope* file = dynamic_cast<FileScope*>(pgmIt.Current());
    BriefAssertion(file); 

    file->DumpLineSorted(os, dmpFlag, prefix);
#else
    ScopeInfo* scope = pgmIt.Current();
    scope->DumpLineSorted(os, dmpFlag, prefix);
#endif
  }
  ScopeInfo::DumpSelfAfter(os, dmpFlag, pre);
}

String 
ScopeInfo::Types() 
{
  String types; 
  if (dynamic_cast<ScopeInfo*>(this)) {
    types += "ScopeInfo "; 
  } 
  if (dynamic_cast<CodeInfo*>(this)) {
    types += "CodeInfo "; 
  } 
  if (dynamic_cast<PgmScope*>(this)) {
    types += "PgmScope "; 
  } 
  if (dynamic_cast<GroupScope*>(this)) {
    types += "GroupScope "; 
  } 
  if (dynamic_cast<LoadModScope*>(this)) {
    types += "LoadModScope "; 
  } 
  if (dynamic_cast<FileScope*>(this)) {
    types += "FileScope "; 
  } 
  if (dynamic_cast<ProcScope*>(this)) {
    types += "ProcScope "; 
  } 
  if (dynamic_cast<LoopScope*>(this)) {
    types += "LoopScope "; 
  } 
  if (dynamic_cast<StmtRangeScope*>(this)) {
    types += "StmtRangeScope "; 
  } 
  return types; 
} 


//***************************************************************************
// CodeInfo specific methods 
//***************************************************************************

void 
CodeInfo::SetLineRange(suint begLn, suint endLn) 
{
  // Sanity Checking
  BriefAssertion(begLn <= endLn);   // begLn <= endLn

  if (begLn == UNDEF_LINE) {
    BriefAssertion(endLn == UNDEF_LINE);
    // simply relocate at beginning of sibling list 
    // no range update in parents is necessary
    BriefAssertion((begLine == UNDEF_LINE) && (endLine == UNDEF_LINE)); 
    if (Parent() != NULL) Relocate(); 
  } else {
    bool changed = false; 
    if (begLine == UNDEF_LINE) {
      BriefAssertion(endLine == UNDEF_LINE); 
      // initialize range
      begLine = begLn; 
      endLine = endLn; 
      changed = true;
    } else {
      BriefAssertion((begLine != UNDEF_LINE) && (endLine != UNDEF_LINE));
      // expand range ?
      if (begLn < begLine) { begLine = begLn; changed = true; }
      if (endLn > endLine) { endLine = endLn; changed = true; }
    }
    CodeInfo *mom = CodeInfoParent(); 
    if (changed && (mom != NULL)) {
      Relocate(); 
      mom->SetLineRange(begLine, endLine); 
    }
  }
}
  
void
CodeInfo::Relocate() 
{
  CodeInfo *prev = PrevScope(); 
  CodeInfo *next = NextScope(); 
  if (((!prev) || (prev->endLine <= begLine)) && 
      ((!next) || (next->begLine >= endLine))) {
     return; 
  } 
  ScopeInfo *mom = Parent(); 
  Unlink(); 
  if (mom->FirstChild() == NULL) {
    Link(mom); 
  }
  else if (begLine == UNDEF_LINE) {
    // insert as first child
    LinkBefore(mom->FirstChild()); 
  } else {
    // insert after sibling with sibling->endLine < begLine 
    // or iff that does not exist insert as first in sibling list
    CodeInfo *sibling = NULL;
    for (sibling = mom->LastEnclScope(); sibling; 
	 sibling = sibling->PrevScope()) {
      if (sibling->endLine < begLine)  
	break; 
    } 
    if (sibling != NULL) {
      LinkAfter(sibling); 
    } else {
      LinkBefore(mom->FirstChild()); 
    } 
  }
}

bool
CodeInfo::ContainsLine(suint ln)  const
{
   BriefAssertion(ln != UNDEF_LINE); 
   if (Type() == FILE) {
     return true; 
   } 
   return ((begLine >= 1) && (begLine <= ln) && (ln <= endLine)); 
} 

CodeInfo* 
CodeInfo::CodeInfoWithLine(suint ln)  const
{
   BriefAssertion(ln != UNDEF_LINE); 
   CodeInfo *ci; 
   // ln > endLine means there is no child that contains ln
   if (ln <= endLine) {
     for (ScopeInfoChildIterator it(this); it.Current(); it++) {
       ci = dynamic_cast<CodeInfo*>(it.Current()); 
       BriefAssertion(ci); 
       if  (ci->ContainsLine(ln)) {
	 return ci->CodeInfoWithLine(ln); 
       } 
     }
   }
   return (CodeInfo*) this; // base case
}

int CodeInfoLineComp(CodeInfo* x, CodeInfo* y)
{
  if (x->BegLine() == y->BegLine()) {
    // Given two CodeInfo's with identical endpoints consider two
    // special cases:
    bool endLinesEqual = (x->EndLine() == y->EndLine());
    
    // 1. If a ProcScope, use a lexiocraphic compare
    if (endLinesEqual
	&& (x->Type() == ScopeInfo::PROC && y->Type() == ScopeInfo::PROC)) {
      ProcScope *px = ((ProcScope*)x), *py = ((ProcScope*)y);
      int ret1 = strcmp(px->Name(), py->Name());
      if (ret1 != 0) { return ret1; }
      int ret2 = strcmp(px->LinkName(), py->LinkName());
      if (ret2 != 0) { return ret2; }
    }

    // 2. Otherwise: rank a leaf node before a non-leaf node
    if (endLinesEqual && !(x->IsLeaf() && y->IsLeaf())) {
      if      (x->IsLeaf()) { return -1; } // x < y 
      else if (y->IsLeaf()) { return 1; }  // x > y  
    }
    
    // 3. General case
    return SimpleLineCmp(x->EndLine(), y->EndLine()); 
  } else {
    return SimpleLineCmp(x->BegLine(), y->BegLine());
  }
}

// - if x < y; 0 if x == y; + otherwise
int SimpleLineCmp(suint x, suint y)
{
  // We would typically wish to use the following for this simple
  // comparison, but it fails if the the differences are greater than
  // an 'int'
  // return (x - y)

  if (x < y)       { return -1; }
  else if (x == y) { return 0; }
  else             { return 1; }
}

// Returns the string with all necessary characters escaped; will
// not modify 'str'
int AddXMLEscapeChars(int dmpFlag)
{
  if ((dmpFlag & PgmScopeTree::XML_TRUE) &&
      !(dmpFlag & PgmScopeTree::XML_NO_ESC_CHARS)) {
    return xml::ESC_TRUE;
  } else {
    return xml::ESC_FALSE;
  }
}

//***************************************************************************

#if 0
void 
ScopeInfoTester(int argc, const char** argv) 
{
  PgmScope *root = new PgmScope("ScopeInfoTester"); 
  LoadModScope *lmScope = new LoadModScope("load module", root); 
  FileScope *file; 
  ProcScope *proc; 
  LoopScope *loop;
  GroupScope *group;
  StmtRangeScope *stmtRange; 
  
  FILE *srcFile = fopen("file.c", "r");
  bool known = (srcFile != NULL); 
  if (srcFile) {
    fclose(srcFile); 
  }  
  
  file = new FileScope("file.c", known, lmScope); 
  proc = new ProcScope("proc", file); 
  loop = new LoopScope(proc->CodeInfoWithLine(2), 2, 10); 
  loop = new LoopScope(proc->CodeInfoWithLine(5), 5, 9);  
  loop = new LoopScope(proc->CodeInfoWithLine(12), 12, 25); 

  stmtRange = new StmtRangeScope(proc->CodeInfoWithLine(4), 4, 4); 
  stmtRange = new StmtRangeScope(proc->CodeInfoWithLine(3), 3, 3); 
  stmtRange = new StmtRangeScope(proc->CodeInfoWithLine(5), 5, 5); 
  stmtRange = new StmtRangeScope(proc->CodeInfoWithLine(13), 13, 13); 

  group = new GroupScope("g1", proc->CodeInfoWithLine(14), 14, 18);
  stmtRange = new StmtRangeScope(proc->CodeInfoWithLine(15), 15, 15);
  
  cout << "root->Dump()" << endl; 
  root->Dump(); 
  cout << "------------------------------------------" << endl << endl; 
  
  cout << "Type Casts" << endl; 
  {
    ScopeInfo* sinfo[10]; 
    int sn = 0; 
    sinfo[sn++] = root; 
    sinfo[sn++] = file; 
    sinfo[sn++] = proc; 
    sinfo[sn++] = loop; 
    sinfo[sn++] = group; 
    sinfo[sn++] = stmtRange; 
    
    for (int i = 0; i < sn; i++) {
      cout << sinfo[i]->ToDumpString(false) << "::\t" ; 
      cout << sinfo[i]->Types() << endl; 
      cout << endl; 
    } 
    cout << endl; 
  } 
  cout << "------------------------------------------" << endl << endl; 
  
  cout << "Iterators " << endl; 
  { 
    FileScope *file_c = lmScope->FindFile("file.c"); 
    BriefAssertion(file_c); 
    ProcScope *proc = file_c->FindProc("proc"); 
    BriefAssertion(proc); 
    CodeInfo *loop3 = proc->CodeInfoWithLine(12);
    BriefAssertion(loop3); 
    
    {
      cerr << "*** everything under root " << endl; 
      ScopeInfoIterator it(root); 
      for (; it.CurScope(); it++) {
	CodeInfo *ci = dynamic_cast<CodeInfo*>(it.CurScope());
	cout << it.CurScope()->ToDumpString(false); 
	if (ci != NULL) { 
	  cout << " Name=" << ci->Name(); 
	} 
	cout << endl; 
      }  
    }

    { 
      cerr << "*** file.c in line order" << endl;  
      LineSortedIterator it(file_c, NULL, false); 
      it.DumpAndReset(); 
      cerr << "***" << endl << endl;
    }
    
    { 
      cerr << "*** loop3's children in line order" << endl;  
      LineSortedChildIterator it(loop3); 
      it.DumpAndReset(); 
      cerr << "***" << endl << endl; 
    }
  }
  cout << "------------------------------------------" << endl << endl; 

  delete root; 
} 
#endif

#ifdef NEVER 
The following is a 'correct' input (file.c) for the statically build tree 
from ScopeInfoTester()
     1  proc p() 
     2    do i = 1,10
     3      line(3) =     Line(3+10) * LINE(3+356)
     4      line(4) = 2390
     5      do j = 5, 9
     6    
     7   
     8   
     9      enddo 
    10    enddo
    11    
    12    do k = 12, 25
    13       atheline(13)
    14  
    15       line(15)        C comment comment comment long lon wrap around ...
    16      
    17
    18
    19
    20
    21
    22
    23
    24
    25    enddo 
    26
    27
    28
#endif 
