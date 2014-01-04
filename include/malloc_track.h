/* copyright 2000 Andrew Gray */

/*
	* Track calls:
	  (malloc,calloc,realloc,strdup/free)
	  (open/close)
	* Todo: track (fopen/fclose) calls.

	* How to use:
	  0. make sure #define GW_MM_ON is uncommented below.
	  1. #include "gw_malloc.h" in your .c file.
	  2. make sure you link against util/gw_malloc.o and util/hash.o
	     Note that in the GW these are part of util.a.

	* Currently gl3200.h includes gw_malloc.h. So most GW code
	  is already tracked.

	* You have to make sure that you call open(2) with 3 arguments.

	* For each process that uses it, a file "/tmp/mm_malloc-<pid>.log" is
	  created and logged to. The file contains stuff like this:

<sample>
** gw_malloc operational
mm_free called with NULL ptr. [gwconf.c:1606]
mm_free called with NULL ptr. [gwconf.c:1608]
mm_free called with NULL ptr. [gwconf.c:1612]
---
mm_log_thread. 0:
---
** hash.c: default_traverse
ms_model.c:486 () 9317
submit.c:142 () 880
smtp.c:715 () 0
smtp.c:780 () 0
smtp.c:924 () 0
lookup.c:132 () 969
aix/gwconfy.c:1043 () 1
gwtxt.c:181 () 73
aix/gwconfy.c:1019 () 1
gwtxt.c:168 () 1
aix/gwconfy.c:1095 () 1
gwconf.c:1612 () 1
mailin.c:1659 () 1
wmio.c:2080 () 0
util.c:814 () 1
util.c:805 () 1
** 36 entries
** hash.c: default_traverse
32 () 0
14 (ms_day.c:119) 1
33 () 0
34 () 0
17 (ms_day.c:119) 1
mailout.c:233 () 1
mailin.c:1951 () 0
7 (ms_db.c:508) 1
8 (ms_day.c:119) 1
ms_db.c:508 () 1
ms_day.c:119 () 5
20 (ms_day.c:119) 1
11 (ms_day.c:119) 1
30 () 0
31 (mailout.c:233) 1
** 15 entries
** hash.c: default_traverse
** 0 entries
</sample>

"mm_free called with NULL ptr. [gwconf.c:1606]" is telling you that on
line 1606 of gwconf.c free() was called with a NULL pointer.  Double
free()s, free()ing something that was not malloc()ed, and other things
are also logged.

The mm_log_thread summarizes the state of the tracker's internal hash
tables once per minute.  

The first hash table is for malloc()s:
"ms_model.c:486 () 9317" is telling you that the malloc() on line 486
of ms_model.c has been called 9317 times without being free()d. Don't
worry though, because it is probably sitting in the in-memory message
store. If you see counts above 10000 that never ever go down it is
probably a leak.
"smtp.c:715 () 0" says that the malloc on line 715 of smtp.c has no
outstanding free()s.

The second hash table is for open(2) and close(2) calls:
There are two types of file-descriptor lines:  
"14 (ms_day.c:119) 1" is telling you that the open(2) call on line 119
of ms_day.c got file descriptor 14 and that it has not been closed yet
(the reference count is 1).  Note:  "30 () 0" is the same type of line.
It is saying that at one point your process was using file descriptor
30 but that it closed it.

"ms_day.c:119 () 5" is telling you that your process currently has 5
open file descriptors resulting from the open(2) call on line 119 of
ms_day.c. A file descriptor leak will show up as a line of this type
with a continually increasing count. You will also see lots of file
descriptors associated with that line of code each with reference count
1 (the first type of line).

The (open/close) tracker also complains in the log file if it thinks
that a file descriptor is being used twice (should never happen).

The third hash table is empty because it is for fopen/fclose which
hasn't been added to the tracking code yet.

Also: Any time you want something done "quickly" and in paralell put it
in a function with a signature like: void *my_func(void *arg) and pass
it to spawn_detached_thread()[1]

[1] Irony.

*/

#ifndef _MM_MALLOC_H_
#define _MM_MALLOC_H_

typedef void *(*thread_func_t)(void *);
int spawn_detached_thread(thread_func_t, void *);

#define GW_MM_ON 1

#ifdef GW_MM_ON
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "hash.h"

#ifndef NO_MM_REDEF
#ifdef malloc
#undef malloc
#endif
#define malloc(s) mm_malloc(s, __FILE__, __LINE__)

#ifdef calloc
#undef calloc
#endif
#define calloc(n,s)  mm_malloc(n * s,__FILE__,__LINE__)

#ifdef realloc
#undef realloc
#endif
#define realloc(p,s) mm_realloc(p,s,__FILE__,__LINE__)

#ifdef free
#undef free
#endif
#define free(p) mm_free(p, __FILE__,__LINE__)

#ifdef open
#undef open
#endif
#define open(p,o,m) mm_open(p,o,m,__FILE__,__LINE__)

#ifdef close
#undef close
#endif
#define close(f) mm_close(f, __FILE__, __LINE__)

#ifdef strdup
#undef strdup
#endif
#define strdup(s) mm_strdup(s, __FILE__, __LINE__)
#endif

void *mm_malloc(size_t, char *, int);
void *mm_realloc(void *, size_t, char *, int);
void mm_free(void *, char *, int);
int mm_open(char *path, int oflag, mode_t mode, char *f, int l);
int mm_close(int fd, char *f, int l);
char *mm_strdup(char *, char *, int);

/* the user must initialize these: */
extern hash_t ht_malloc; 
extern hash_t ht_malloc_ptr; 
extern hash_t ht_open; 
extern hash_t ht_open_fd; 



#endif




#endif

