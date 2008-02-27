#include <stdlib.h>

#include "mem.h"
#include "state.h"
#include "handling_sample.h"

#include "thread_data.h"

static thread_data_t _local_td;

static thread_data_t *
local_td(void)
{
  return &_local_td;
}

thread_data_t *(*csprof_get_thread_data)(void) = &local_td;

void
csprof_unthreaded_data(void)
{
  csprof_get_thread_data = &local_td;
}


void
csprof_thread_data_init(int id, offset_t sz, offset_t sz_tmp)
{
  TD_GET(id)       = id;
  TD_GET(memstore) = csprof_malloc_init(sz,sz_tmp);
  TD_GET(state)    = NULL;

  csprof_state_t *state = csprof_malloc(sizeof(csprof_state_t));

  csprof_set_state(state);
  csprof_state_init(state);
  csprof_state_alloc(state);
  csprof_init_handling_sample(csprof_get_thread_data(),0);
}

#ifdef CSPROF_THREADS

#include <pthread.h>

static pthread_key_t _csprof_key;

void
csprof_init_pthread_key(void)
{
  int ret = pthread_key_create(&_csprof_key, NULL);
}


void
csprof_set_thread_data(thread_data_t *td)
{
  pthread_setspecific(_csprof_key,(void *) td);
}


void
csprof_set_thread0_data(void)
{
  csprof_set_thread_data(&_local_td);
}

// FIXME: use csprof_malloc ??
thread_data_t *
csprof_allocate_thread_data(void)
{
  return malloc(sizeof(thread_data_t));
}

static thread_data_t *
thread_specific_td(void)
{
  return (thread_data_t *) pthread_getspecific(_csprof_key);
}

void
csprof_threaded_data(void)
{
  assert(csprof_get_thread_data == &local_td);
  csprof_get_thread_data = &thread_specific_td;
}
#endif // defined(CSPROF_THREADS)
