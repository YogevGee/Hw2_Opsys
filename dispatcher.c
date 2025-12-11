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

/* Internal handlers for dispatcher command and workers job */
static int handle_dispatcher_command(char *line)
{
    /* line starts with "dispatcher" (already trimmed) */
    char buf[MAX_LINE_LEN];
    strncpy(buf, line, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';

    /* Tokenize: dispatcher <subcommand> [arg] */
    char *token = strtok(buf, " \t");
    if (!token) {
        return 0; /* empty / weird line, ignore */
    }

    /* token == "dispatcher" */
    char *subcmd = strtok(NULL, " \t");
    if (!subcmd) {
        fprintf(stderr, "dispatcher: missing subcommand in line: %s\n", line);
        return -1;
    }

    if (strcmp(subcmd, "msleep") == 0) {
        char *arg = strtok(NULL, " \t");
        if (!arg) {
            fprintf(stderr, "dispatcher msleep: missing argument in line: %s\n", line);
            return -1;
        }

        char *endptr = NULL;
        long val = strtol(arg, &endptr, 10);
        if (endptr == arg || *endptr != '\0' || val < 0) {
            fprintf(stderr, "dispatcher msleep: invalid value '%s' in line: %s\n", arg, line);
            return -1;
        }

        /* Sleep x ms in the dispatcher (main) thread */
        msleep_ms((long long)val);
        return 0;
    }
    else if (strcmp(subcmd, "wait") == 0) {
        /* Wait for all pending worker jobs */
        worker_pool_wait_all();
        return 0;
    }
    else {
        fprintf(stderr, "dispatcher: unknown subcommand '%s' in line: %s\n", subcmd, line);
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

        /* Find first token: "dispatcher" or "worker" or something else */
        char *first = strtok(parse_buf, " \t");
        if (!first) {
            continue;
        }

        if (strcmp(first, "dispatcher") == 0) {
            /* Handle dispatcher msleep/wait (serially in main thread) */
            if (handle_dispatcher_command(line_buf) != 0) {
                /* We log errors to stderr but continue processing other lines */
                /* If you prefer, you can decide to return -1 here. */
            }
        }
        else if (strcmp(first, "worker") == 0) {
            /* Pass the FULL original line (without newline, but with original spacing)
               to the worker pool. The job's dispatch time is now_ms. */
            if (handle_worker_command(line_buf, now_ms) != 0) {
                /* Again, error printed; we continue. */
            }
        }
        else {
            /* Unknown command line – not dispatcher/worker; ignore or warn */
            fprintf(stderr, "Unknown command (ignored): %s\n", line_buf);
        }
    }

    if (logf) {
        fclose(logf);
    }
    fclose(cmdf);
    return 0;
}
