#include "hw2.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>


static pthread_mutex_t *counter_locks = NULL;
static int g_num_counters_stored = 0;

int init_counters(int num_counters)
{
    /* Validation: strict bounds check against the limit (MAX_COUNTERS=100) */
    if (num_counters <= 0 || num_counters > MAX_COUNTERS) {
        return -1;
    }

    g_num_counters_stored = num_counters;

    /* Dynamic allocation for the locks based on input arguments */
    counter_locks = malloc(sizeof(pthread_mutex_t) * num_counters);
    if (!counter_locks) {
        perror("Malloc failed for counter locks");
        return -1;
    }

    char filename[64];
    for (int i = 0; i < num_counters; i++) {
        /* Initialize the mutex for this specific counter index */
        if (pthread_mutex_init(&counter_locks[i], NULL) != 0) {
            perror("Mutex init failed");
            return -1;
        }

        /* Each counter file is called countxx.txt... */

        
        sprintf(filename, "count%02d.txt", i);

        FILE *f = fopen(filename, "w");
        if (!f) {
            perror("Failed to create counter file");
            return -1;
        }
        
        /* '\n' to ensure it is a valid POSIX text file.*/
        fprintf(f, "%lld\n", 0LL);
        fclose(f);
    }

    return 0;
}

void close_counters(void)
{
    /*  cleanup (release memory...)*/
    if (counter_locks) {
        for (int i = 0; i < g_num_counters_stored; i++) {
            pthread_mutex_destroy(&counter_locks[i]);
        }
        free(counter_locks);
        counter_locks = NULL;
    }
}

/* * Helper function to handle the Read-Modify-Write cycle safely.*/
static int update_counter(int idx, int change)
{
    if (idx < 0 || idx >= g_num_counters_stored) {
        fprintf(stderr, "Error: Counter index %d out of bounds\n", idx);
        return -1;
    }

    /*  Lock the specific counter before touching the file.
     prevents race conditions between worker threads.*/
    pthread_mutex_lock(&counter_locks[idx]);

    char filename[64];
    sprintf(filename, "count%02d.txt", idx);

    long long val = 0;

    /* READ current value from disk */
    FILE *f = fopen(filename, "r");
    if (f) {
        if (fscanf(f, "%lld", &val) != 1) {
            val = 0; /* Fallback if file is corrupted/empty */
        }
        fclose(f);
    } else {
        /* If file is missing, cannot increment. Unlock and fail. */
        perror("Error reading counter file");
        pthread_mutex_unlock(&counter_locks[idx]);
        return -1;
    }

    /* MODIFY value in memory */
    if (change > 0) val++;
    else val--;

    /* WRITE new value back to disk.*/
    f = fopen(filename, "w");
    if (!f) {
        perror("Error writing counter file");
        pthread_mutex_unlock(&counter_locks[idx]);
        return -1;
    }
    
    /* Write the new value with a newline */
    fprintf(f, "%lld\n", val);
    fclose(f);

    /* Unlock so other threads can use this counter */
    pthread_mutex_unlock(&counter_locks[idx]);
    return 0;
}

int increment_counter(int idx)
{
    return update_counter(idx, 1);
}

int decrement_counter(int idx)

{
    return update_counter(idx, -1);
}
