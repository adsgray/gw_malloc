
/* copyright 2000 Andrew Gray */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "rwlock.h"

/* assert: rcount >= 0 
   Translation: the number of readers is always at least 0.
 */

#define rw_size sizeof(struct _rwlock_t)
/*#define RWDEBUG 1*/

static void do_rlock(rwlock_t self)
{
	pthread_mutex_lock(&self->m);

	while (self->writer_waiting)
	{
			/* if there is still a writer waiting,
			   we should signal him... -ag*/
			pthread_cond_signal(&self->w);
			pthread_cond_wait(&self->w, &self->m);
	}

	if (self->rmax > 0)
		while (self->rcount >= self->rmax)
			pthread_cond_wait(&self->w, &self->m);

	self->rcount++;
	pthread_mutex_unlock(&self->m);
}

static void do_runlock(rwlock_t self)
{
	pthread_mutex_lock(&self->m);
	self->rcount--;
	/* signal any waiting readers/writers */
	pthread_cond_signal(&self->w);
	pthread_mutex_unlock(&self->m);
}

static void do_wlock(rwlock_t self)
{
	pthread_mutex_lock(&self->m);

	self->writer_waiting = 1;
	while (self->rcount > 0) {
		pthread_cond_wait(&self->w, &self->m);
	}
	/* note that now we hold the lock... any readers or
	   writers coming in now will block */
	self->writer_waiting = 0;
	   
}

static void do_wunlock(rwlock_t self)
{
	/* signal any waiting readers/writers */
	pthread_cond_signal(&self->w);
	pthread_mutex_unlock(&self->m);
}

rwlock_t new_rwlock(rwlock_t rw)
{
	if (!rw) rw = calloc(1, rw_size);
	if (!rw) return NULL;

	pthread_cond_init(&rw->w, NULL);
	pthread_mutex_init(&rw->m, NULL);
	rw->rlock = do_rlock;
	rw->runlock = do_runlock;
	rw->wlock = do_wlock;
	rw->wunlock = do_wunlock;

	return rw;
}
