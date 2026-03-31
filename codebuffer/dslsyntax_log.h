#ifndef DSLSYNTAX_LOG_H
#define DSLSYNTAX_LOG_H

#include <stdio.h>
#include <stdarg.h>

extern int cb_debug_enabled;
extern FILE *cb_log_file;

void cb_log_init(const char *filename);
void cb_log_close();
void cb_log(const char *format, ...);

#define LOG(fmt, ...) do { if (cb_debug_enabled) cb_log(fmt, ##__VA_ARGS__); } while (0)

#endif /* DSLSYNTAX_LOG_H */
