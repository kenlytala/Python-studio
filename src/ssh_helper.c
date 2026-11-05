#define _GNU_SOURCE
#include "ssh.h"
#include "ssh_json.h"
#include "buffer.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__linux__)
#include <dirent.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

static bool write_all(int fd, const char *s, size_t n) {
    while (n) {
        ssize_t sent = write(fd, s, n);
        if (sent < 0 && errno == EINTR)
            continue;
        if (sent <= 0)
            return false;
        s += sent;
        n -= (size_t)sent;
    }
    return true;
}
static int askpass(const char *prompt) {
    const char *path = getenv("PRISME_ASKPASS_SOCKET"), *token = getenv("PRISME_ASKPASS_TOKEN");
    if (!path || !token || strlen(path) >= sizeof(((struct sockaddr_un *)0)->sun_path) ||
        strlen(token) != 64 || strlen(prompt) > SSH_PROMPT_MAX)
        return 1;
    struct stat st;
    if (lstat(path, &st) || !S_ISSOCK(st.st_mode) || st.st_uid != getuid() || (st.st_mode & 0077))
        return 1;
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return 1;
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
        close(fd);
        return 1;
    }
    struct ucred credentials;
    socklen_t size = sizeof(credentials);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &size) || credentials.uid != getuid()) {
        close(fd);
        return 1;
    }
    char *message = calloc(1, 65537);
    if (!message) {
        close(fd);
        return 1;
    }
    SjOut out;
    sj_out(&out, message, 65537);
    sj_add(&out, "{\"token\":");
    sj_quote(&out, token);
    sj_add(&out, ",\"prompt\":");
    sj_quote(&out, prompt);
    const char *hint = getenv("SSH_ASKPASS_PROMPT");
    bool confirm = (hint && !strcmp(hint, "confirm")) || (strstr(prompt, "yes/no") != NULL);
    sj_add(&out, confirm ? ",\"confirm\":true}\n" : ",\"confirm\":false}\n");
    if (!out.ok || !write_all(fd, out.text, out.len)) {
        free(message);
        close(fd);
        return 1;
    }
    size_t used = 0;
    bool complete = false;
    while (used < 65536) {
        struct pollfd ready = {fd, POLLIN, 0};
        int r = poll(&ready, 1, 300000);
        if (r < 0 && errno == EINTR)
            continue;
        if (r <= 0)
            break;
        ssize_t got = read(fd, message + used, 65536 - used);
        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0)
            break;
        used += (size_t)got;
        char *end = memchr(message, '\n', used);
        if (end) {
            used = (size_t)(end - message);
            complete = true;
            break;
        }
    }
    close(fd);
    SjDoc d;
    SjToken tokens[16];
    char answer[SSH_AUTH_MAX + 1];
    bool ok = complete && sj_parse(&d, message, used, tokens, 16) &&
              sj_string(&d, sj_get(&d, 0, "answer"), answer, sizeof(answer)) && !strpbrk(answer, "\r\n") &&
              (!confirm || !strcmp(answer, "yes") || !strcmp(answer, "no"));
    if (ok)
        ok = write_all(STDOUT_FILENO, answer, strlen(answer)) && write_all(STDOUT_FILENO, "\n", 1);
    ssh_erase(answer, sizeof(answer));
    ssh_erase(message, 65537);
    free(message);
    return ok ? 0 : 1;
}
int ssh_helper_main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "--probe")) {
        DIR *dir = opendir(argv[2]);
        if (!dir)
            return 1;
        errno = 0;
        (void)readdir(dir);
        int e = errno;
        if (closedir(dir))
            return 1;
        return e ? 1 : 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("Prisme C SSH helper 1.7.0");
        return 0;
    }
    return argc == 2 ? askpass(argv[1]) : 1;
}
#else
int ssh_helper_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return 1;
}
#endif
