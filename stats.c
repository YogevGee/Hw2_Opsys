#include "hw2.h"
#include <stdio.h>

long long get_time_ms(void)
{
    /* TODO: real implementation later */
    return 0;
}

void msleep_ms(long long ms)
{
    /* TODO: real implementation later */
    (void)ms;
}

int stats_register_job(const char *line, long long dispatch_time_ms)
{
    /* TODO: real implementation later */
    (void)line;
    (void)dispatch_time_ms;
    return 0;
}

void stats_job_finished(int job_id, long long finish_time_ms)
{
    /* TODO: real implementation later */
    (void)job_id;
    (void)finish_time_ms;
}

void stats_write_file(const char *filename)
{
    /* TODO: real implementation later */
    (void)filename;
}
