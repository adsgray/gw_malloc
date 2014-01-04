#ifndef _VOID_HASH_H_
#define _VOID_HASH_H_

/* constructor: */
hash_t new_void_hash(hash_t self, int size);

/* so that derived classes can use them: */
void void_clear_entries(hash_t self);
int void_destroy(hash_t self);

#endif
