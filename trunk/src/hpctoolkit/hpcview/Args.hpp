// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
//   $Source$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef Args_h
#define Args_h

//************************* System Include Files ****************************

#include <iostream>
#include <string>
#include <list>

//*************************** User Include Files ****************************

#include <include/general.h>
#include <lib/support/CmdLineParser.hpp>

//*************************** Forward Declarations **************************

extern int fileTrace;

//***************************************************************************

class Args {
public: 
  Args(); 
  Args(int argc, const char* const argv[]);
  ~Args(); 

  // Parse the command line
  void Parse(int argc, const char* const argv[]);

  // Version and Usage information
  void PrintVersion(std::ostream& os) const;
  void PrintUsage(std::ostream& os) const;
  
  // Error
  void PrintError(std::ostream& os, const char* msg) const;
  void PrintError(std::ostream& os, const std::string& msg) const;

  // Dump
  void Dump(std::ostream& os = std::cerr) const;
  void DDump() const;

public:  
  static const std::string HPCTOOLKIT;
  std::string hpcHome;

  // Parsed Data: Command
  const std::string& GetCmd() const;

  // Parsed Data: optional arguments
  std::string dbDir;
  std::string OutFilename_XML; // disable: "no", stdout: "-"
  std::string OutFilename_CSV; // disable: ""  , stdout: "-"
  std::string OutFilename_TSV; // disable: ""  , stdout: "-"

  bool CopySrcFiles;
  bool XML_DumpAllMetrics;

  int deleteUnderscores;

  // Parsed Data: arguments
  std::string configurationFile;
  //TODO std::list<std::string> profileFiles;

private:
  void Ctor();
  void setHPCHome(); 

private:
  static CmdLineParser::OptArgDesc optArgs[];
  CmdLineParser parser;
}; 

#endif
