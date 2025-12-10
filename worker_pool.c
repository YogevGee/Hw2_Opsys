#include "hw2.h"
#include <stdio.h>

int worker_pool_init(int num_threads)
{
    (void)num_threads;
    return 0;
}

int worker_pool_submit(const char *line, long long dispatch_time_ms)
{
    (void)line;
    (void)dispatch_time_ms;
    return 0;
}

void worker_pool_wait_all(void)
{
}

void worker_pool_shutdown(void)
{
}
