/* copyright 2000 Andrew Gray */

/*
   Most of the hash table code is generic in the extreme now.
   You only really need to write three functions:
      hash_func       
      key_equal       
   which are specific for your keys. see void_hash.c.

   You also of course have to write a destroy method
   for whatever you are keeping in hent_t (both key and cd).
 */
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "hash.h"

#define hash_t_size sizeof(struct _hash_t)
#define hent_t_size sizeof(struct _hent_t)

/*#define HDEBUG 1*/

static int string_hash(void *s);
static int string_equal(void *s1, void *s2);

/* these two functions do the acutal put and get. They
   require that the caller has the hash table lock. */
static void __put(hash_t self, hent_t e);
static hent_t __get(hash_t self, void *vkey, int remove_p);


static struct _hash_ops_t default_ops = {
	hash_put,
	hash_dupput,
	hash_get,
	hash_pop,
	hash_clear_entries,
	hash_destroy,
	hash_traverse,
	string_hash,
	string_equal
};


/* "string" functions */
static int string_hash(void *vs)
{
	char *s = vs;
	int rval = 0;
	while (*s) rval += (int)(*s++);
	return rval; 
}

static int string_equal(void *v1, void *v2)
{
	char *s1 = v1, *s2 = v2;
#ifndef HDEBUG
	return (strncmp(s1,s2,strlen(s1)) == 0);
#else
	int r;
	r = strncmp(s1,s2,strlen(s1));
	fprintf(stderr,"string_equal: s1=%s, s2=%s, r=%d\n",s1,s2,r);
	fflush(stderr);
	return (r == 0);
#endif
}

/* default is to put everything out on stderr */
void default_log(hash_t h, char *fmt, ...)
{
	char buf[1024];
	va_list ap;

	memset(buf, 0, 1024);
	if (h && h->name) {
		strcpy(buf, h->name);
		strcat(buf, ": ");
	}

	va_start(ap, fmt);
	vsprintf(buf + strlen(buf), fmt, ap);
	va_end(ap);

	strcat(buf, "\n");
	fprintf(stderr, buf);
	fflush(stderr);
}

hash_t new_hash(hash_t self, int size)
{
	if (!self) self = calloc(1, hash_t_size);
	self->size = size;
	self->entries = calloc(size, sizeof(hent_t)); /* ya, malloc ptrs */
	self->ops = &default_ops;
	self->log = default_log;
	pthread_mutex_init(&self->m, NULL);
	return self;
}


/* 
   This is a put for char * keys.
   It first strdups the key...
 */
hent_t hash_put(hash_t self, void *vkey)
{
	char *key = vkey;
	return do_put(self, strdup(key));
}

/* this function assumes that you hold the lock... */
static void __put(hash_t self, hent_t e)
{
	int h;
	h = self->ops->hash_func(e->key);
	h %= self->size;

	/* chain! */
	e->next = self->entries[h];
	if (!self->entries[h]) self->stats.used_slots++;
	self->entries[h] = e;
	self->stats.num++;
}

/* Do the put, and assume that the key will not
   be free()d out from under you.
   don't check for duplicates!!! 
 */
hent_t do_put(hash_t self, void *key)
{
	hent_t new_entry;

	new_entry = calloc(1, hent_t_size);
	new_entry->key = key;
#ifdef HENT_HAS_MUTEX
	pthread_mutex_init(&new_entry->m, NULL);
#endif

	pthread_mutex_lock(&self->m);
	__put(self, new_entry);
	pthread_mutex_unlock(&self->m);

	return new_entry;
}


int hash_dupput(hash_t self, void *vkey, hent_t *res)
{
	char *key;
	int rv;

	key = strdup((char *)vkey);

	rv = do_dupput(self, key, res);

	/* if the entry was already in the hash table,
	   there is no need for our strdup()d key. */
	if (rv) free(key);

	return rv;
}
/* do a put but check for duplicates attomically 

   If the entry already exists, return 1 and return
   the existing entry in res.
   If the entry does not already exist, put it
   in, return 0, and return the new entry in res.
 */
int do_dupput(hash_t self, void *key, hent_t *res)
{
	hent_t he;
	int rv;

	pthread_mutex_lock(&self->m);
	he = __get(self, key, 0);
	if (he) {
		/* entry existed before */
		rv = 1;
	} else {
		/* entry did not exist. put it. */
		he = calloc(1, hent_t_size);
		he->key = key;
#ifdef HENT_HAS_MUTEX
		pthread_mutex_init(&he->m, NULL);
#endif
		__put(self, he);
		rv = 0;
	}
	pthread_mutex_unlock(&self->m);

	*res = he;

	return rv;
}



hent_t hash_pop(hash_t self, void *key)
{
	return do_get(self, key, 1);
}

hent_t hash_get(hash_t self, void *key)
{
	return do_get(self, key, 0);
}

/* remove_p == 1 means remove the returned entry from the ht.
   remove_p == 0 means leave the returned entry in the ht.
   This function assumes that you have the lock...
 */
static hent_t __get(hash_t self, void *key, int remove_p)
{
	int h;
	int seeks = 1;
	hent_t prev = NULL;
	hent_t he;

	h = self->ops->hash_func(key);
	h %= self->size;

	he = self->entries[h];
	self->stats.gets++;

	while (he) {
		if (self->ops->key_equal(he->key, key)) {
			/* found it */
			if (remove_p) {
				/* 1: he is entries[h] */
				if (!prev) {
					self->entries[h] = he->next;
					if (!he->next)
						self->stats.used_slots--;
				} else {
				/* 2: he is further in */
					prev->next = he->next;	
				}
				self->stats.num--;
			}
			break;
		}
		prev = he;
		he = he->next;
		seeks++;
	}

	if (!he) {
		self->stats.fails++;
		self->stats.fail_seeks += seeks;
	} else {
		self->stats.found++;
		self->stats.found_seeks += seeks;
		/* paranoia */
		if (remove_p) he->next = NULL;
	}

	return he;
}

/* remove_p == 1 means remove the returned entry from the ht.
   remove_p == 0 means leave the returned entry in the ht.
 */
hent_t do_get(hash_t self, void *key, int remove_p)
{
	hent_t he;

	pthread_mutex_lock(&self->m);
	he = __get(self, key, remove_p);
	pthread_mutex_unlock(&self->m);

	return he;
}


/* the locking is a little messy... */
void hash_clear_entries(hash_t self)
{
	int i;
	hent_t he, cur;

	pthread_mutex_lock(&self->m);
	for (i = 0; i < self->size; i++) {
		/* kill entries[i] chain */
		he = self->entries[i];
		while (he) {
			cur = he;
			he = he->next;
			if (cur->destroy) cur->destroy(cur);
			free(cur);
		}
		self->entries[i] = NULL;
	}
	/* clear stats, too. Duh... */
	self->stats.num = self->stats.used_slots = 0;
	pthread_mutex_unlock(&self->m);
}

int hash_destroy(hash_t self)
{
	/* bye */

	self->ops->clear_entries(self);

	/* it is difficult to get the locking right because
	   we cannot unlock self->m after freeing self.
	   Is there a better way? */

	pthread_mutex_lock(&self->m);
	free(self->entries);
	pthread_mutex_unlock(&self->m);

	free(self);
	self = NULL;

	return 0;
}

/* print a summary of the contents of a hash table */
void hash_traverse(hash_t self)
{
	int i, count=0;
	int maxchain = 1;
	int chain;
	hent_t hent;
	char *cd;
	log_func_t log = self->log;

	if (!log) return;

	log(self, "** hash_traverse: %s",
		(self->name) ? self->name : "(anonymous)");

	pthread_mutex_lock(&self->m);
	/* print stats: */
	log(self, "sz=%d us=%d nu=%d ge=%d fa=%d fo=%d fas=%d fos=%d",
		self->size, self->stats.used_slots, 
		self->stats.num, self->stats.gets,
		self->stats.fails, self->stats.found,
		self->stats.fail_seeks, self->stats.found_seeks);

	/* print contents: */
	for (i = 0; i < self->size; i++) {
		chain = 0;
		hent = self->entries[i];
		while (hent) {
			chain++;
			count++;
			if (hent->display)
				cd = hent->display(hent);
			else
				cd = NULL;
			log(self, "%s", 
				(cd) ? cd:"## no display method...");
			/* the display method malloc()s for us... */
			if (cd) free(cd);
			hent = hent->next;
		}
		if (chain > maxchain) maxchain = chain;
	}
	if (count != self->stats.num) {
		log(self, "## hash_traverse: count[%d] != num[%d]!!",
		count, self->stats.num);
	}
	pthread_mutex_unlock(&self->m);

	log(self, "** %d entries. longest chain=%d", count, maxchain);
}
