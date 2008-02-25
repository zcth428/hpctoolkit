#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/mman.h>           /* for mmap() */
#include <sys/stat.h>
#include <sys/types.h>          /* for pthreads */

/* user include files */

#include "mem.h"
#include "pmsg.h"
#include "thread_data.h"

/* various convenience issues */

static csprof_mem_t MEM;

static const offset_t CSPROF_MEM_INIT_SZ     = 2 * 1024 * 1024; // 2 Mb

/* this is rarely used, so the default size is small */
static const offset_t CSPROF_MEM_INIT_SZ_TMP = 128 * 1024;  // 128 Kb

/* public interface */

/* the system malloc is good about rounding odd amounts to be aligned.
   we need to do the same thing. */
static size_t csprof_align_malloc_request(size_t size){
  /* might need to be different for 32-bit vs. 64-bit */
  return (size + (8 - 1)) & (~(8 - 1));
}

// See header file for documentation of public interface.

csprof_mem_t *csprof_malloc_init(offset_t sz, offset_t sz_tmp){

  csprof_mem_t *memstore = alloc_memstore();

  int status;

  if(memstore == NULL) { return NULL; }
  if(sz == 1) { sz = CSPROF_MEM_INIT_SZ; }
  if(sz_tmp == 1) { sz_tmp = CSPROF_MEM_INIT_SZ_TMP; }
  
  // MSG(1,"csprof malloc init calls csprof_mem__init");
  status = csprof_mem__init(memstore, sz, sz_tmp);

  if(status == CSPROF_ERR) {
    return NULL;
  }

  // MSG(1,"malloc_init returns %p",memstore);
  return memstore;
}

#ifdef NO
// See header file for documentation of public interface.
int csprof_malloc_fini(csprof_mem_t *memstore){
  if(csprof_mem__get_status(memstore) != CSPROF_MEM_STATUS_INIT) {
    return CSPROF_ERR;
  }
  
  return csprof_mem__fini(memstore);
}
#endif

void *csprof_malloc(size_t size){
  csprof_mem_t *memstore = TD_GET(memstore);

  // MSG(1,"csprof malloc memstore = %p",memstore);
  return csprof_malloc_threaded(memstore, size);
}

// See header file for documentation of public interface.
void *csprof_malloc_threaded(csprof_mem_t *memstore, size_t size){
  void *mem;

#if 1
  // Sanity check
  if(csprof_mem__get_status(memstore) != CSPROF_MEM_STATUS_INIT) { MSG(1,"NO MEM STATUS");return NULL; }
  if(!csprof_mem__is_enabled(memstore, CSPROF_MEM_STORE)) { MSG(1,"NO MEM ENBL");return NULL; }
#endif

  if(size <= 0) { MSG(1,"size <=0");return NULL; } // check to prevent an infinite loop!

  size = csprof_align_malloc_request(size);

  // Try to allocate the memory (should loop at most once)
  while ((mem = csprof_mem__alloc(memstore, size, CSPROF_MEM_STORE)) == NULL) {

    if((csprof_mem__grow(memstore, size, CSPROF_MEM_STORE) != CSPROF_OK)) {
      DIE("could not allocate swap space for memory manager", __FILE__, __LINE__);
      return NULL;
    }
  }

  return mem;
}

// See header file for documentation of public interface.
void *
csprof_tmalloc(size_t size)
{
    csprof_mem_t *memstore = csprof_get_memstore();

    return csprof_tmalloc_threaded(memstore, size);
}

void* 
csprof_tmalloc_threaded(csprof_mem_t *memstore, size_t size)
{
#ifndef CSPROF_PERF
    // Sanity check
    if(csprof_mem__get_status(memstore) != CSPROF_MEM_STATUS_INIT) { return NULL; }
    if(!csprof_mem__is_enabled(memstore, CSPROF_MEM_STORETMP)) { return NULL; }
#endif

    size = csprof_align_malloc_request(size);

    return csprof_mem__alloc(memstore, size, CSPROF_MEM_STORETMP);
}

// See header file for documentation of public interface.
void
csprof_tfree(void *ptr, size_t size)
{
    csprof_mem_t *memstore = csprof_get_memstore();

    csprof_tfree_threaded(memstore, ptr, size);
}

void
csprof_tfree_threaded(csprof_mem_t *memstore, void* ptr, size_t size)
{
#ifndef CSPROF_PERF
    // Sanity check
    if(csprof_mem__get_status(memstore) != CSPROF_MEM_STATUS_INIT) { return; }
    if(!csprof_mem__is_enabled(memstore, CSPROF_MEM_STORETMP)) { return; }
#endif
  
    if(csprof_mem__free(memstore, ptr, size, CSPROF_MEM_STORETMP) != CSPROF_OK) {
        DIE("programming error", __FILE__, __LINE__); 
    }
}

/* private implementation functions */

// csprof_mem__init: Initialize and prepare memory stores for use,
// using 'sz' and 'sz_tmp' as the initial sizes (in bytes) of the
// respective stores.  If either size is 0, the respective store is
// disabled; however it is an error for both stores to be
// disabled.  Returns CSPROF_OK upon success; CSPROF_ERR on error.
static int csprof_mem__init(csprof_mem_t *x, offset_t sz, offset_t sz_tmp){
  if(sz == 0 && sz_tmp == 0) { return CSPROF_ERR; }

  memset(x, 0, sizeof(*x));

  x->sz_next     = sz;
  x->sz_next_tmp = sz_tmp;
  
  // MSG(1,"csprof mem init about to call grow");
  if(sz != 0
     && csprof_mem__grow(x, sz, CSPROF_MEM_STORE) != CSPROF_OK) {
    MSG(1,"** MEM GROW for main store failed");
    return CSPROF_ERR;
  }
  if(sz_tmp != 0 
     && csprof_mem__grow(x, sz_tmp, CSPROF_MEM_STORETMP) != CSPROF_OK) {
    // MSG(1,"** MEM GROW for tmp store failed");
    return CSPROF_ERR;
  }
  
  // MSG(1,"mem init setting status to INIT");
  x->status = CSPROF_MEM_STATUS_INIT;
  return CSPROF_OK;
}

#ifdef NO
// csprof_mem__fini: Cleanup and deallocate memory stores.  Returns
// CSPROF_OK upon success; CSPROF_ERR on error.
static int csprof_mem__fini(csprof_mem_t *x){
    int i;

    csprof_mmap_info_t *mminf_vec[2];
    mminf_vec[0] = x->store;
    mminf_vec[1] = x->store_tmp;
  
    for (i = 0; i < 2; ++i) {
        csprof_mmap_info_t *mminf = mminf_vec[i];
        csprof_mmap_info_t *mminf_nxt = NULL;
        int mmfd;
    
        while (mminf) {
            // we must save any info needed from 'mminf' first, since it
            // will vanish out from under us when the mmap closes.
            mminf_nxt = mminf->next; 
            mmfd = mminf->fd;

            csprof_mmap_info__fini(mminf);
            munmap(mminf->beg, mminf->sz);
            close(mmfd);
      
            mminf = mminf_nxt;
        }
    }
  
    csprof_mmap_alloc_info__fini(&x->allocinf);
    csprof_mmap_alloc_info__fini(&x->allocinf_tmp);

    x->status = CSPROF_MEM_STATUS_FINI;
    return CSPROF_OK;
}

// csprof_mem__reset: Frees all memory stores, leaving only one store
// of at least 'sz' or 'sz_tmp' bytes in the regular and temporary
// memory stores, respectively.  If either size is 0, the respective
// store is disabled; however it is an error for both stores to be
// disabled.  Returns CSPROF_OK upon success; CSPROF_ERR on error.
static int
csprof_mem__reset(csprof_mem_t *x, offset_t sz, offset_t sz_tmp)
{
  int st_rs = 2, sttmp_rs = 2;      // reset level; 0, 1 (easy), 2 (hard)
  offset_t st_sz = 0, sttmp_sz = 0; // store sizes

  if(x->status != CSPROF_MEM_STATUS_INIT) { return CSPROF_ERR; }
  if(sz == 0 && sz_tmp == 0) { return CSPROF_ERR; }

  // Set reset levels and test for special cases. E.g., if we only have
  // one store in each list, and they are both of sufficient size
  // simply reset alloc pointers.
  if(x->store && sz != 0) {
    st_sz = x->store->sz - sizeof(csprof_mmap_info_t);
    st_rs = ( sz <= st_sz && !(x->store->next) ) ? 1 : 2;
  } else if(!x->store && sz == 0) {
    st_rs = 0;
  }
  
  if(x->store_tmp && sz_tmp != 0) {
    sttmp_sz = x->store_tmp->sz - sizeof(csprof_mmap_info_t);
    sttmp_rs = ( sz_tmp <= sttmp_sz && !(x->store_tmp->next) ) ? 1 : 2;
  } else if(!x->store_tmp && sz_tmp == 0) {
    sttmp_rs = 0;
  }

  if(st_rs <= 1 && sttmp_rs <= 1) {
    // Special case: reset the alloc pointers
    if(st_rs == 1) {
      x->allocinf.next = VPTR_ADD(x->allocinf.beg, sizeof(csprof_mmap_info_t));
    }
    if(sttmp_rs == 1) { 
      x->allocinf_tmp.next = VPTR_ADD(x->allocinf_tmp.beg, sizeof(csprof_mmap_info_t));
    }
  } else {
    // General case: reset the hard way
    if(csprof_mem__fini(x) != CSPROF_OK) { return CSPROF_ERR; }
    if(csprof_mem__init(x, sz, sz_tmp) != CSPROF_OK) { return CSPROF_ERR; }
  }

  return CSPROF_OK;
}
#endif

// csprof_mem__alloc: Attempts to allocate 'sz' bytes in the store
// specified by 'st'.  If there is sufficient space, returns a
// pointer to the beginning of a block of memory of exactly 'sz'
// bytes; otherwise returns NULL.
static void *
csprof_mem__alloc(csprof_mem_t *x, size_t sz, csprof_mem_store_t st)
{
  void* m, *new_mem;
  csprof_mmap_alloc_info_t *allocinf = NULL;

  // MSG(1,"csprof mem alloc: sz = %ld",sz);

  // Setup pointers
  switch (st) {
    case CSPROF_MEM_STORE:    allocinf = &(x->allocinf); break;
    case CSPROF_MEM_STORETMP: allocinf = &(x->allocinf_tmp); break;
    default: DIE("programming error", __FILE__, __LINE__);
  }

  // Sanity check
  if(sz <= 0) { return NULL; }
  if(!allocinf->mmap) { DIE("programming error", __FILE__, __LINE__); }

  // Check for space
  m = VPTR_ADD(allocinf->next, sz);
  if(m >= allocinf->end) { return NULL; }

  // Allocate the memory
  new_mem = allocinf->next;
  allocinf->next = m;        // bump the 'next' pointer
  
  return new_mem;
}

// csprof_mem__free: Attempts to free 'sz' bytes in the store
// specified by 'st'.  Returns CSPROF_OK upon success; CSPROF_ERR on
// error.
static int
csprof_mem__free(csprof_mem_t *x, void* ptr, size_t sz, csprof_mem_store_t st)
{
  void* m;
  csprof_mmap_alloc_info_t *allocinf = NULL;

  // Setup pointers
  switch (st) {
    case CSPROF_MEM_STORE:    allocinf = &(x->allocinf); break;
    case CSPROF_MEM_STORETMP: allocinf = &(x->allocinf_tmp); break;
    default: DIE("programming error", __FILE__, __LINE__);
  }

  // Sanity check
  if(!ptr) { return CSPROF_OK; }
  if(sz <= 0) { return CSPROF_OK; }
  if(!allocinf->mmap) { DIE("programming error", __FILE__, __LINE__); }

  // Check for space
  m = VPTR_ADD(allocinf->next, -sz);
  if(m < allocinf->beg) {
    return CSPROF_ERR;
  }
  
  // Unallocate the memory
  allocinf->next = m; 

  return CSPROF_OK;
}

// csprof_mem__grow: Creates a new pool of memory that is at least as
// large as 'sz' bytes for the mem store specified by 'st' and returns
// CSPROF_OK; otherwise returns CSPROF_ERR.
static int
csprof_mem__grow(csprof_mem_t *x, size_t sz, csprof_mem_store_t st)
{
  // Note: we cannot use our memory stores until we have allocated more mem
  void* mmbeg = NULL;
  int fd, mmflags, mmprot;
  size_t mmsz = 0, mmsz_base = 0;

  csprof_mmap_info_t** store = NULL;
  csprof_mmap_alloc_info_t *allocinf = NULL;
  csprof_mmap_info_t *mminfo = NULL;
  offset_t *sz_next = NULL;

  // Setup pointers
  if(st == CSPROF_MEM_STORE) {    
    mmsz_base = x->sz_next;
    store = &(x->store);
    allocinf = &(x->allocinf);
    sz_next = &(x->sz_next);
  } else if(st == CSPROF_MEM_STORETMP) {
    mmsz_base = x->sz_next_tmp;
    store = &(x->store_tmp);
    allocinf = &(x->allocinf_tmp);
    sz_next = &(x->sz_next_tmp);
  } else {
    DIE("programming error", __FILE__, __LINE__);
  }

  if(sz > mmsz_base) { mmsz_base = sz; } // max of 'mmsz_base' and 'sz'

  mmsz = mmsz_base + sizeof(csprof_mmap_info_t);

  MSG(CSPROF_MSG_MEMORY, "creating new %s memory store of %lu bytes", 
      csprof_mem_store__str(st), mmsz_base);

#ifdef NO_MMAP
  MSG(1,"mem grow about to call malloc");
  mmbeg = malloc(mmsz);
  if (!mmbeg){
    MSG(1,"mem grow malloc failed!");
    return CSPROF_ERR;
  }
#else
  // Open a new memory map for storage: Create a mmap using swap space
  // as the shared object.
  // FIXME: Linux had MAP_NORESERVE here; no equivalent on Alpha
  mmflags = MAP_PRIVATE; // MAP_VARIABLE
  mmprot = (PROT_READ | PROT_WRITE);
  if((fd = open("/dev/zero", O_RDWR)) == -1) {
    mmflags |= MAP_ANONYMOUS;
  } else {
    mmflags |= MAP_FILE;
  }
  if((mmbeg = mmap(NULL, mmsz, mmprot, mmflags, fd, 0)) == MAP_FAILED) {
    MSG(1,"mem grow mmap failed");
    return CSPROF_ERR;
  }
#endif  
  // Allocate some space at the beginning of the map to store
  // 'mminfo', add to mmap list and reset allocation information.
  mminfo = (csprof_mmap_info_t*)mmbeg;
  csprof_mmap_info__init(mminfo);
  
  if(allocinf->mmap == NULL) {
    if(*store != NULL) { DIE("programming error", __FILE__, __LINE__); }
    allocinf->mmap = mminfo;
    *store = mminfo;
  } else {
    allocinf->mmap->next = mminfo;
    allocinf->mmap = mminfo;
  }

  allocinf->next = VPTR_ADD(mmbeg, sizeof(*mminfo));
  allocinf->beg = mmbeg;
  allocinf->end = VPTR_ADD(mmbeg, mmsz);
  
  // Prepare for (possible) future growth
  *sz_next = (mmsz_base * 2);

  return CSPROF_OK;
}

// csprof_mem__is_enabled: Returns 1 if the memory store 'st' is
// enabled, 0 otherwise.
static int
csprof_mem__is_enabled(csprof_mem_t *x, csprof_mem_store_t st)
{
  switch (st) {
    case CSPROF_MEM_STORE:    return (x->store) ? 1 : 0;
    case CSPROF_MEM_STORETMP: return (x->store_tmp) ? 1 : 0;
    default:
      DIE("programming error", __FILE__, __LINE__);
      return -1;		/* shut gcc up */
  }
}

/* debugging aid */

static const char *
csprof_mem_store__str(csprof_mem_store_t st)
{
  switch (st) {
    case CSPROF_MEM_STORE:    return "general";
    case CSPROF_MEM_STORETMP: return "temp";
    default:
      DIE("programming error", __FILE__, __LINE__);
      return "";		/* shut gcc up */
  }
}

/* csprof_mmap_info_t and csprof_mmap_alloc_info_t */

int
csprof_mmap_info__init(csprof_mmap_info_t *x)
{
  memset(x, 0, sizeof(*x));
  return CSPROF_OK;
}

int
csprof_mmap_info__fini(csprof_mmap_info_t *x)
{
  return CSPROF_OK;
}

int
csprof_mmap_alloc_info__init(csprof_mmap_alloc_info_t *x)
{
  memset(x, 0, sizeof(*x));
  return CSPROF_OK;
}

int
csprof_mmap_alloc_info__fini(csprof_mmap_alloc_info_t *x) 
{
  return CSPROF_OK;
}
