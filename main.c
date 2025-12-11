#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "hw2.h"


/* Define the global variables declared in hw2.h */
int g_num_threads = 0;
int g_num_counters = 0;
int g_log_enabled = 0;
long long g_start_time_ms = 0;

/* Forward declaration: implemented in dispatcher.c */
int dispatcher_run(const char *cmdfile);

/* Time utilities*/

long long get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long long ms = (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
    return ms;
}

void msleep_ms(long long ms)
{
    if (ms <= 0)
        return;

    struct timespec req;
    req.tv_sec  = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;

    /* Handle interruption by signals */
    while (nanosleep(&req, &req) == -1) {
        continue;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 5) {
        fprintf(stderr,
                "Usage: %s cmdfile.txt num_threads num_counters log_enabled\n",
                argv[0]);
        fprintf(stderr,
                "  num_threads  : 1..%d\n"
                "  num_counters : 1..%d\n"
                "  log_enabled  : 0 or 1\n",
                MAX_THREADS, MAX_COUNTERS);
        return 1;
    }

    const char *cmdfile = argv[1];

    g_num_threads  = atoi(argv[2]);
    g_num_counters = atoi(argv[3]);
    g_log_enabled  = atoi(argv[4]);

    /* Basic validation */
    if (g_num_threads <= 0 || g_num_threads > MAX_THREADS) {
        fprintf(stderr, "Error: num_threads must be between 1 and %d\n", MAX_THREADS);
        return 1;
    }
    if (g_num_counters <= 0 || g_num_counters > MAX_COUNTERS) {
        fprintf(stderr, "Error: num_counters must be between 1 and %d\n", MAX_COUNTERS);
        return 1;
    }
    if (g_log_enabled != 0 && g_log_enabled != 1) {
        fprintf(stderr, "Error: log_enabled must be 0 or 1\n");
        return 1;
    }

    /* Set start time for all logs and statistics */
    g_start_time_ms = get_time_ms();

    /* Initialize counters (Partner A) */
    if (init_counters(g_num_counters) != 0) {
        fprintf(stderr, "Error: init_counters failed\n");
        return 1;
    }

    /* Initialize worker pool (Partner B) */
    if (worker_pool_init(g_num_threads) != 0) {
        fprintf(stderr, "Error: worker_pool_init failed\n");
        close_counters();
        return 1;
    }

    /* Run dispatcher: read cmdfile, handle dispatcher/worker lines */
    if (dispatcher_run(cmdfile) != 0) {
        fprintf(stderr, "Error: dispatcher_run failed\n");
        worker_pool_shutdown();
        close_counters();
        return 1;
    }

    /* After finishing the file:
       wait for all background jobs, write stats, cleanup. */
    worker_pool_wait_all();
    stats_write_file("stats.txt");
    worker_pool_shutdown();
    close_counters();

    return 0;
}
