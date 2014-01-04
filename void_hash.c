/* copyright 2000 Andrew Gray */

#include <pthread.h>
#include <stdlib.h>
#include "hash.h"
#include "void_hash.h"

static int void_equal(void *k1, void *k2);
static int void_hash(void *key);


static struct _hash_ops_t void_ops = {
	do_put,     /* yes, directly call do_put! yowza! */
	do_dupput,
	hash_get,
	hash_pop,
	hash_clear_entries, 
	hash_destroy,
	hash_traverse,
	void_hash,          /* deal with keys differently, too */
	void_equal
};

/* "void *" functions */
static int void_hash(void *key)
{
	/* could make this fancier with << 2 etc type stuff  */
	return (int)key;
}

static int void_equal(void *k1, void *k2)
{
	return (k1 == k2);
}

hash_t new_void_hash(hash_t self, int size)
{
	/* use super class' constructor: */
	self = new_hash(self, size);
	/* but override some methods: */
	self->ops = &void_ops;
	return self;
}
