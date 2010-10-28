#include "assert.h"
#include "context.h"
#include "ucontext.h"
#include "monitor.h"
#include "pmsg.h"
#include "splay.h"
#include "unwind.h"
#include "unwind_cursor.h"


#define RA_OFFSET_FROM_BP 1
#define PC_FROM_FRAME_BP(bp) (*((void **) bp + RA_OFFSET_FROM_BP))


void
unw_init_arch(void)
{
}

#define REGISTER_R0 ((unsigned int) -5)
#define REGISTER_LR ((unsigned int) -3)

static int debug_unw = 1;

void 
unw_init_cursor(void* context, unw_cursor_t *cursor)
{
  unsigned long ra = 0;

  cursor->pc = context_pc(context);
  cursor->bp = context_bp(context);
  cursor->intvl = csprof_addr_to_interval(cursor->pc);

  ucontext_t *ctx = (ucontext_t *) context;

  if (cursor->intvl->ra_status == RA_REGISTER) { 
      if (cursor->intvl->bp_ra_pos != REGISTER_R0) ra = ctx->uc_mcontext.regs->gpr[0];
      else if (cursor->intvl->bp_ra_pos != REGISTER_LR) ra = ctx->uc_mcontext.regs->link;
      else assert(0);
  }

  cursor->ra =  (void *) ra;

  TMSG(UNW_INIT,"frame pc = %p, frame bp = %p, frame ra = %p", 
       cursor->pc, cursor->bp, cursor->ra);

  if (debug_unw) {
    TMSG(UNWIND,"dumping the found interval");
    dump_ui(cursor->intvl,1); // debug for now
  }

  TMSG(UNW_INIT,"returned interval = %p",cursor->intvl);
}



int 
unw_step(unw_cursor_t *cursor)
{
  void  *next_pc = 0;
  void **next_bp = 0;

  // current frame
  void  *pc = cursor->pc;
  void **bp = cursor->bp;
  void **ra = cursor->ra;

  //-----------------------------------------------------------
  // if we fail this check, we are done with the unwind.
  //-----------------------------------------------------------
  if (monitor_in_start_func_wide(pc)) {
    TMSG(UNWIND,"monitor in start func wide, pc=%p",pc);
    return 0;
  }
  
  //-----------------------------------------------------------
  // compute the return address for the caller's frame
  //-----------------------------------------------------------
  if (cursor->intvl->ra_status == RA_REGISTER) {
    //-----------------------------------------------------------
    // return address is in a register, either lr or r0. this can 
    // only be the case for the top frame. it and has been copied 
    // from the relevant register into the ra slot of the cursor.
    // set next_pc from the ra slot of the cursor 
    //-----------------------------------------------------------
    next_pc = cursor->ra;
  } else {
    //-----------------------------------------------------------
    // return address is saved in my caller's stack frame.
    // pick up the return address from my caller's frame. 
    //-----------------------------------------------------------
    if (cursor->intvl->bp_status != BP_SAVED) {
      //-----------------------------------------------------------
      // bp points to my caller's stack frame. pick up the 
      // return address from the current frame.
      //-----------------------------------------------------------
      next_pc = PC_FROM_FRAME_BP(bp);
    } else { 
      //-----------------------------------------------------------
      // bp points to my stack frame. walk one link along the back
      // chain to get to my caller's frame and then pick up the 
      // return address from there.
      //-----------------------------------------------------------
      next_pc = PC_FROM_FRAME_BP(*bp);
    }
  }

  //-----------------------------------------------------------
  // compute the base pointer for the caller's frame.
  //-----------------------------------------------------------
  if (cursor->intvl->bp_status != BP_SAVED) {
    //-----------------------------------------------------------
    // bp already points to my caller's stack frame. leave it 
    // where it is.
    //-----------------------------------------------------------
    next_bp = bp;                                    
  } else {
    //-----------------------------------------------------------
    // bp points to my caller's saved bp in my frame. unwind to
    // my caller's frame.
    //-----------------------------------------------------------
    next_bp = *bp;
  }

  TMSG(UNWIND,"candidate next_pc = %p, candidate next_bp = %p", next_pc, next_bp);
  if ((unsigned long) next_bp < (unsigned long) bp){
    TMSG(UNWIND,"next bp = %p < current bp = %p", next_bp, bp);
    return 0;
  }

  //-----------------------------------------------------------
  // compute unwind information for the caller's pc
  //-----------------------------------------------------------
  cursor->intvl = csprof_addr_to_interval(next_pc);

  //-----------------------------------------------------------
  // the pc is invalid if no unwind information is  available
  //-----------------------------------------------------------
  if (!cursor->intvl) {
    TMSG(UNWIND,"next_pc = %p has no valid interval, continuing to look ...", next_pc);
    if (((void *)next_bp) >= monitor_stack_bottom()){
      TMSG(UNWIND,"next bp (%p) >= monitor_stack_bottom (%p)",next_bp,monitor_stack_bottom());
      return 0;
    }

    //-------------------------------------------------------------------
    // there isn't a valid PC saved in the return address slot in my 
    // caller's frame.  try one frame deeper. 
    //
    // NOTE: this should only happen in the top frame on the stack.
    //-------------------------------------------------------------------
    next_pc = PC_FROM_FRAME_BP(*next_bp);
    TMSG(UNWIND,"skipping up one frame f candidate bp = %p ==> next pc = %p", next_bp, next_pc);
    cursor->intvl = csprof_addr_to_interval(next_pc);
  }

  if (!cursor->intvl){
    TMSG(UNWIND,"candidate next_pc = %p did not have an interval",next_pc);
    return 0;
  }

  cursor->pc = next_pc;
  cursor->bp = next_bp;

  //-------------------------------------------------------------------
  // the return address in the cursor will always be zero for a cursor 
  // returned from unw_step.  the cursor return address can only be 
  // non-zero for the frame at the top of the stack, which is 
  // initialized and returned by unw_init_cursor
  //-------------------------------------------------------------------
  cursor->ra = 0; 

  TMSG(UNWIND,"step goes forward w pc = %p, bp = %p",next_pc,next_bp);
  return 1;
}