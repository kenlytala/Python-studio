#ifndef PRISME_RUNNER_H
#define PRISME_RUNNER_H
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    bool running, stopped, exited, eof;
    int exit_code, pid;
    int out_fd, in_fd;
    void *process, *job, *out_handle, *in_handle;
    double started, stop_time;
    char temporary[4096];
} Runner;

void runner_init(Runner *r);
bool runner_start(Runner *r, const char *python, const char *source, const char *filename, const char *cwd,
                  char *error, size_t error_size);
/* External analysis adapter: isolated Python (-I -B), no project module shadowing. */
bool runner_start_tool(Runner *r, const char *python, const char *source, const char *filename,
                       const char *cwd, char *error, size_t error_size);
/* Renvoie un nombre d'octets non bloquant ; la fin est annoncée par running=false. */
size_t runner_poll(Runner *r, char *out, size_t capacity);
bool runner_input(Runner *r, const char *text);
void runner_stop(Runner *r);
void runner_close(Runner *r);
double runner_clock(void);
#endif
