#include <stdlib.h>
#include <signal.h>
#include "malloc_track.h"
#include "hash.h"
#include "void_hash.h"
#include "rwlock.h"
#include "testp.h"

/*
	Well holy geez, most of this can be dropped
	straight into the GL3200...
 */


static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static void mm_init();
static void mm_enable(int p);
static void my_log(hash_t h, char *fmt, ...);
static rwlock_t ed_lock; /* enabled/disabled lock. pre: mm_init() */
static int enabled;


/* Test Application: */
static void signal_handler(int signo);
static void *nasty_thread(void *arg);

/* would be cool to pass in a ptr to a list of hash tables
   in arg. Then this function just walks along the list
   "traverse"-ing each of them. */
void mm_log(log_func_t log)
{
	/* We actually need a "write" lock to do
	   logging safely. Because somebody could
	   have done a put but not filled in their
	   acc, display/destroy methods fully... */

	ed_lock->wlock(ed_lock);
	if (!enabled) goto out;

	ht_malloc->ops->traverse(ht_malloc);
	if (ht_malloc_ptr->stats.num < 50)
		ht_malloc_ptr->ops->traverse(ht_malloc_ptr);
	else
		log(ht_malloc, "%d malloc ptrs",
			ht_malloc_ptr->stats.num);
	ht_open->ops->traverse(ht_open);
	if (ht_open_fd->stats.num < 50)
		ht_open_fd->ops->traverse(ht_open_fd);
	else
		log(ht_malloc, "%d open fds",
			ht_open_fd->stats.num);

	out:
	ed_lock->wunlock(ed_lock);
}


/* replace with gw_ht_log... */
static void my_log(hash_t h, char *fmt, ...)
{
	char buf[1024];
	va_list ap;

	memset(buf, 0, 1024);
	if (h && h->name) {
		strcpy(buf, h->name);
		strcat(buf, " m_l: ");
	}

	va_start(ap, fmt);
	vsprintf(buf + strlen(buf), fmt, ap);
	va_end(ap);

	strcat(buf, "\n");

	fprintf(stderr, buf);
	fflush(stderr);
}

int mm_enter_hook()
{
	if (!enabled) return 0;
	ed_lock->rlock(ed_lock);
	if (!enabled) {
		ed_lock->runlock(ed_lock);
		return 0;
	}
	return 1;
}

void mm_exit_hook()
{
	ed_lock->runlock(ed_lock);
}


static void mm_init()
{
	ed_lock = new_rwlock(NULL);

	ht_malloc = new_hash(NULL, 13);
	ht_malloc->name = "malloc(3)";
	ht_malloc->log = my_log;

	ht_malloc_ptr = new_void_hash(NULL, 1027);
	ht_malloc_ptr->name = "malloc pointers";
	ht_malloc_ptr->log = my_log;

	ht_open = new_hash(NULL, 101);
	ht_open->name = "open(2)";
	ht_open->log = my_log;

	ht_open_fd = new_void_hash(NULL, 101);
	ht_open_fd->name = "open fds";
	ht_open_fd->log = my_log;
}

static void mm_enable(int p) 
{
	pthread_once(&init_once, mm_init);

	my_log(NULL, "mm_enable");
	/* same state */
	if (p == enabled) return;

	/* get exclusive access: */
	ed_lock->wlock(ed_lock);

	if (p) {
		/* enabling */
		my_log(NULL, "enabling tracking...");
	} else {
		/* disabling */
		my_log(NULL, "disabling tracking...");
		if (ht_malloc)
			ht_malloc->ops->clear_entries(ht_malloc);

		if (ht_malloc)
			ht_malloc_ptr->ops->clear_entries(ht_malloc_ptr);
			
		if (ht_open)
			ht_open->ops->clear_entries(ht_open);

		if (ht_open_fd)
			ht_open->ops->clear_entries(ht_open_fd);
	}

	enabled = p;
	ed_lock->wunlock(ed_lock);

	my_log(NULL, "mm_enable: %d", enabled);
}

void mm_toggle()
{
	mm_enable(!enabled);
}



/************************************
    Test Application:
    USR1 --> toggle tracking on/off
    USR2 --> display hash tables
 ************************************/

static void signal_handler(int signo)
{
	switch(signo) {
		case SIGUSR1:
			fprintf(stderr, "USR1\n");
			mm_toggle();
			break;
		case SIGUSR2:
			fprintf(stderr, "USR2\n");
			mm_log(my_log);
			break;
		case SIGINT:
			fprintf(stderr, "INT\n");
			fflush(stderr);
			abort();
		default:
			break;
	}
}


/* this is a nasty thread that leaks memory.
 */
static void *nasty_thread(void *arg)
{
	char *foo;
	int fd;
	int i;

	/* on Linux, when you run out of fd's, you SEGV! barf. */
	for (i = 0; 1 ; i++) {
		foo = malloc(5);

		fd = open("/tmp/foo", O_CREAT, 0666);
		if (fd != -1) close(fd);


		free(foo);

		foo = malloc(5);

		/*sleep(1);*/

		free(foo);

		foo = malloc(5); /* <-- leaks */
#if 1
		free(foo);
#endif

		/*if (i % 30 == 0) 
			fd = open("/tmp/foo", O_CREAT, 0666);*/
		/*fd = open("/tmp/foo", O_CREAT, 0666);*/

/*		close(1002);*/

	}
}

int main()
{
	struct sigaction act;
	sigset_t         empty_mask;
	int i;
  
	srand(time(0));

	sigfillset(&empty_mask);
	sigprocmask(SIG_BLOCK,&empty_mask,NULL);

/*	start_log_thread();*/

	for (i = 0; i < 13; i++) 
		(void) spawn_detached_thread(nasty_thread, NULL);

	sigprocmask(SIG_UNBLOCK,&empty_mask,NULL);
	sigemptyset(&empty_mask);
	act.sa_handler = signal_handler;
	act.sa_mask    = empty_mask;
	act.sa_flags   = 0;
  
	sigaction(SIGINT,  &act, 0);
	sigaction(SIGHUP,  &act, 0);
	sigaction(SIGSEGV, &act, 0);
	sigaction(SIGABRT, &act, 0);
	sigaction(SIGUSR1, &act, 0);
	sigaction(SIGUSR2, &act, 0);
	for (;;) sleep(120);
	return 0;
}

