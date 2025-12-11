#include "hw2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* Per-job statistics structure*/
typedef struct job_info {
    char       *line_copy;        /* copy of the "worker ..." line*/
    long long   dispatch_time_ms; /* time dispatcher read the line */
    long long   finish_time_ms;   /* time worker finished processing */
    int         finished;         /* 1 if worker reported completion */
} job_info_t;

/* Global (file-local) state for statistics */
static job_info_t  *g_jobs         = NULL;  /* dynamic array of jobs */
static int          g_jobs_count   = 0;     /* how many jobs in use  */
static int          g_jobs_cap     = 0;     /* allocated capacity  */
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ensure capacity */
static int ensure_capacity_locked(int min_capacity)
{
    if (g_jobs_cap >= min_capacity) {
        return 0;
    }

    int new_cap = (g_jobs_cap == 0) ? 16 : g_jobs_cap * 2;
    if (new_cap < min_capacity) {
        new_cap = min_capacity;
    }

    job_info_t *new_arr = (job_info_t *)realloc(g_jobs,
                           (size_t)new_cap * sizeof(job_info_t));
    if (!new_arr) {
        return -1; /* allocation failed */
    }

    /* Initialize newly added slots */
    for (int i = g_jobs_cap; i < new_cap; i++) {
        new_arr[i].line_copy        = NULL;
        new_arr[i].dispatch_time_ms = 0;
        new_arr[i].finish_time_ms   = 0;
        new_arr[i].finished         = 0;
    }

    g_jobs     = new_arr;
    g_jobs_cap = new_cap;
    return 0;
}

/* Register a new job */
int stats_register_job(const char *line, long long dispatch_time_ms)
{
    if (!line) {
        return -1;
    }

    if (pthread_mutex_lock(&g_stats_mutex) != 0) {
        return -1;
    }

    /* Make sure we have space for one more job */
    if (ensure_capacity_locked(g_jobs_count + 1) != 0) {
        pthread_mutex_unlock(&g_stats_mutex);
        return -1;
    }

    int job_id = g_jobs_count;

    g_jobs[job_id].dispatch_time_ms = dispatch_time_ms;
    g_jobs[job_id].finish_time_ms   = 0;
    g_jobs[job_id].finished         = 0;

    /* Store a copy of the line (for debugging/logging if needed) */
    size_t len = strlen(line);
    g_jobs[job_id].line_copy = (char *)malloc(len + 1);
    if (g_jobs[job_id].line_copy) {
        memcpy(g_jobs[job_id].line_copy, line, len + 1);
    }

    g_jobs_count++;

    pthread_mutex_unlock(&g_stats_mutex);
    return job_id;
}

/* Mark a job as finished (called by worker threads)*/
void stats_job_finished(int job_id, long long finish_time_ms)
{
    if (job_id < 0) {
        return;
    }

    if (pthread_mutex_lock(&g_stats_mutex) != 0) {
        return;
    }

    if (job_id < g_jobs_count) {
        g_jobs[job_id].finish_time_ms = finish_time_ms;
        g_jobs[job_id].finished       = 1;
    }

    pthread_mutex_unlock(&g_stats_mutex);
}

/* Compute and write global statistics to stats.txt*/
void stats_write_file(const char *filename)
{
    if (!filename) {
        filename = "stats.txt";
    }

    /* total running time: now - g_start_time_ms */
    long long now_ms = get_time_ms();
    long long total_running_time = now_ms - g_start_time_ms;
    if (total_running_time < 0) {
        total_running_time = 0;
    }

    long long sum_turnaround = 0;
    long long min_turnaround = 0;
    long long max_turnaround = 0;
    double    avg_turnaround = 0.0;
    int       num_jobs_counted = 0;

    /* Copy data under lock*/
    if (pthread_mutex_lock(&g_stats_mutex) != 0) {
        return;
    }

    for (int i = 0; i < g_jobs_count; i++) {
        if (!g_jobs[i].finished) {
            /* Ideally shouldn't happen if worker_pool_wait_all() worked */
            continue;
        }

        long long ta = g_jobs[i].finish_time_ms - g_jobs[i].dispatch_time_ms;
        if (ta < 0) ta = 0;  /* guard against any clock weirdness */

        if (num_jobs_counted == 0) {
            min_turnaround = ta;
            max_turnaround = ta;
        } else {
            if (ta < min_turnaround) min_turnaround = ta;
            if (ta > max_turnaround) max_turnaround = ta;
        }

        sum_turnaround += ta;
        num_jobs_counted++;
    }

    if (num_jobs_counted > 0) {
        avg_turnaround = (double)sum_turnaround / (double)num_jobs_counted;
    } else {
        /* No worker jobs: define min/max/avg as 0 */
        min_turnaround = 0;
        max_turnaround = 0;
        avg_turnaround = 0.0;
    }

    pthread_mutex_unlock(&g_stats_mutex);

    /* Now write to stats.txt in the required format */
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("stats_write_file: fopen");
        return;
    }

    fprintf(f, "total running time: %lld milliseconds\n", total_running_time);
    fprintf(f, "sum of jobs turnaround time: %lld milliseconds\n", sum_turnaround);
    fprintf(f, "min job turnaround time: %lld milliseconds\n", min_turnaround);
    fprintf(f, "average job turnaround time: %f milliseconds\n", avg_turnaround);
    fprintf(f, "max job turnaround time: %lld milliseconds\n", max_turnaround);

    fclose(f);

}

