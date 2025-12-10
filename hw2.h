#ifndef HW2_H
#define HW2_H

#include <pthread.h>

#define MAX_THREADS   4096
#define MAX_COUNTERS  100
#define MAX_LINE_LEN  1024

/* -------------------------------------------------
 * Global configuration (set by main/dispatcher)
 * Implemented by: Partner A
 * ------------------------------------------------- */
extern int g_num_threads;
extern int g_num_counters;
extern int g_log_enabled;
extern long long g_start_time_ms;

/* -------------------------------------------------
 * Time utilities (Partner A)
 * We'll implement them later.
 * ------------------------------------------------- */
long long get_time_ms(void);
void msleep_ms(long long ms);

/* -------------------------------------------------
 * Counter file API (Partner A)
 * We'll implement these in counters.c
 * ------------------------------------------------- */
int init_counters(int num_counters);
void close_counters(void);
int increment_counter(int idx);
int decrement_counter(int idx);

/* -------------------------------------------------
 * Job statistics API (Partner A)
 * We'll implement these in stats.c
 * ------------------------------------------------- */
int stats_register_job(const char *line, long long dispatch_time_ms);
void stats_job_finished(int job_id, long long finish_time_ms);
void stats_write_file(const char *filename);

/* -------------------------------------------------
 * Worker pool / queue API (Partner B)
 * Implemented in worker_pool.c by your partner.
 * For now you have stub implementations so you can link.
 * ------------------------------------------------- */
int worker_pool_init(int num_threads);
int worker_pool_submit(const char *line, long long dispatch_time_ms);
void worker_pool_wait_all(void);
void worker_pool_shutdown(void);

/* -------------------------------------------------
 * Dispatcher API (PArtner A)
 * We'll implement this in dispatcher.c
 * ------------------------------------------------- */
int run_dispatcher(const char *cmdfile_path);

#endif /* HW2_H */
