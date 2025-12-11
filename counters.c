#include "hw2.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* * We need an array of mutexes, one for each counter file.
 * This ensures that if Thread A is updating counter 0, Thread B 
 * has to wait before it can touch counter 0.
 */
static pthread_mutex_t *counter_locks = NULL;
static int g_num_counters_stored = 0;

int init_counters(int num_counters)
{
    if (num_counters <= 0) return -1;

    g_num_counters_stored = num_counters;

    /* Allocate memory for the locks */
    counter_locks = malloc(sizeof(pthread_mutex_t) * num_counters);
    if (!counter_locks) {
        perror("Malloc failed for counter locks");
        return -1;
    }

    char filename[64];
    for (int i = 0; i < num_counters; i++) {
        /* Initialize the mutex for this specific counter */
        if (pthread_mutex_init(&counter_locks[i], NULL) != 0) {
            perror("Mutex init failed");
            return -1;
        }

        /* * Create/Reset the file: countxx.txt
         * Requirements:
         * 1. Filename format: countxx.txt (two digits) 
         * 2. Initial content: "0" in %lld format [cite: 19]
         */
        sprintf(filename, "count%02d.txt", i);
        FILE *f = fopen(filename, "w");
        if (!f) {
            perror("Failed to create counter file");
            return -1;
        }
        fprintf(f, "%lld", 0LL);
        fclose(f);
    }

    return 0;
}

void close_counters(void)
{
    if (counter_locks) {
        for (int i = 0; i < g_num_counters_stored; i++) {
            pthread_mutex_destroy(&counter_locks[i]);
        }
        free(counter_locks);
        counter_locks = NULL;
    }
}

/* Helper function to handle the Read-Modify-Write cycle safely */
static int update_counter(int idx, int change)
{
    if (idx < 0 || idx >= g_num_counters_stored) {
        fprintf(stderr, "Error: Counter index %d out of bounds\n", idx);
        return -1;
    }

    /* LOCK: Prevent other threads from touching this file */
    pthread_mutex_lock(&counter_locks[idx]);

    char filename[64];
    sprintf(filename, "count%02d.txt", idx);

    long long val = 0;

    /* STEP 1: READ current value */
    FILE *f = fopen(filename, "r");
    if (f) {
        if (fscanf(f, "%lld", &val) != 1) {
            val = 0; /* Should not happen if init worked, but safe fallback */
        }
        fclose(f);
    } else {
        /* If file doesn't exist, we can't increment. Error out or assume 0. */
        perror("Error reading counter file");
        pthread_mutex_unlock(&counter_locks[idx]);
        return -1;
    }

    /* STEP 2: MODIFY value */
    if (change > 0) val++;
    else val--;

    /* STEP 3: WRITE new value (fopen with "w" truncates/overwrites) */
    f = fopen(filename, "w");
    if (!f) {
        perror("Error writing counter file");
        pthread_mutex_unlock(&counter_locks[idx]);
        return -1;
    }
    fprintf(f, "%lld", val);
    fclose(f);

    /* UNLOCK: Done */
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
