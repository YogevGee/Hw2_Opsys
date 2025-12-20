# define _POSIX_C_SOURCE 200809L
#include "hw2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> 

typedef struct job_node{
    int job_id;
    char *line;
    struct job_node *next;
} job_node_t;

static job_node_t *q_head = NULL;
static job_node_t *q_tail = NULL;

static pthread_mutex_t q_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t q_cond = PTHREAD_COND_INITIALIZER;

static pthread_cond_t q_empty_cond = PTHREAD_COND_INITIALIZER;

static long long outstanding_jobs = 0;
static int shutdown_flag = 0;

static pthread_t *threads = NULL;
static int *thread_indices = NULL;
static int worker_count = 0;

static void *worker_thread_main(void *arg);
static void execute_job_commands(const job_node_t *job, FILE *logf, int thread_index);
static void execute_command_list(char *commands, FILE *logf, int thread_index);
static void execute_single_command(char *cmd, FILE *logf, int thread_index);


int worker_pool_init(int num_threads)
{
    int i;
    int rc;

    // basic sanity check
    if (num_threads <= 0 || num_threads > MAX_THREADS) {
        fprintf(stderr, "worker_pool_init: invalid num_threads=%d\n", num_threads);
        return -1;
    }

    worker_count = num_threads;
    shutdown_flag = 0;
    outstanding_jobs = 0;
    q_head = q_tail = NULL;

    /* allocate arrays for threads + indices */
    threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
    thread_indices = (int *)malloc(sizeof(int) * num_threads);
    if (threads == NULL || thread_indices == NULL) {
        fprintf(stderr, "worker_pool_init: malloc failed\n");
        free(threads);
        free(thread_indices);
        threads = NULL;
        thread_indices = NULL;
        return -1;
    }

    /* create all worker threads */
    for (i = 0; i < num_threads; i++) {
        thread_indices[i] = i;   /* each thread gets its own index */

        rc = pthread_create(
            &threads[i],
            NULL,                 /* default attributes */
            worker_thread_main,   /* thread function */
            &thread_indices[i]    /* argument = pointer to index */
        );
        if (rc != 0) {
            fprintf(stderr, "worker_pool_init: pthread_create failed on %d\n", i);

            /* if creation failed in the middle, shut down what we started */
            shutdown_flag = 1;
            pthread_cond_broadcast(&q_cond);

            /* join already-created threads */
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }

            free(threads);
            free(thread_indices);
            threads = NULL;
            thread_indices = NULL;
            return -1;
        }
    }

    return 0;   /* success */
}

int worker_pool_submit(const char *line, long long dispatch_time_ms)
{

    if (shutdown_flag) {
        fprintf(stderr, "worker_pool_submit: called after shutdown\n");
        return -1;
    }

    /* נרשום את העבודה בסטטיסטיקות ונקבל job_id */
    int job_id = stats_register_job(line, dispatch_time_ms);
    if (job_id < 0) {
        fprintf(stderr, "worker_pool_submit: stats_register_job failed\n");
        return -1;
    }

    /* ניצור node חדש לתור */
    job_node_t *node = (job_node_t *)malloc(sizeof(job_node_t));
    if (!node) {
        fprintf(stderr, "worker_pool_submit: malloc failed\n");
        return -1;
    }

    node->job_id = job_id;
    node->line   = strdup(line);   /* צריך #include <string.h> למעלה */
    node->next   = NULL;

    if (!node->line) {
        fprintf(stderr, "worker_pool_submit: strdup failed\n");
        free(node);
        return -1;
    }

    /* נכניס לתור בצורה מוגנת ב-mutex */
    pthread_mutex_lock(&q_mutex);

    if (q_tail) {
        q_tail->next = node;
        q_tail = node;
    } else {
        q_head = q_tail = node;
    }

    outstanding_jobs++;              /* עוד עבודה פתוחה */
    pthread_cond_signal(&q_cond);    /* להעיר worker אחד שיש עבודה */

    pthread_mutex_unlock(&q_mutex);

    return 0;
}

void worker_pool_wait_all(void)
{
     pthread_mutex_lock(&q_mutex);

    /* מחכים עד שאין תור ואין עבודות פתוחות */
    while (outstanding_jobs > 0 || q_head != NULL) {
        pthread_cond_wait(&q_empty_cond, &q_mutex);
    }

    pthread_mutex_unlock(&q_mutex);
}

void worker_pool_shutdown(void)
{
       /* מסמנים ל-threads להיסגר ומעירים את כולם */
    pthread_mutex_lock(&q_mutex);
    shutdown_flag = 1;
    pthread_cond_broadcast(&q_cond);
    pthread_mutex_unlock(&q_mutex);

    /* מצטרפים לכל ה-threads */
    if (threads != NULL) {
        for (int i = 0; i < worker_count; i++) {
            pthread_join(threads[i], NULL);
        }
        free(threads);
        threads = NULL;
    }

    if (thread_indices != NULL) {
        free(thread_indices);
        thread_indices = NULL;
    }

    /* מנקים עבודות שנשארו במקרה בתור */
    pthread_mutex_lock(&q_mutex);
    job_node_t *cur = q_head;
    while (cur) {
        job_node_t *next = cur->next;
        free(cur->line);
        free(cur);
        cur = next;
    }
    q_head = q_tail = NULL;
    pthread_mutex_unlock(&q_mutex);
}

static void *worker_thread_main(void *arg)
{
    int thread_index = *(int *)arg;

    /* פותחים קובץ לוג רק אם יש לוגינג */
    FILE *logf = NULL;
    char fname[64];

    if (g_log_enabled) {
        snprintf(fname, sizeof(fname), "thread%02d.txt", thread_index);
        logf = fopen(fname, "w");
        if (!logf) {
            perror("worker_thread_main: fopen log file");
        }
    }

    while (1) {
        pthread_mutex_lock(&q_mutex);

        /* מחכים לעבודה או ל-shutdown */
        while (!shutdown_flag && q_head == NULL) {
            pthread_cond_wait(&q_cond, &q_mutex);
        }

        /* אם אין עבודות ויש shutdown – יוצאים */
        if (shutdown_flag && q_head == NULL) {
            pthread_mutex_unlock(&q_mutex);
            break;
        }

        /* שולפים עבודה מהתור */
        job_node_t *job = q_head;
        q_head = job->next;
        if (q_head == NULL) {
            q_tail = NULL;
        }

        pthread_mutex_unlock(&q_mutex);

        /* לוג – START */
        long long now_ms = get_time_ms() - g_start_time_ms;
        if (g_log_enabled && logf) {
            fprintf(logf, "TIME %lld: START job %s\n", now_ms, job->line);
            fflush(logf);
        }

        /* כאן בפועל מריצים את הפקודות של העבודה */
        execute_job_commands(job, logf, thread_index);

        /* לוג – END */
        now_ms = get_time_ms() - g_start_time_ms;
        if (g_log_enabled && logf) {
            fprintf(logf, "TIME %lld: END job %s\n", now_ms, job->line);
            fflush(logf);
        }

        /* מדווחים לסטטיסטיקות על סיום העבודה */
        stats_job_finished(job->job_id, now_ms);

        /* עדכון מונה העבודות + אולי להעיר את wait_all */
        pthread_mutex_lock(&q_mutex);
        outstanding_jobs--;
        if (outstanding_jobs == 0 && q_head == NULL) {
            pthread_cond_broadcast(&q_empty_cond);
        }
        pthread_mutex_unlock(&q_mutex);

        /* משחררים זיכרון של ה-job */
        free(job->line);
        free(job);
    }

    if (logf) {
        fclose(logf);
    }

    return NULL;
}


static void execute_job_commands(const job_node_t *job, FILE *logf, int thread_index)
{
    (void)logf;        /* כרגע לא משתמשים בלוג ברמת פקודה */
    (void)thread_index;

    char buf[MAX_LINE_LEN + 1];
    strncpy(buf, job->line, MAX_LINE_LEN);
    buf[MAX_LINE_LEN] = '\0';

    char *p = buf;

    while (isspace((unsigned char)*p)) p++;

    /* מדלגים על המילה "worker" ואם יש – גם על מספר job פנימי */
    if (strncmp(p, "worker", 6) == 0 && isspace((unsigned char)p[6])) {
        p += 6;
        while (isspace((unsigned char)*p)) p++;
        if (isdigit((unsigned char)*p)) {
            char *endptr;
            (void)strtol(p, &endptr, 10);
            p = endptr;
            while (isspace((unsigned char)*p)) p++;
        }
    }

    execute_command_list(p, logf, thread_index);
}

/* מפצלת לרשימת פקודות לפי ';' ומריצה כל אחת בנפרד */
static void execute_command_list(char *commands, FILE *logf, int thread_index)
{
    char *cmd_array[128];   // enough for most reasonable lines
    int   cmd_count = 0;

    char *saveptr = NULL;

    /* First pass: split into trimmed commands, store pointers in cmd_array */
    for (char *token = strtok_r(commands, ";", &saveptr);
         token != NULL && cmd_count < 128;
         token = strtok_r(NULL, ";", &saveptr)) {

        char *cmd = token;

        /* Trim leading whitespace */
        while (isspace((unsigned char)*cmd)) cmd++;

        /* Trim trailing whitespace */
        char *end = cmd + strlen(cmd);
        while (end > cmd && isspace((unsigned char)end[-1])) {
            end--;
        }
        *end = '\0';

        if (*cmd == '\0') {
            continue;  // empty chunk, skip
        }

        cmd_array[cmd_count++] = cmd;
    }

    /* Second pass: execute with "repeat N; suffix" semantics.
       repeat N; cmdX; cmdY; ...  => run {cmdX..cmdY} N times and stop. */
    int i = 0;
    while (i < cmd_count) {
        char *cmd = cmd_array[i];

        /* Check for "repeat" as a standalone keyword followed by a number */
        if (strncmp(cmd, "repeat", 6) == 0 &&
            (cmd[6] == '\0' || isspace((unsigned char)cmd[6]))) {

            char *p = cmd + 6;
            while (isspace((unsigned char)*p)) p++;

            if (*p == '\0') {
                // "repeat" with no number – ignore the rest of the line
                return;
            }

            char *endptr;
            long times = strtol(p, &endptr, 10);
            if (times <= 0) {
                // non-positive repeat – do nothing more
                return;
            }

            /* After the number, only whitespace is allowed on this token */
            while (isspace((unsigned char)*endptr)) endptr++;
            if (*endptr != '\0') {
                // Something extra after the number on the same token – malformed
                fprintf(stderr, "worker: malformed repeat: '%s'\n", cmd);
                return;
            }

            /* Suffix commands are all commands after "repeat" on this line */
            for (long t = 0; t < times; t++) {
                for (int j = i + 1; j < cmd_count; j++) {
                    execute_single_command(cmd_array[j], logf, thread_index);
                }
            }

            /* After a repeat, spec says: repeat to END OF LINE, then stop. */
            break;
        } else {
            /* Normal command, just execute once and move on */
            execute_single_command(cmd, logf, thread_index);
            i++;
        }
    }
}

/* Run a single basic command:
     msleep X
     increment I
     decrement I
   (repeat is handled at the list level in execute_command_list)
*/
static void execute_single_command(char *cmd, FILE *logf, int thread_index)
{
    (void)logf;
    (void)thread_index;

    /* msleep X */
    if (strncmp(cmd, "msleep", 6) == 0 &&
        (cmd[6] == '\0' || isspace((unsigned char)cmd[6]))) {

        long long ms;
        if (sscanf(cmd + 6, "%lld", &ms) == 1 && ms >= 0) {
            msleep_ms(ms);
        }
        return;
    }

    /* increment I */
    if (strncmp(cmd, "increment", 9) == 0 &&
        (cmd[9] == '\0' || isspace((unsigned char)cmd[9]))) {

        int idx;
        if (sscanf(cmd + 9, "%d", &idx) == 1) {
            increment_counter(idx);
        }
        return;
    }

    /* decrement I */
    if (strncmp(cmd, "decrement", 9) == 0 &&
        (cmd[9] == '\0' || isspace((unsigned char)cmd[9]))) {

        int idx;
        if (sscanf(cmd + 9, "%d", &idx) == 1) {
            decrement_counter(idx);
        }
        return;
    }

    /* Unknown command – helpful for debugging */
    fprintf(stderr, "worker: unknown command: '%s'\n", cmd);
}
