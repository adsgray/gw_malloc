
/* copyright 2000 Andrew Gray */

#ifndef _RWLOCK_H_
#define _RWLOCK_H_

#include <pthread.h>

/* allow multiple readers, single writer. wlock will block
   until all readers have left, rlock will block until
   no writers are present.

   writer_waiting is set in wlock. It prevents the "writer starvation"
   that would happen if a writer was waiting for all readers
   to leave, but new readers kept coming in. Basically, as
   soon as a writer is waiting, no new readers are let in.
 */

typedef struct _rwlock_t *rwlock_t;

struct _rwlock_t {
	pthread_mutex_t m;
	pthread_cond_t  w;
	int writer_waiting;
	int rcount; /* number of readers inside */
	int rmax;    /* set to > 0 for reader upper-bound */

	/* rlock() blocks if writer present or rcount >= rmax */
	void (*rlock)(rwlock_t);  
	void (*runlock)(rwlock_t);  
	void (*wlock)(rwlock_t);
	void (*wunlock)(rwlock_t);  
};


rwlock_t new_rwlock(rwlock_t);

#endif
