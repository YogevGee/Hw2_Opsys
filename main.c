#include <stdio.h>
#include <stdlib.h>
#include "hw2.h"

/* Define the global variables declared in hw2.h */
int g_num_threads = 0;
int g_num_counters = 0;
int g_log_enabled = 0;
long long g_start_time_ms = 0;

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

    /* TODO (later Step): initialize time, counters, workers, dispatcher, stats, etc.
     * For now we just print something and exit to make sure linking works.
     */

    printf("HW2 skeleton running.\n");
    printf("  cmdfile      = %s\n", cmdfile);
    printf("  num_threads  = %d\n", g_num_threads);
    printf("  num_counters = %d\n", g_num_counters);
    printf("  log_enabled  = %d\n", g_log_enabled);

    return 0;
}
