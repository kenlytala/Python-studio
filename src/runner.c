#define _POSIX_C_SOURCE 200809L
#include "runner.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

/* Le lanceur est en C. Python externe exécute le script de l’utilisateur ou
   l’adaptateur d’analyse isolé ; l’interface ne dépend pas de Python.
   compile() conserve le vrai nom dans les exceptions et __file__. */
static const char *bootstrap =
    "import sys,linecache; p,n=sys.argv[1:3]; sys.argv=[n]; "
    "s=open(p,encoding='utf-8').read(); "
    "linecache.cache[n]=(len(s),None,s.splitlines(True),n); "
    "exec(compile(s,n,'exec'),{'__name__':'__main__','__file__':n,'__package__':None})";

double runner_clock(void) {
#ifdef _WIN32
    return (double)GetTickCount64() / 1000.0;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
#endif
}
void runner_init(Runner *r) {
    memset(r, 0, sizeof(*r));
    r->out_fd = r->in_fd = -1;
}

#ifdef _WIN32
static wchar_t *wide(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = malloc((size_t)n * sizeof(wchar_t));
    if (w)
        MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}
/* Quoting Windows argv : les antislashs avant un guillemet sont doublés. */
static bool append_arg(char *out, size_t cap, const char *arg) {
    size_t p = strlen(out), slashes = 0;
    if (p + 3 >= cap)
        return false;
    if (p)
        out[p++] = ' ';
    out[p++] = '"';
    for (size_t i = 0;; i++) {
        char c = arg[i];
        if (c == '\\') {
            slashes++;
            continue;
        }
        size_t count = (c == '"' || !c) ? slashes * 2 : slashes;
        if (p + count + 4 >= cap)
            return false;
        while (count--)
            out[p++] = '\\';
        slashes = 0;
        if (!c)
            break;
        if (c == '"')
            out[p++] = '\\';
        out[p++] = c;
    }
    out[p++] = '"';
    out[p] = 0;
    return true;
}
#endif

static bool start(Runner *r, const char *python, const char *source, const char *filename, const char *cwd,
                  char *error, size_t cap, bool isolated) {
    if (r->running) {
        snprintf(error, cap, "Un programme est déjà en cours");
        return false;
    }
    runner_init(r);
#ifdef _WIN32
    char tmpdir[MAX_PATH], tempfile[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, tmpdir) || !GetTempFileNameA(tmpdir, "pri", 0, tempfile)) {
        snprintf(error, cap, "Dossier temporaire inaccessible");
        return false;
    }
    snprintf(r->temporary, sizeof(r->temporary), "%s", tempfile);
    FILE *f = fopen(r->temporary, "wb");
#else
    const char *tmp = getenv("TMPDIR");
    snprintf(r->temporary, sizeof(r->temporary), "%s/prisme-c-XXXXXX", tmp && *tmp ? tmp : "/tmp");
    int fd = mkstemp(r->temporary);
    FILE *f = fd >= 0 ? fdopen(fd, "wb") : NULL;
#endif
    if (!f) {
        snprintf(error, cap, "%s", strerror(errno));
        return false;
    }
    size_t n = strlen(source);
    bool saved = fwrite(source, 1, n, f) == n;
    if (fclose(f) != 0)
        saved = false;
    if (!saved) {
        snprintf(error, cap, "Écriture temporaire impossible");
        remove(r->temporary);
        return false;
    }
#ifdef _WIN32
    HANDLE read_out = NULL, write_out = NULL, read_in = NULL, write_in = NULL;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    if (!CreatePipe(&read_out, &write_out, &sa, 0) || !CreatePipe(&read_in, &write_in, &sa, 0))
        goto win_fail;
    SetHandleInformation(read_out, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(write_in, HANDLE_FLAG_INHERIT, 0);
    char command[18000] = "";
    const char *args[10];
    int argc = 0;
    args[argc++] = python;
    if (isolated) {
        args[argc++] = "-I";
        args[argc++] = "-B";
    }
    args[argc++] = "-u";
    args[argc++] = "-X";
    args[argc++] = "utf8";
    args[argc++] = "-c";
    args[argc++] = bootstrap;
    args[argc++] = r->temporary;
    args[argc++] = filename;
    for (int i = 0; i < argc; i++)
        if (!append_arg(command, sizeof(command), args[i]))
            goto win_fail;
    wchar_t *wcommand = wide(command), *wcwd = wide(cwd);
    if (!wcommand || !wcwd) {
        free(wcommand);
        free(wcwd);
        goto win_fail;
    }
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = si.hStdError = write_out;
    si.hStdInput = read_in;
    BOOL created = CreateProcessW(NULL, wcommand, NULL, NULL, TRUE,
                                  CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, wcwd, &si, &pi);
    free(wcommand);
    free(wcwd);
    if (!created)
        goto win_fail;
    CloseHandle(pi.hThread);
    CloseHandle(write_out);
    CloseHandle(read_in);
    r->process = pi.hProcess;
    r->out_handle = read_out;
    r->in_handle = write_in;
    HANDLE job = CreateJobObjectW(NULL, NULL);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
        memset(&info, 0, sizeof(info));
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info)) &&
            AssignProcessToJobObject(job, pi.hProcess))
            r->job = job;
        else
            CloseHandle(job);
    }
#else
    int output[2], input[2];
    if (pipe(output) != 0) {
        snprintf(error, cap, "%s", strerror(errno));
        remove(r->temporary);
        return false;
    }
    if (pipe(input) != 0) {
        close(output[0]);
        close(output[1]);
        snprintf(error, cap, "%s", strerror(errno));
        remove(r->temporary);
        return false;
    }
    fflush(NULL);
    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        dup2(output[1], STDOUT_FILENO);
        dup2(output[1], STDERR_FILENO);
        dup2(input[0], STDIN_FILENO);
        close(output[0]);
        close(output[1]);
        close(input[0]);
        close(input[1]);
        if (chdir(cwd) != 0) {
            perror("Dossier de travail");
            _exit(126);
        }
        setenv("PYTHONIOENCODING", "utf-8", 1);
        if (isolated)
            execlp(python, python, "-I", "-B", "-u", "-X", "utf8", "-c", bootstrap, r->temporary, filename,
                   (char *)NULL);
        else
            execlp(python, python, "-u", "-X", "utf8", "-c", bootstrap, r->temporary, filename, (char *)NULL);
        perror("Interpréteur Python introuvable");
        _exit(127);
    }
    close(output[1]);
    close(input[0]);
    if (pid < 0) {
        close(output[0]);
        close(input[1]);
        remove(r->temporary);
        snprintf(error, cap, "%s", strerror(errno));
        return false;
    }
    setpgid(pid, pid);
    r->pid = (int)pid;
    r->out_fd = output[0];
    r->in_fd = input[1];
    fcntl(r->out_fd, F_SETFL, fcntl(r->out_fd, F_GETFL) | O_NONBLOCK);
    fcntl(r->in_fd, F_SETFL, fcntl(r->in_fd, F_GETFL) | O_NONBLOCK);
    fcntl(r->out_fd, F_SETFD, FD_CLOEXEC);
    fcntl(r->in_fd, F_SETFD, FD_CLOEXEC);
    signal(SIGPIPE, SIG_IGN);
#endif
    r->running = true;
    r->started = runner_clock();
    return true;
#ifdef _WIN32
win_fail:
    snprintf(error, cap, "Impossible de lancer Python (Windows %lu)", (unsigned long)GetLastError());
    if (read_out)
        CloseHandle(read_out);
    if (write_out)
        CloseHandle(write_out);
    if (read_in)
        CloseHandle(read_in);
    if (write_in)
        CloseHandle(write_in);
    remove(r->temporary);
    return false;
#endif
}

bool runner_start(Runner *r, const char *python, const char *source, const char *filename, const char *cwd,
                  char *error, size_t cap) {
    return start(r, python, source, filename, cwd, error, cap, false);
}
bool runner_start_tool(Runner *r, const char *python, const char *source, const char *filename,
                       const char *cwd, char *error, size_t cap) {
    return start(r, python, source, filename, cwd, error, cap, true);
}
size_t runner_poll(Runner *r, char *out, size_t cap) {
    if (!r->running || cap < 2)
        return 0;
    size_t total = 0;
#ifdef _WIN32
    DWORD available = 0, got = 0;
    if (PeekNamedPipe(r->out_handle, NULL, 0, NULL, &available, NULL)) {
        if (available && ReadFile(r->out_handle, out,
                                  (DWORD)((size_t)available < cap - 1 ? available : cap - 1), &got, NULL))
            total = got;
    } else
        r->eof = true;
    DWORD code = STILL_ACTIVE;
    if (GetExitCodeProcess(r->process, &code) && code != STILL_ACTIVE) {
        r->exited = true;
        r->exit_code = (int)code;
        if (!available && !total)
            r->eof = true;
    }
#else
    ssize_t got = read(r->out_fd, out, cap - 1);
    if (got > 0)
        total = (size_t)got;
    else if (got == 0 || (got < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
        r->eof = true;
    if (!r->exited) {
        int status = 0;
        pid_t p = waitpid((pid_t)r->pid, &status, WNOHANG);
        if (p > 0) {
            r->exited = true;
            r->exit_code =
                WIFEXITED(status) ? WEXITSTATUS(status) : 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
        }
    }
    if (r->stopped && runner_clock() - r->stop_time > 1.5 && !r->eof)
        kill(-(pid_t)r->pid, SIGKILL);
#endif
    out[total] = 0;
    if (r->exited && r->eof && !total) {
#ifdef _WIN32
        CloseHandle(r->process);
        CloseHandle(r->out_handle);
        CloseHandle(r->in_handle);
        if (r->job)
            CloseHandle(r->job);
        r->process = r->out_handle = r->in_handle = r->job = NULL;
#else
        close(r->out_fd);
        close(r->in_fd);
        r->out_fd = r->in_fd = -1;
#endif
        r->running = false;
        remove(r->temporary);
        r->temporary[0] = 0;
    }
    return total;
}
bool runner_input(Runner *r, const char *text) {
    if (!r->running)
        return false;
    size_t n = strlen(text);
    if (n > 4095)
        return false;
    char value[4098];
    memcpy(value, text, n);
    value[n++] = '\n';
#ifdef _WIN32
    DWORD written;
    return WriteFile(r->in_handle, value, (DWORD)n, &written, NULL) && written == n;
#else
    return write(r->in_fd, value, n) == (ssize_t)n;
#endif
}
void runner_stop(Runner *r) {
    if (!r->running || r->stopped)
        return;
    r->stopped = true;
    r->stop_time = runner_clock();
#ifdef _WIN32
    if (r->job)
        TerminateJobObject(r->job, 130);
    else
        TerminateProcess(r->process, 130);
#else
    kill(-(pid_t)r->pid, SIGTERM);
#endif
}
void runner_close(Runner *r) {
    if (!r->running)
        return;
    runner_stop(r);
    char discarded[4096];
    double end = runner_clock() + 2.5;
    while (r->running && runner_clock() < end) {
        runner_poll(r, discarded, sizeof(discarded));
#ifdef _WIN32
        Sleep(5);
#else
        struct timespec delay = {0, 5000000};
        nanosleep(&delay, NULL);
#endif
    }
    if (r->running) {
#ifdef _WIN32
        if (r->job) {
            TerminateJobObject(r->job, 130);
            CloseHandle(r->job);
        } else
            TerminateProcess(r->process, 130);
        CloseHandle(r->process);
        CloseHandle(r->out_handle);
        CloseHandle(r->in_handle);
#else
        kill(-(pid_t)r->pid, SIGKILL);
        waitpid((pid_t)r->pid, NULL, 0);
        close(r->out_fd);
        close(r->in_fd);
#endif
        r->running = false;
        remove(r->temporary);
    }
}
