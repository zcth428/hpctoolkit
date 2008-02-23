#include "pmsg.h"
#include "find.h"

extern int nm_bound(unsigned long pc, unsigned long *st, unsigned long *e);

int 
find_enclosing_function_bounds_v(char *addr, char **start, char **end,int verbose)
{
  int failure = 0;
  int nmb = nm_bound((unsigned long)addr,(unsigned long *)start,
		     (unsigned long *)end);

  if (nmb){
    PMSG(FIND,"FIND:found in nm table for %p: start=%p,end=%p\n",
	 addr, *start, *end);
  }
  else {
    if (verbose){
      PMSG(FIND,"FIND:look up failed @ %p!\n",addr); // FIXME: change to EMSG??
    }
    failure = 1;
  }
  return failure;
}
