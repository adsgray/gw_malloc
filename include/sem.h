/* copyright 2000 Andrew Gray */

#ifndef _SEM_H_
#define _SEM_H_

#include <pthread.h>
typedef struct _sem_t *sem_t;

struct _sem_t {
	pthread_mutex_t m;
	pthread_cond_t  w;
	int c;
	int (*p)(sem_t);          /* "down" == lock/wait */
	int (*v)(sem_t);          /* "up"   == unlock    */
	int (*set)(sem_t, int);   /* set the count to whatever */
	int (*get)(sem_t);        /* get the current value (w/locking) */
};

#define SEM_LOCKED 0
#define SEM_UNLOCKED 1
#define SEM_PERM_UNLOCKED -1

sem_t new_sem(sem_t, int);

/* eg: 

   main thread does this:
   sem_t r1_sem = new_sem(NULL, SEM_LOCKED); 
   sem_t r2_sem = new_sem(NULL, SEM_LOCKED); 

   then start threads which set up resources:
   pthread_create(r1_thread);
   pthread_create(r2_thread);
   pthread_create(thread_n);


   r1_thread_init() {
   	... set up resource 1 ...

	(void) r1_sem->set(r1_sem, SEM_PERM_UNLOCKED);

	and now wait a resource that another thread sets up:
	(void) r2_sem->p(r2_sem);

	now we are guaranteed that "resource 2" is available.
	... do other stuff ...
   }

   r2_thread_init() {
   	... set up r2 ...

	(void) r2_sem->set(r2_sem, SEM_PERM_UNLOCKED);

	wait for r1...
	(void) r1_sem->p(r1_sem);

   }

   Ya, I'm thinking about the call limiting thing...

   The above example is more complex than most: Two threads
   who make a resource available and who require each other's
   resources...
   Reality would be more like this:

   thread_n_init() {
   	(void) r1_sem->p(r1_sem);
   	(void) r2_sem->p(r2_sem);
	... okay, now go ...
   }

   That is, a thread needs to wait until r1 and r2 are initialized
   before continuing.

*/


#endif
