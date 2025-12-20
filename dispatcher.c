#include "hw2.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* strip newline and trim whitespaces*/
static void strip_newline(char *s)
{
    if (!s) return;
    size_t len = strlen(s);
    while(len > 0 && (s[len -1] == '\n' || s[len - 1] == '\r')){
        s[--len] = '\0';
    }
}
    
static void trim_whitespaces(char *s)
{
    if (!s) return;
    char *start = s;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != s){
        memmove(s, start, strlen(start) + 1);
    }
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static int handle_dispatcher_command(char *line)
{
    /* line starts with "dispatcher_msleep" or "dispatcher_wait" (already trimmed) */
    char buf[MAX_LINE_LEN];
    strncpy(buf, line, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';

    /* Tokenize: dispatcher_msleep [arg]   OR   dispatcher_wait */
    char *cmd = strtok(buf, " \t");
    if (!cmd) {
        return 0; /* empty / weird line, ignore */
    }

    if (strcmp(cmd, "dispatcher_msleep") == 0) {
        char *arg = strtok(NULL, " \t");
        if (!arg) {
            fprintf(stderr, "dispatcher_msleep: missing argument in line: %s\n", line);
            return -1;
        }

        char *endptr = NULL;
        long val = strtol(arg, &endptr, 10);
        if (endptr == arg || *endptr != '\0' || val < 0) {
            fprintf(stderr, "dispatcher_msleep: invalid value '%s' in line: %s\n", arg, line);
            return -1;
        }

        /* Sleep x ms in the dispatcher (main) thread */
        msleep_ms((long long)val);
        return 0;
    }
    else if (strcmp(cmd, "dispatcher_wait") == 0) {
        /* Wait for all pending worker jobs */
        worker_pool_wait_all();
        return 0;
    }
    else {
        fprintf(stderr, "dispatcher: unknown command '%s' in line: %s\n", cmd, line);
        return -1;
    }
}

static int handle_worker_command(char *line, long long dispatch_time_ms)
{
    /* line starts with "worker" (already trimmed).
     *  Send the entire line to the worker pool.
     */

    int rc = worker_pool_submit(line, dispatch_time_ms);
    if (rc != 0) {
        fprintf(stderr, "worker_pool_submit failed for line: %s\n", line);
        return -1;
    }
    return 0;
}





int dispatcher_run(const char *cmdfile_path)
{
     FILE *cmdf = fopen(cmdfile_path, "r");
    if (!cmdf) {
        fprintf(stderr, "Error: cannot open command file %s\n", cmdfile_path);
        return -1;
    }

    /* Open dispatcher log if logging is enabled */
    FILE *logf = NULL;
    if (g_log_enabled) {
        logf = fopen("dispatcher.txt", "w");
        if (!logf) {
            fprintf(stderr, "Warning: cannot open dispatcher.txt for writing\n");
            /* Not fatal, we can still continue without dispatcher log */
        }
    }

    char line_buf[MAX_LINE_LEN];

    while (fgets(line_buf, sizeof(line_buf), cmdf) != NULL) {
        /* Keep an "original" copy for logging (without trailing newline) */
        strip_newline(line_buf);

        /* If line is empty after stripping newline, just continue */
        if (line_buf[0] == '\0') {
            continue;
        }

        /* Compute current time and elapsed since hw2 started */
        long long now_ms = get_time_ms();
        long long elapsed_ms = now_ms - g_start_time_ms;

        /* Log to dispatcher.txt if enabled:
         * TIME %lld: read cmd line: %s
         */
        if (logf) {
            fprintf(logf, "TIME %lld: read cmd line: %s\n", elapsed_ms, line_buf);
            fflush(logf);
        }

        /* For parsing, work on a trimmed copy of the line */
        char parse_buf[MAX_LINE_LEN];
        strncpy(parse_buf, line_buf, sizeof(parse_buf));
        parse_buf[sizeof(parse_buf) - 1] = '\0';
        trim_whitespaces(parse_buf);

        if (parse_buf[0] == '\0') {
            /* Line was only spaces; nothing to do */
            continue;
        }

        /* Find first token: "dispatcher_msleep", "dispatcher_wait", "worker", or something else */
        char *first = strtok(parse_buf, " \t");
        if (!first) {
            continue;
        }

        if (strcmp(first, "dispatcher_msleep") == 0 ||
            strcmp(first, "dispatcher_wait")  == 0) {
            /* Handle dispatcher_msleep / dispatcher_wait (serially in main thread) */
            if (handle_dispatcher_command(line_buf) != 0) {
                /* error already printed, keep going */
            }
        }
        else if (strcmp(first, "worker") == 0) {
            /* Pass the full original line to the worker pool */
            if (handle_worker_command(line_buf, elapsed_ms) != 0) {
                /* error already printed */
            }
        }
        else {
            fprintf(stderr, "Unknown command (ignored): %s\n", line_buf);
        }

    }

    if (logf) {
        fclose(logf);
    }
    fclose(cmdf);
    return 0;
}
