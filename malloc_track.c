/* copyright 2000 Andrew Gray */

/* 
   Whew. Now it is possible to destroy/recreate the hash tables
   at run time, I think. 2000.10.29.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/* use originals */
#define NO_MM_REDEF 1
#include "malloc_track.h"
#include "hash.h"

/*#define MDEBUG 1*/

/* APPLICATION SPECIFIC: */
#include "testp.h"

int
spawn_detached_thread(thread_func_t func, void *arg)
{
	pthread_attr_t attr;
	pthread_t throw_away;

	/* this is a throwaway thread, so we won't bother
   	reaping its exit code when it pthread_exit()s */

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	return pthread_create(&throw_away, /* thread id */
			&attr,
			func,
			arg);
}

#ifdef GW_MM_ON

#ifndef MM_ENABLED
#define MM_ENABLED 1
#endif
#ifndef MM_RETURN
#define MM_RETURN
#endif

/* these two functions do all of the interesting work. 
   See the comments below for more info.
   Note that they assume that resource keys are unique. That is,
   no resource is ever given out twice.

   I see you still do not understand. They assume this:
   malloc(n) != malloc(n);
   open( blah blah ) != open( blah blah );

   More explanation:
   Two consecutive calls to malloc() or open() will never ever
   ever ever return the same pointer/fd.
 */
void res_track(hash_t ht_res, void *res_key,
               hash_t ht_caller, char *caller_key,
	       char *(*display_func)(hent_t));

void res_untrack(hash_t ht_res, void *res_key,
                hash_t ht_caller, char *caller_key);


/* accounting stuff:

        count is self explanatory.

	caller is used to record which file/line allocated
	a particular resource so that the corresponding
	count (in the other hash table) can be decremented
	when it is released.

	That might not make much sense. 
 */
struct acc_stuff {
	int count;
	char *caller;
};

/* The following three functions are used by hash->traverse.
   They are hent_t->display methods.
 */

/* format pointer info */
static char *display_ptr_cd(hent_t h)
{
	char *rv;
	struct acc_stuff *acc;

	/*if (!h->cd) return NULL;*/
	rv = calloc(1, 256);
	acc = h->cd;
	if (acc->caller) sprintf(rv, "%p -- count=%d caller=%s", 
				h->key, acc->count,
				acc->caller);
	else sprintf(rv, "%p -- count=%d", h->key, acc->count);
	return rv;
}

/* format file descriptor info */
static char *display_fd_cd(hent_t h)
{
	char *rv;
	struct acc_stuff *acc;

	/*if (!h->cd) return NULL;*/
	rv = calloc(1, 256);
	acc = h->cd;
	if (acc->caller) sprintf(rv, "%d -- count=%d caller=%s", 
				(int)h->key, acc->count,
				acc->caller);
	else sprintf(rv, "%d -- count=%d", (int)h->key, acc->count);
	return rv;
}

/* format char* info */
static char *display_char_cd(hent_t h)
{
	char *rv;
	struct acc_stuff *acc;

	/*if (!h->cd) return NULL;*/
	rv = calloc(1, 256);
	acc = h->cd;
	if (acc->caller) sprintf(rv, "%s -- count=%d caller=%s", 
				h->key, acc->count,
				acc->caller);
	else sprintf(rv, "%s -- count=%d", h->key, acc->count);
	return rv;
}

/* The following two functions are used by hash->clear_entries.
   They are hent_t->destroy methods.
 */

/* destroy accounting info. Use this for void_hash hent_t's */
static void destroy_fd_cd(hent_t h)
{
	struct acc_stuff *acc;

	acc = h->cd;
	if (!acc) return;
	if (acc->caller) free(acc->caller);
	free(acc);
}

/* for destroying char* keyed hent_t's */
static void destroy_char_cd(hent_t h)
{
	struct acc_stuff *acc;

	acc = h->cd;
	if (h->key) free(h->key);
	if (!acc) return;
	if (acc->caller) free(acc->caller);
	free(acc);
}

/* used to atomically modify acc->count's */
static void mod_count(hent_t he, int bywhat)
{
	struct acc_stuff *acc;

	acc = he->cd;
	if (!acc) return;

	pthread_mutex_lock(&he->m);
	acc->count += bywhat;
	pthread_mutex_unlock(&he->m);
}


/* This is the most complicated function in this file, next to
   res_untrack(). Note however that it is about 50 lines long...
   - display func is for filedes' or pointers... see mm_malloc, mm_open.

   Purpose: 
     Track a resource denoted by res_key for later retreival by
     res_untrack.

   Method:
     stick {res_key => (acc,caller_key)} into ht_res, to track the resource
     denoted by res_key.
     stick {caller_key => (acc, NULL)} into ht_caller, if it is not
     already there. Up its acc->count to denote that that particular
     line of code has allocated another resource.

 */
void res_track(hash_t ht_res, void *res_key,
               hash_t ht_caller, char *caller_key,
	       char *(*display_func)(hent_t))
{
	hent_t hent;
	log_func_t log;
	struct acc_stuff *acc;

	log = ht_caller->log;
#ifdef MDEBUG
	log(NULL, "res_track: caller is: %s", caller_key);
#endif

	if (!ht_res->ops->dupput(ht_res, res_key, &hent)) {
		hent->display = display_func;
		hent->destroy = destroy_fd_cd;
		acc = calloc(1, sizeof(struct acc_stuff));
		acc->caller = strdup(caller_key);
		hent->cd = acc;
	} else {
		acc = hent->cd;
		log(ht_caller,
			"%d previously gotten by [%s] now by [%s]",
			res_key, acc->caller, caller_key);
		/*hent->destroy(hent); */
	}

	/* up the count for this res. should never be > 1!!! */
	mod_count(hent,1);

	/* record which line of code got the resource.
	   Will be used by res_untrack.

	   Argh. There is still a race condition here.
	   I will buy you a beer if you see it. No hints.[1]
	 */
	if (!ht_caller->ops->dupput(ht_caller, caller_key, &hent)) {
		/* the entry didn't exist before... */
		hent->display = display_char_cd;
		hent->destroy = destroy_char_cd;
		hent->cd = calloc(1, sizeof(struct acc_stuff));
#ifdef MDEBUG
		log(NULL, "res_track: %s not found", caller_key);
#endif
	}
#ifdef MDEBUG
	else {
		log(NULL, "res_track: %s found", caller_key);
	}
#endif
	/* up the cound for this file/line */
	mod_count(hent,1);
}

/* This is the most complicated function in this file, next to
   res_track(). It is about 50 lines long.

   Purpose:
     This function is called when the resource associated with
     res_key is being released. So we remove that resource
     from the resource tracking table.

   Method:
     Get (acc, caller) = ht_res(res_key). That is, see if the
     resource key being passed into us was tracked with res_track.
     If yes:
         get (acc, NULL) = ht_caller(caller) and decrement
         its acc->count.
     If no:
         log a good-natured complaint.
 */
void res_untrack(hash_t ht_res, void *res_key,
                hash_t ht_caller, char *caller_key)
{
	hent_t hent;
	struct acc_stuff *acc;
	log_func_t log;
	char *c;

	log = ht_caller->log;

	hent = ht_res->ops->pop(ht_res, res_key);

	if (hent) {
#ifdef MDEBUG
		log(NULL, "res_untrack: %d found", res_key);
#endif
		mod_count(hent, -1);
		acc = hent->cd;
		if (acc->count != 0) {
			log(ht_caller,"[%s]: count is %d!", 
			caller_key, acc->count);
		}
		if (!acc->caller) {
			log(ht_caller,
			"[%s]: don't know who got %d",
			caller_key, res_key);
			goto out;
		}

		c = acc->caller;
		acc->caller = NULL;

		/* we popped this hent. destroy it. arrrr matey... */
		hent->destroy(hent);
		free(hent);

		/* find the entry for file/line */
		hent = ht_caller->ops->get(ht_caller, c);
		if (!hent) {
			log(ht_caller,"[%s]: no entry for %s",
			caller_key, c);
			free(c);
			goto out;
		}
		mod_count(hent, -1);
		free(c);
	} else {
		log(ht_caller,"[%s]: %d was not tracked", 
		caller_key, res_key);
	}

	out:
	/* maybe log something here */
}

/* the hash tables */
hash_t ht_malloc;       /* track callers of malloc */
hash_t ht_malloc_ptr;   /* track pointers returned by malloc */
hash_t ht_open;         /* track callers of open */
hash_t ht_open_fd;      /* track fds returned by open */


/* the creation of res_{track/untrack} greatly simplified
   all of these other functions...
 */
void *mm_malloc(size_t n, char *f, int l)
{
	char temp[100];
	log_func_t log;
	void *rv;

	rv = calloc(1,n);

	if (!rv) goto out;
	if (!MM_ENABLED) goto out;

	log = ht_malloc->log;

	memset(temp, 0, 100);
	sprintf(temp, "%s:%d", f, l);
	res_track(ht_malloc_ptr, rv, ht_malloc, temp, display_ptr_cd);

	MM_RETURN;

	out:
	if (!rv) log(ht_malloc, "malloc got NULL");
	return rv;
}

int mm_open(char *path, int oflag, mode_t mode, char *f, int l)
{
	char temp[100];
	log_func_t log;
	int rv;

	rv = open(path, oflag, mode);

	if (rv < 0) goto out;
	if (!MM_ENABLED) goto out;

	log = ht_open->log;

	memset(temp, 0, 100);
	sprintf(temp, "%s:%d", f, l);
	/* is that cast "nice"? */
	res_track(ht_open_fd, (void *)rv, ht_open, temp, display_fd_cd);

	MM_RETURN;

	out:
	if (rv < 0) log(ht_open, "open returned %d", rv);
	return rv;
}


void *mm_realloc(void *orig, size_t s, char *f, int l)
{
	/* 
	check to see if orig was malloced by mm_malloc().
	if so, do a realloc and return the new pointer.
	if orig is null, return mm_malloc(s, f, l);
	*/

	char temp[100];

	if (!orig) return mm_malloc(s, f, l);
	if (!MM_ENABLED) return realloc(orig, s);

	/* detrack orig pointer */
	memset(temp, 0, 100);
	sprintf(temp, "%s:%d", f, l);
	res_untrack(ht_malloc_ptr, orig, ht_malloc, temp);

	/* track new pointer */
	orig = realloc(orig, s);
	if (orig) 
		res_track(ht_malloc_ptr, orig, ht_malloc, temp, display_ptr_cd);

	MM_RETURN;
	return orig;
}

void mm_free(void *p, char *f, int l)
{
	char temp[100];
	log_func_t log;

	if (!MM_ENABLED) goto out;
	log = ht_malloc->log;

	if (!p) {
		log(ht_malloc,
			"free called with NULL ptr. [%s:%d]",
			f,l);
		goto retout;
	}

	memset(temp, 0, 100);
	sprintf(temp, "%s:%d", f, l);
	res_untrack(ht_malloc_ptr, p, ht_malloc, temp);

	retout:
	MM_RETURN;
	out:
	free(p);
}

int mm_close(int fd, char *f, int l)
{
	char temp[100];
	log_func_t log;

	if (!MM_ENABLED) goto out;
	log = ht_open->log;

	memset(temp, 0, 100);
	sprintf(temp, "%s:%d", f, l);
	/* is that cast "nice"? */
	res_untrack(ht_open_fd, (void *)fd, ht_open, temp);

	MM_RETURN;
	out:
	return close(fd);
}

char *mm_strdup(char *s, char *f, int l)
{
	int ln;
	char *rval;

	if (!s) return NULL;
	if (!MM_ENABLED) return strdup(s);
	MM_RETURN; /* tricky. Must do this before calling mm_malloc */

	ln = strlen(s);
	rval = mm_malloc(ln + 1, f, l);
	if (rval) strncpy(rval, s, ln);
	return rval;
}

/*
  [1] Okay, I don't think the race is likely. Maybe it is even
  impossible. Here it is:
  Th1: res_track, dupput(), but doesn't get a chance to initialize acc.
  Th2: res_untrack. Does a get, and then a mod_count.

  Can this even happen? The malloc at least has to complete
  before the free happens.

  Disable/Enable may cause it to happen.  Later: Nope.

  But it can happen if threads which are calling ht->traverse
  only have a read lock. Solution: force them to get a write lock.
  Kind of ugly, but it works. See how hard testp.c pounds this
  stuff.
 */

#endif
