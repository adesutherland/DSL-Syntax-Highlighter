#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include "dslsyntax_log.h"

int cb_debug_enabled = 0;
FILE *cb_log_file = NULL;

void cb_log_init(const char *filename) {
    if (filename == NULL) {
        cb_log_file = stderr;
    } else {
        cb_log_file = fopen(filename, "a");
        if (!cb_log_file) {
            perror("Failed to open log file");
            cb_log_file = stderr;
        }
    }
    cb_debug_enabled = 1;
    cb_log("--- Log started ---\n");
}

void cb_log_close() {
    if (cb_log_file && cb_log_file != stderr) {
        fclose(cb_log_file);
    }
    cb_log_file = NULL;
    cb_debug_enabled = 0;
}

void cb_log(const char *format, ...) {
    if (!cb_log_file) return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[26];
    strftime(time_buf, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(cb_log_file, "[%s] ", time_buf);

    va_list args;
    va_start(args, format);
    vfprintf(cb_log_file, format, args);
    va_end(args);

    fprintf(cb_log_file, "\n");
    fflush(cb_log_file);
}
