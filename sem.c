/* copyright 2000 Andrew Gray */

#include <pthread.h>
#include <stdlib.h>
#include "sem.h"

static int do_p(sem_t);
static int do_v(sem_t);
static int do_set(sem_t, int);
static int do_get(sem_t);

#define sem_size sizeof(struct _sem_t)
sem_t new_sem(sem_t s, int c)
{
	if (!s) s = calloc(1, sem_size);
	pthread_mutex_init(&s->m, NULL);
	pthread_cond_init(&s->w, NULL);
	s->p = do_p;
	s->v = do_v;
	s->set = do_set;
	s->get = do_get;
	s->c = c;
	
	return s;
}

/* wait for count to be non-zero, then decrement the count */
static int do_p(sem_t self)
{
	int rv;

	/* this is a hack for a special case... another 
           caller
	   can set the value to negative to mean
	   "permanently unlocked" */
	if (self->c < 0) return self->c;

	pthread_mutex_lock(&self->m);
	while (self->c == SEM_LOCKED) pthread_cond_wait(&self->w, &self->m);

	/* if it is set to "permanently unlocked" don't bother */
	if (self->c > 0) self->c--;
	rv = self->c;
	pthread_cond_signal(&self->w);
	pthread_mutex_unlock(&self->m);
	return rv;
}

/* increment the count and signal waiters */
static int do_v(sem_t self)
{
	int rv;

	pthread_mutex_lock(&self->m);
	self->c++;
	rv = self->c;
	pthread_cond_signal(&self->w);
	pthread_mutex_unlock(&self->m);
	return rv;
}

static int do_set(sem_t self, int val)
{
	int rv;

	pthread_mutex_lock(&self->m);
	rv = self->c;
	self->c = val;
	pthread_cond_signal(&self->w);
	pthread_mutex_unlock(&self->m);
	return rv;
}

static int do_get(sem_t self)
{
	int rv;

	pthread_mutex_lock(&self->m);
	rv = self->c;
	pthread_mutex_unlock(&self->m);
	return rv;
}
