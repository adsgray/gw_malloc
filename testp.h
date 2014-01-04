
#define MM_ENABLED mm_enter_hook()
#define MM_RETURN  mm_exit_hook()

extern int mm_enter_hook();
extern void mm_exit_hook();

/* dump contents of hash tables: */
void mm_log(log_func_t log);

/* enable/disable: */
void mm_toggle();
