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

//***************************************************************************
//
// File:
//    DCPIMetricDesc.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include "DCPIMetricDesc.h"

#include <lib/support/String.h>
#include <lib/support/Assertion.h>

//*************************** Forward Declarations ***************************

using std::endl;
using std::hex;
using std::dec;

int
SetDCPIMetricDescBit(const char* token, DCPIMetricDesc& m);

struct DCPITranslationTableEntry {
  const char* tok;
  DCPIMetricDesc::bitvec_t bits;
};

DCPITranslationTableEntry*
FindDCPITranslationTableEntry(const char* token);

//****************************************************************************
// DCPIMetricDesc
//****************************************************************************

DCPIMetricDesc::DCPIMetricDesc(const char* str)
{
  DCPIMetricDesc m = String2DCPIMetricDesc(str);
  bits = m.bits;
}

// String2DCPIMetricDesc: See header for accepted syntax.  If an error
// occurs, the DCPIMetricDesc::IsValid() will return false.
DCPIMetricDesc
String2DCPIMetricDesc(const char* str)
{
  DCPIMetricDesc m; // initialized to invalid state

  // 0. Test for NULL or empty string
  if (!str || strlen(str) == 0) {
    return m;
  }

  // 1. Special recursive case (should only happen once)
  //    str ::= ProfileMe_sample_set:ProfileMe_counter
  // Note: cannot use strtok because of recursion.
  char* sep = const_cast<char*>(strchr(str, ':')); // separator
  if (sep != NULL) {
    *sep = '\0'; // temporarily modify to get first part
    String sampleset = str;
    *sep = ':';
    String counter = sep+1;
    DCPIMetricDesc m1 = String2DCPIMetricDesc(sampleset);
    DCPIMetricDesc m2 = String2DCPIMetricDesc(counter);
	
    if (m1.IsValid() && m2.IsValid()) {
      m.Set(m1);
      m.Set(m2);
    } 
    return m;
  }
  
  // 2. Typical case: 
  //    str ::= ProfileMe_sample_set | ProfileMe_counter | Regular_counter
  //
  // ProfileMe_sample_set may contain multiple tokens (divided by '^'); 
  // otherwise we can only distinguish which type we have by
  // string matching.  Fortunately, this can be a very simple
  // implementation because attribute and counter names are unique.

  DCPIMetricDesc m1;
  char* tok = strtok(const_cast<char*>(str), "^");
  while (tok != NULL) {
    int ret = SetDCPIMetricDescBit(tok, m1);
    if (ret != 0) { return m; }
    
    tok = strtok((char*)NULL, "^");
  }
  m.Set(m1);
  
  return m;
}


// SetDCPIMetricDescBit: Given a DCPI bit value (possibly preceeded by
// ! for negation) or counter name token string, set the appropriate
// bit in 'm'.  Returns non-zero on error.
int
SetDCPIMetricDescBit(const char* token, DCPIMetricDesc& m)
{
  // 0. Test for NULL or empty string
  if (!token || strlen(token) == 0) {
    return 1;
  }
  
  // 1. Set the appropriate bit by string comparisons
  DCPITranslationTableEntry* e = FindDCPITranslationTableEntry(token);
  if (!e) { return 1; }
  m.Set(e->bits);
  
  return 0;
}

//****************************************************************************

#define TYPEPM(m) DCPI_MTYPE_PM | (m)
#define TYPERM(m) DCPI_MTYPE_RM | (m)

#define DCPI_PM_CNTR(pfx, n1, n2, n3, cbit1, cbit2, cbit3) \
  { pfx##n1, TYPEPM((cbit1)) }, \
  { pfx##n2, TYPEPM((cbit2)) }, \
  { pfx##n3, TYPEPM((cbit3)) }

#define DCPI_PM_BIT(name, tbit, fbit) \
  { name,      TYPEPM((tbit)) }, \
  { "!"##name, TYPEPM((fbit)) }


#define DCPITranslationTableSZ \
   sizeof(DCPITranslationTable) / sizeof(DCPITranslationTableEntry)

static DCPITranslationTableEntry DCPITranslationTable[] = {
  // ProfileMe counters
  DCPI_PM_CNTR("m0", "count", "inflight", "retires", DCPI_PM_CNTR_count, DCPI_PM_CNTR_inflight, DCPI_PM_CNTR_retires), 
  DCPI_PM_CNTR("m1", "count", "inflight", "retdelay", DCPI_PM_CNTR_count, DCPI_PM_CNTR_inflight, DCPI_PM_CNTR_retdelay), 
  DCPI_PM_CNTR("m2", "count", "retires", "bcmisses", DCPI_PM_CNTR_count, DCPI_PM_CNTR_retires, DCPI_PM_CNTR_bcmisses), 
  DCPI_PM_CNTR("m3", "count", "inflight", "replays", DCPI_PM_CNTR_count, DCPI_PM_CNTR_inflight, DCPI_PM_CNTR_replays), 

  // ProfileMe instruction attributes
  DCPI_PM_BIT("retired", DCPI_PM_ATTR_retired_T, DCPI_PM_ATTR_retired_F),
  DCPI_PM_BIT("taken", DCPI_PM_ATTR_taken_T, DCPI_PM_ATTR_taken_F),
  DCPI_PM_BIT("cbrmispredict", DCPI_PM_ATTR_cbrmispredict_T, DCPI_PM_ATTR_cbrmispredict_F),
  DCPI_PM_BIT("valid", DCPI_PM_ATTR_valid_T, DCPI_PM_ATTR_valid_F),
  DCPI_PM_BIT("nyp", DCPI_PM_ATTR_nyp_T, DCPI_PM_ATTR_nyp_F),
  DCPI_PM_BIT("ldstorder", DCPI_PM_ATTR_ldstorder_T, DCPI_PM_ATTR_ldstorder_F),
  DCPI_PM_BIT("map_stall", DCPI_PM_ATTR_map_stall_T, DCPI_PM_ATTR_map_stall_F),
  DCPI_PM_BIT("early_kill", DCPI_PM_ATTR_early_kill_T, DCPI_PM_ATTR_early_kill_F),
  DCPI_PM_BIT("late_kill", DCPI_PM_ATTR_late_kill_T, DCPI_PM_ATTR_late_kill_F),
  DCPI_PM_BIT("capped", DCPI_PM_ATTR_capped_T, DCPI_PM_ATTR_capped_F),
  DCPI_PM_BIT("twnzrd", DCPI_PM_ATTR_twnzrd_T, DCPI_PM_ATTR_twnzrd_F),
  
  // ProfileMe instruction traps  
  DCPI_PM_BIT("notrap", DCPI_PM_TRAP_notrap_T, DCPI_PM_TRAP_notrap_F),
  DCPI_PM_BIT("mispredict", DCPI_PM_TRAP_mispredict_T, DCPI_PM_TRAP_mispredict_F),
  DCPI_PM_BIT("replays", DCPI_PM_TRAP_replays_T, DCPI_PM_TRAP_replays_F),
  DCPI_PM_BIT("unaligntrap", DCPI_PM_TRAP_unaligntrap_T, DCPI_PM_TRAP_unaligntrap_F),
  DCPI_PM_BIT("dtbmiss", DCPI_PM_TRAP_dtbmiss_T, DCPI_PM_TRAP_dtbmiss_F),
  DCPI_PM_BIT("dtb2miss3", DCPI_PM_TRAP_dtb2miss3_T, DCPI_PM_TRAP_dtb2miss3_F),
  DCPI_PM_BIT("dtb2miss4", DCPI_PM_TRAP_dtb2miss4_T, DCPI_PM_TRAP_dtb2miss4_F),
  DCPI_PM_BIT("itbmiss", DCPI_PM_TRAP_itbmiss_T, DCPI_PM_TRAP_itbmiss_F),
  DCPI_PM_BIT("arithtrap", DCPI_PM_TRAP_arithtrap_T, DCPI_PM_TRAP_arithtrap_F),
  DCPI_PM_BIT("fpdisabledtrap", DCPI_PM_TRAP_fpdisabledtrap_T, DCPI_PM_TRAP_fpdisabledtrap_F),
  DCPI_PM_BIT("MT_FPCRtrap", DCPI_PM_TRAP_MT_FPCRtrap_T, DCPI_PM_TRAP_MT_FPCRtrap_F),
  DCPI_PM_BIT("dfaulttrap", DCPI_PM_TRAP_dfaulttrap_T, DCPI_PM_TRAP_dfaulttrap_F),
  DCPI_PM_BIT("iacvtrap", DCPI_PM_TRAP_iacvtrap_T, DCPI_PM_TRAP_iacvtrap_F),
  DCPI_PM_BIT("OPCDECtrap", DCPI_PM_TRAP_OPCDECtrap_T, DCPI_PM_TRAP_OPCDECtrap_F),
  DCPI_PM_BIT("interrupt", DCPI_PM_TRAP_interrupt_T, DCPI_PM_TRAP_interrupt_F),
  DCPI_PM_BIT("mchktrap", DCPI_PM_TRAP_mchktrap_T, DCPI_PM_TRAP_mchktrap_F),
  
  // Non-ProfileMe event types
  { "cycles", TYPERM(DCPI_RM_cycles) },
  { "retires", TYPERM(DCPI_RM_retires) },
  { "replaytrap", TYPERM(DCPI_RM_replaytrap) },
  { "bmiss", TYPERM(DCPI_RM_bmiss) }

};

#undef TYPEPM
#undef TYPERM
#undef DCPI_PM_CNTR
#undef DCPI_PM_BIT


DCPITranslationTableEntry*
FindDCPITranslationTableEntry(const char* token)
{
  // FIXME: we should search a quick-sorted table with binary search.
  DCPITranslationTableEntry* found = NULL;
  for (uint i = 0; i < DCPITranslationTableSZ; ++i) {
    if (strcmp(token, DCPITranslationTable[i].tok) == 0) {
      found = &DCPITranslationTable[i];
    }
  }
  return found;
}

//****************************************************************************
