/* copyright 2000 Andrew Gray */

/*
    Added dupput. It is a tad on the bizarre side. Look at hash.c
    for details. 2000.10.29

 */

#ifndef _HASH_H_
#define _HASH_H_

#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>

/*#define HENT_HAS_MUTEX 1*/

/* hash table entry and hash table. 
   currently very specific for mm_malloc.
   should be more general.
*/
typedef struct _hent_t *hent_t;
struct _hent_t {
	void *next;
	char *key;
#ifdef HENT_HAS_MUTEX
	pthread_mutex_t m;
#endif
	void *cd; /* clientdata */
	char *(*display)(hent_t); /* returns a string describing
	                                contents of hent_t */
	void (*destroy)(hent_t);     /* destructor for hent_t */
};

/* may as well do some pure CS research while we're at it... */
typedef struct _hash_stats {
	int num;	 /* number of entries */
	int gets;        /* get calls */
	int fails;       /* failed get calls */
	int found;       /* successful get calls */
	int fail_seeks;  /* chain follows for fails */
	int found_seeks; /* chain follows for finds */
	int used_slots;  /* used slots in "entries" */
} hash_stats_t;

/* hash table.
   improvements:  - have a separate lock for each
   row of the table, instead of for the whole table.
*/
typedef struct _hash_t *hash_t;
typedef struct _hash_ops_t *hash_ops_t;
struct _hash_ops_t {
	hent_t (*put)(hash_t, void *);

	/* an atomic put if key not present */
        int    (*dupput)(hash_t, void *key, hent_t *res);

	hent_t (*get)(hash_t, void *);

	/* same as get, but removes from table */
	hent_t (*pop)(hash_t, void *); 

	/* destroys all hent_t's in table */
	void (*clear_entries)(hash_t); 

	/* calls clear_entries, then free()s everything else */
	int (*destroy)(hash_t);

	/* should be called "display" */
	void (*traverse)(hash_t);

	/* specific for your keys */
	int (*hash_func)(void *);
	int (*key_equal)(void *, void *);
};

typedef void (*log_func_t)(hash_t, char *fmt, ...);
struct _hash_t {
	int size;
	char *name;
	hent_t *entries;
	hash_stats_t stats;
	hash_ops_t ops;
	log_func_t log;
	pthread_mutex_t m;
};

/* size should be prime. */
hash_t new_hash(hash_t, int);

/* these are exported so that derived classes can use them if
   they wish. 

   The do_functions are non-key-type specific.
   The hash_functions assume your keys are strings
   and they call strdup before calling the do_functions.

   The above does not apply to hash_clear_entries, hash_destroy,
   or hash_traverse.
 */
void default_log(hash_t h, char *fmt, ...);
hent_t hash_put(hash_t self, void *key);
int hash_dupput(hash_t self, void *key, hent_t *res);
hent_t hash_get(hash_t self, void *key);
hent_t hash_pop(hash_t self, void *key);
int do_dupput(hash_t self, void *key, hent_t *res);
hent_t do_get(hash_t self, void *key, int remove_p);
hent_t do_put(hash_t self, void *key);
void hash_clear_entries(hash_t self);
void hash_restore_entries(hash_t self);
int hash_destroy(hash_t self);
void hash_traverse(hash_t self);

#endif
