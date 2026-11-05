#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "files.h"
#include <stdatomic.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static _Thread_local FsGuard operation_guard;
static _Thread_local void *guard_context;
void fs_set_guard(FsGuard guard, void *context) {
    operation_guard = guard;
    guard_context = context;
}
static bool guarded(const char *path, bool leaf, char *error, size_t cap) {
    return !operation_guard || operation_guard(guard_context, path, leaf, error, cap);
}
#ifndef _WIN32
static bool guarded_fd(int fd, const char *leaf, bool allow_link, char *error, size_t cap) {
    if (!operation_guard)
        return true;
#ifdef __linux__
    char proc[64], base[FS_PATH], path[FS_PATH];
    snprintf(proc, sizeof(proc), "/proc/self/fd/%d", fd);
    ssize_t n = readlink(proc, base, sizeof(base) - 1);
    if (n <= 0 || (size_t)n >= sizeof(base) - 1)
        return false;
    base[n] = 0;
    int used =
        leaf ? snprintf(path, sizeof(path), "%s/%s", base, leaf) : snprintf(path, sizeof(path), "%s", base);
    if (used < 0 || (size_t)used >= sizeof(path))
        return false;
    return guarded(path, allow_link, error, cap);
#else
    (void)fd;
    (void)leaf;
    (void)allow_link;
    (void)error;
    (void)cap;
    return false;
#endif
}
#endif
static bool fail(char *error, size_t cap, const char *format, ...) {
    if (cap) {
        va_list args;
        va_start(args, format);
        vsnprintf(error, cap, format, args);
        va_end(args);
    }
    return false;
}
static bool separator(unsigned char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}
const char *fs_basename(const char *path) {
    if (!path)
        return "";
    const char *name = path;
    for (const char *p = path; *p; p++)
        if (separator((unsigned char)*p))
            name = p + 1;
    return name;
}
static bool join(char *out, const char *base, const char *name) {
    size_t n = strlen(base), m = strlen(name);
    bool slash = n && !separator((unsigned char)base[n - 1]);
    if (!n || n + m + (slash ? 1u : 0u) >= FS_PATH)
        return false;
    memcpy(out, base, n);
    if (slash)
        out[n++] = '/';
    memcpy(out + n, name, m + 1);
    return true;
}
static bool absolute(const char *s) {
#ifdef _WIN32
    return s && ((isalpha((unsigned char)s[0]) && s[1] == ':' && (s[2] == '/' || s[2] == '\\')) ||
                 (s[0] == '\\' && s[1] == '\\'));
#else
    return s && s[0] == '/';
#endif
}
static int path_char(unsigned char c) {
#ifdef _WIN32
    if (c == '\\')
        c = '/';
    return tolower(c);
#else
    return c;
#endif
}
bool fs_under(const char *parent, const char *path) {
    if (!parent || !path || !*parent || !*path)
        return false;
    size_t n = strlen(parent);
    while (n > 1 && separator((unsigned char)parent[n - 1]))
        n--;
    for (size_t i = 0; i < n; i++)
        if (!path[i] || path_char((unsigned char)parent[i]) != path_char((unsigned char)path[i]))
            return false;
    return !path[n] || path_char((unsigned char)path[n]) == '/' || (n == 1 && parent[0] == '/');
}
bool fs_name_valid(const char *s, char *error, size_t cap) {
    if (!s || !*s || !strcmp(s, ".") || !strcmp(s, ".."))
        return fail(error, cap, "Saisissez un nom de fichier ou de dossier.");
    size_t n = strlen(s);
    if (n > 240)
        return fail(error, cap, "Nom trop long (240 octets maximum).");
    if (s[n - 1] == '.' || s[n - 1] == ' ')
        return fail(error, cap, "Le nom ne doit pas finir par un point ou un espace.");
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)s[i] < 32 || (unsigned char)s[i] == 127 || strchr("/\\:*?\"<>|", s[i]))
            return fail(error, cap, "Caractère interdit dans le nom : / \\ : * ? \" < > |");
#ifdef _WIN32
    char base[16] = {0};
    size_t i = 0;
    while (s[i] && s[i] != '.' && i < sizeof(base) - 1) {
        base[i] = (char)toupper((unsigned char)s[i]);
        i++;
    }
    if (!strcmp(base, "CON") || !strcmp(base, "PRN") || !strcmp(base, "AUX") || !strcmp(base, "NUL") ||
        ((strstr(base, "COM") == base || strstr(base, "LPT") == base) && strlen(base) == 4 &&
         base[3] >= '1' && base[3] <= '9'))
        return fail(error, cap, "Ce nom est réservé par Windows.");
#endif
    return true;
}
static bool split(const char *path, char *parent, char *name) {
    if (!absolute(path) || strlen(path) >= FS_PATH)
        return false;
    char temp[FS_PATH];
    strcpy(temp, path);
    size_t n = strlen(temp);
    while (n > 1 && separator((unsigned char)temp[n - 1]))
        temp[--n] = 0;
    char *slash = strrchr(temp, '/');
#ifdef _WIN32
    char *back = strrchr(temp, '\\');
    if (back && (!slash || back > slash))
        slash = back;
#endif
    if (!slash || !slash[1] || !strcmp(slash + 1, ".") || !strcmp(slash + 1, ".."))
        return false;
    strcpy(name, slash + 1);
    if (slash == temp)
        slash[1] = 0;
    else
        *slash = 0;
#ifdef _WIN32
    if (strlen(temp) == 2 && temp[1] == ':')
        strcat(temp, "/");
#endif
    strcpy(parent, temp);
    return true;
}
bool fs_parent(const char *path, char *out) {
    if (!path || !out)
        return false;
    if (!strcmp(path, "/")) {
        strcpy(out, "/");
        return true;
    }
#ifdef _WIN32
    if (isalpha((unsigned char)path[0]) && path[1] == ':' && separator((unsigned char)path[2]) && !path[3]) {
        strcpy(out, path);
        return true;
    }
#endif
    char name[FS_PATH];
    return split(path, out, name);
}
#ifdef _WIN32
static bool wide(const char *s, wchar_t *out) {
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, out, FS_PATH) > 0;
}
static bool canonical(const char *path, char *out) {
    wchar_t w[FS_PATH], full[FS_PATH];
    if (!wide(path, w))
        return false;
    HANDLE h = CreateFileW(w, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    DWORD n = GetFinalPathNameByHandleW(h, full, FS_PATH, FILE_NAME_NORMALIZED);
    CloseHandle(h);
    if (!n || n >= FS_PATH)
        return false;
    const wchar_t *p = full;
    if (!wcsncmp(p, L"\\\\?\\", 4))
        p += 4;
    if (!WideCharToMultiByte(CP_UTF8, 0, p, -1, out, FS_PATH, NULL, NULL))
        return false;
    for (char *q = out; *q; q++)
        if (*q == '\\')
            *q = '/';
    return true;
}
FsKind fs_kind(const char *path) {
    wchar_t w[FS_PATH];
    if (!path || !wide(path, w))
        return FS_MISSING;
    DWORD a = GetFileAttributesW(w);
    if (a == INVALID_FILE_ATTRIBUTES)
        return FS_MISSING;
    if (a & FILE_ATTRIBUTE_REPARSE_POINT)
        return FS_LINK;
    return a & FILE_ATTRIBUTE_DIRECTORY ? FS_DIRECTORY : FS_FILE;
}
#else
static bool canonical(const char *path, char *out) {
    return path && strlen(path) < FS_PATH && realpath(path, out) != NULL;
}
FsKind fs_kind(const char *path) {
    struct stat s;
    if (!path || lstat(path, &s))
        return FS_MISSING;
    return S_ISLNK(s.st_mode)   ? FS_LINK
           : S_ISDIR(s.st_mode) ? FS_DIRECTORY
           : S_ISREG(s.st_mode) ? FS_FILE
                                : FS_OTHER;
}
#endif
static bool safe_parent(const char *root, const char *parent, char *resolved, char *error, size_t cap) {
    char base[FS_PATH];
    if (!guarded(root, false, error, cap) || !guarded(parent, false, error, cap))
        return false;
    if (!canonical(root, base) || !canonical(parent, resolved))
        return fail(error, cap, "Dossier introuvable ou inaccessible.");
    if (!fs_under(base, resolved))
        return fail(error, cap,
                    "Cette opération doit rester dans le dossier du projet (liens externes exclus).");
    return true;
}
static bool tick(FsProgress progress, void *user, unsigned long long bytes, bool entry, char *error,
                 size_t cap) {
    return !progress || progress(user, bytes, entry) || fail(error, cap, "Copie annulée.");
}

#ifndef _WIN32
/* All recursion is descriptor-relative and never follows directory symlinks. */
static bool remove_at(int parent, const char *name, bool rollback, int depth, FsProgress progress, void *user,
                      char *error, size_t cap) {
    if (depth > 128)
        return fail(error, cap, "Arborescence trop profonde (128 niveaux).");
    if (!guarded_fd(parent, name, true, error, cap))
        return false;
    struct stat s;
    if (fstatat(parent, name, &s, AT_SYMLINK_NOFOLLOW))
        return fail(error, cap, "Impossible de lire %s : %s", name, strerror(errno));
    if (S_ISDIR(s.st_mode)) {
        int fd = openat(parent, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0)
            return fail(error, cap, "Dossier inaccessible : %s", name);
        if (rollback)
            (void)fchmod(fd, 0700);
        DIR *dir = fdopendir(fd);
        if (!dir) {
            close(fd);
            return fail(error, cap, "Lecture du dossier impossible : %s", name);
        }
        bool ok = true;
        struct dirent *e;
        errno = 0;
        while ((e = readdir(dir))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
                continue;
            if (!remove_at(fd, e->d_name, rollback, depth + 1, progress, user, error, cap)) {
                ok = false;
                break;
            }
            errno = 0;
        }
        if (ok && errno)
            ok = fail(error, cap, "Lecture du dossier interrompue : %s", name);
        closedir(dir);
        if (!ok)
            return false;
        if (!guarded_fd(parent, name, true, error, cap))
            return false;
        if (unlinkat(parent, name, AT_REMOVEDIR))
            return fail(error, cap, "Suppression du dossier impossible : %s", name);
    } else if (!guarded_fd(parent, name, true, error, cap))
        return false;
    else if (unlinkat(parent, name, 0))
        return fail(error, cap, "Suppression impossible : %s", name);
    if (progress)
        (void)progress(user, 0, true);
    return true;
}
static bool copy_at(int source, const char *name, int destination, const char *target, bool *owned, int depth,
                    FsProgress progress, void *user, char *error, size_t cap) {
    if (depth > 128)
        return fail(error, cap, "Arborescence trop profonde (128 niveaux).");
    if (!tick(progress, user, 0, false, error, cap))
        return false;
    if (!guarded_fd(source, name, false, error, cap) || !guarded_fd(destination, NULL, false, error, cap))
        return false;
    struct stat s;
    if (fstatat(source, name, &s, AT_SYMLINK_NOFOLLOW))
        return fail(error, cap, "Source introuvable : %s", name);
    if (S_ISLNK(s.st_mode)) {
        char link[FS_PATH];
        ssize_t n = readlinkat(source, name, link, sizeof(link) - 1);
        if (n < 0 || (size_t)n >= sizeof(link) - 1)
            return fail(error, cap, "Lien symbolique illisible : %s", name);
        link[n] = 0;
        if (symlinkat(link, destination, target))
            return fail(error, cap, "Copie du lien impossible : %s", target);
        if (owned)
            *owned = true;
    } else if (S_ISDIR(s.st_mode)) {
        int src = openat(source, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (src < 0)
            return fail(error, cap, "Dossier source inaccessible : %s", name);
        if (mkdirat(destination, target, 0700)) {
            close(src);
            return fail(error, cap, "Création du dossier impossible : %s", target);
        }
        if (owned)
            *owned = true;
        int dst = openat(destination, target, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (dst < 0) {
            close(src);
            return fail(error, cap, "Dossier de destination inaccessible.");
        }
        DIR *dir = fdopendir(src);
        if (!dir) {
            close(src);
            close(dst);
            return fail(error, cap, "Lecture du dossier source impossible.");
        }
        bool ok = true;
        struct dirent *e;
        errno = 0;
        while ((e = readdir(dir))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
                continue;
            if (!copy_at(src, e->d_name, dst, e->d_name, NULL, depth + 1, progress, user, error, cap)) {
                ok = false;
                break;
            }
            errno = 0;
        }
        if (ok && errno)
            ok = fail(error, cap, "Lecture du dossier source interrompue.");
        if (ok)
            (void)fchmod(dst, s.st_mode & 0777);
        closedir(dir);
        close(dst);
        if (!ok)
            return false;
    } else if (S_ISREG(s.st_mode)) {
        int src = openat(source, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
        if (src < 0)
            return fail(error, cap, "Fichier source inaccessible : %s", name);
        struct stat actual;
        if (fstat(src, &actual) || !S_ISREG(actual.st_mode)) {
            close(src);
            return fail(error, cap, "Le type du fichier source a changé.");
        }
        /* Stage + exclusive hard-link publication. No overwrite fallback if
           SFTP does not implement hard links; a racing destination stays intact. */
        static atomic_uint serial;
        char stage[96];
        snprintf(stage, sizeof(stage), ".prisme-copy-%ld-%u.tmp", (long)getpid(),
                 atomic_fetch_add(&serial, 1));
        int dst = openat(destination, stage, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (dst < 0) {
            close(src);
            return fail(error, cap, "Création temporaire impossible : %s", target);
        }
        char *data = malloc(65536);
        bool ok = data != NULL && guarded_fd(src, NULL, false, error, cap) &&
                  guarded_fd(dst, NULL, false, error, cap);
        if (!ok && !*error)
            fail(error, cap, "Mémoire insuffisante ou montage indisponible.");
        while (ok) {
            if (!guarded_fd(src, NULL, false, error, cap) || !guarded_fd(dst, NULL, false, error, cap)) {
                ok = false;
                break;
            }
            ssize_t n = read(src, data, 65536);
            if (n < 0 && errno == EINTR)
                continue;
            if (n < 0) {
                ok = fail(error, cap, "Lecture du fichier impossible : %s", name);
                break;
            }
            if (!n)
                break;
            for (ssize_t p = 0; p < n;) {
                ssize_t w = write(dst, data + p, (size_t)(n - p));
                if (w < 0 && errno == EINTR)
                    continue;
                if (w <= 0) {
                    ok = fail(error, cap, "Écriture impossible (espace disque ?) : %s", target);
                    break;
                }
                p += w;
            }
            if (!ok || !tick(progress, user, (unsigned long long)n, false, error, cap)) {
                ok = false;
                break;
            }
        }
        if (ok)
            (void)fchmod(dst, actual.st_mode & 0777);
        if (ok && fsync(dst))
            ok = fail(error, cap, "Synchronisation du fichier impossible : %s", target);
        if (close(dst) && ok)
            ok = fail(error, cap, "Finalisation du fichier impossible : %s", target);
        if (ok)
            ok = guarded_fd(destination, NULL, false, error, cap) &&
                 guarded_fd(source, name, false, error, cap);
        if (ok && linkat(destination, stage, destination, target, 0))
            ok = fail(
                error, cap,
                "Publication exclusive impossible (cible existante ou liens SFTP non pris en charge) : %s",
                target);
        else if (ok && owned)
            *owned = true;
        if (guarded_fd(destination, NULL, false, error, cap))
            (void)unlinkat(destination, stage, 0);
        close(src);
        free(data);
        if (!ok)
            return false;
    } else
        return fail(error, cap, "Type de fichier non pris en charge : %s", name);
    return tick(progress, user, 0, true, error, cap);
}
#endif

bool fs_create(const char *root, const char *parent, const char *name, bool directory, char *created,
               char *error, size_t cap) {
    char resolved[FS_PATH];
    if (created)
        created[0] = 0;
    if (!fs_name_valid(name, error, cap) || !safe_parent(root, parent, resolved, error, cap))
        return false;
    char target[FS_PATH];
    if (!join(target, resolved, name))
        return fail(error, cap, "Chemin trop long.");
#ifdef _WIN32
    wchar_t w[FS_PATH];
    if (!wide(target, w))
        return fail(error, cap, "Chemin UTF-8 invalide.");
    if (directory) {
        if (!CreateDirectoryW(w, NULL))
            return fail(error, cap, "Ce dossier existe déjà ou ne peut pas être créé.");
    } else {
        HANDLE h = CreateFileW(w, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE)
            return fail(error, cap, "Ce fichier existe déjà ou ne peut pas être créé.");
        CloseHandle(h);
    }
#else
    int fd = open(resolved, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0)
        return fail(error, cap, "Dossier inaccessible.");
    if (!guarded_fd(fd, NULL, false, error, cap)) {
        close(fd);
        return false;
    }
    int result = directory ? mkdirat(fd, name, 0777) : -1;
    if (!directory) {
        int file = openat(fd, name, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0666);
        result = file < 0 ? -1 : close(file);
    }
    int saved = errno;
    close(fd);
    if (result)
        return fail(error, cap, "Création impossible : %s", strerror(saved));
#endif
    if (created)
        strcpy(created, target);
    return true;
}

#ifdef _WIN32
/* Reparse directories are removed as links, never traversed. Copying Windows
   reparse points is rejected rather than silently copying their external target. */
static bool win_remove(const char *path, int depth, FsProgress progress, void *user, char *error,
                       size_t cap) {
    if (depth > 128)
        return fail(error, cap, "Arborescence trop profonde.");
    wchar_t w[FS_PATH];
    if (!wide(path, w))
        return fail(error, cap, "Chemin invalide.");
    DWORD attrs = GetFileAttributesW(w);
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return fail(error, cap, "Chemin introuvable.");
    if ((attrs & FILE_ATTRIBUTE_DIRECTORY) && !(attrs & FILE_ATTRIBUTE_REPARSE_POINT)) {
        char pattern[FS_PATH];
        wchar_t search[FS_PATH];
        if (!join(pattern, path, "*") || !wide(pattern, search))
            return fail(error, cap, "Chemin trop long.");
        WIN32_FIND_DATAW data;
        HANDLE h = FindFirstFileW(search, &data);
        bool ok = true;
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!wcscmp(data.cFileName, L".") || !wcscmp(data.cFileName, L".."))
                    continue;
                char leaf[FS_PATH], child[FS_PATH];
                if (!WideCharToMultiByte(CP_UTF8, 0, data.cFileName, -1, leaf, FS_PATH, NULL, NULL) ||
                    !join(child, path, leaf) || !win_remove(child, depth + 1, progress, user, error, cap)) {
                    ok = false;
                    break;
                }
            } while (FindNextFileW(h, &data));
            if (ok && GetLastError() != ERROR_NO_MORE_FILES)
                ok = fail(error, cap, "Lecture du dossier interrompue.");
            FindClose(h);
        } else if (GetLastError() != ERROR_FILE_NOT_FOUND)
            ok = fail(error, cap, "Dossier inaccessible.");
        if (!ok)
            return false;
    }
    if (attrs & FILE_ATTRIBUTE_READONLY)
        SetFileAttributesW(w, attrs & ~FILE_ATTRIBUTE_READONLY);
    BOOL ok = attrs & FILE_ATTRIBUTE_DIRECTORY ? RemoveDirectoryW(w) : DeleteFileW(w);
    if (!ok)
        return fail(error, cap, "Suppression impossible (fichier utilisé ou accès refusé).");
    if (progress)
        (void)progress(user, 0, true);
    return true;
}
static bool win_copy(const char *source, const char *target, bool *owned, int depth, FsProgress progress,
                     void *user, char *error, size_t cap) {
    if (depth > 128)
        return fail(error, cap, "Arborescence trop profonde (128 niveaux).");
    if (!tick(progress, user, 0, false, error, cap))
        return false;
    wchar_t src[FS_PATH], dst[FS_PATH];
    if (!wide(source, src) || !wide(target, dst))
        return fail(error, cap, "Chemin invalide.");
    FsKind kind = fs_kind(source);
    if (kind == FS_LINK)
        return fail(error, cap, "La copie des liens Windows n’est pas prise en charge.");
    if (kind == FS_DIRECTORY) {
        if (!CreateDirectoryW(dst, NULL))
            return fail(error, cap, "Création du dossier impossible.");
        if (owned)
            *owned = true;
        char pattern[FS_PATH];
        wchar_t search[FS_PATH];
        if (!join(pattern, source, "*") || !wide(pattern, search))
            return fail(error, cap, "Chemin trop long.");
        WIN32_FIND_DATAW data;
        HANDLE h = FindFirstFileW(search, &data);
        bool ok = true;
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!wcscmp(data.cFileName, L".") || !wcscmp(data.cFileName, L".."))
                    continue;
                char leaf[FS_PATH], s[FS_PATH], d[FS_PATH];
                if (!WideCharToMultiByte(CP_UTF8, 0, data.cFileName, -1, leaf, FS_PATH, NULL, NULL) ||
                    !join(s, source, leaf) || !join(d, target, leaf) ||
                    !win_copy(s, d, NULL, depth + 1, progress, user, error, cap)) {
                    ok = false;
                    break;
                }
            } while (FindNextFileW(h, &data));
            if (ok && GetLastError() != ERROR_NO_MORE_FILES)
                ok = fail(error, cap, "Lecture du dossier interrompue.");
            FindClose(h);
        } else if (GetLastError() != ERROR_FILE_NOT_FOUND)
            ok = fail(error, cap, "Dossier inaccessible.");
        if (!ok)
            return false;
    } else if (kind == FS_FILE) {
        HANDLE in =
            CreateFileW(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (in == INVALID_HANDLE_VALUE)
            return fail(error, cap, "Lecture du fichier impossible.");
        HANDLE out = CreateFileW(dst, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (out == INVALID_HANDLE_VALUE) {
            CloseHandle(in);
            return fail(error, cap, "Destination existante ou inaccessible.");
        }
        if (owned)
            *owned = true;
        char *data = malloc(65536);
        DWORD n, w;
        bool ok = data != NULL;
        while (ok) {
            if (!ReadFile(in, data, 65536, &n, NULL)) {
                ok = false;
                break;
            }
            if (!n)
                break;
            if (!WriteFile(out, data, n, &w, NULL) || w != n || !tick(progress, user, n, false, error, cap)) {
                ok = false;
                break;
            }
        }
        CloseHandle(in);
        CloseHandle(out);
        free(data);
        if (!ok)
            return fail(error, cap, "Copie interrompue ou annulée.");
    } else
        return fail(error, cap, "Source introuvable ou non prise en charge.");
    return tick(progress, user, 0, true, error, cap);
}
#endif
bool fs_remove(const char *root, const char *path, FsProgress progress, void *user, char *error, size_t cap) {
    char parent[FS_PATH], name[FS_PATH], resolved[FS_PATH];
    if (!guarded(path, true, error, cap))
        return false;
    if (!split(path, parent, name) || !safe_parent(root, parent, resolved, error, cap))
        return fail(error, cap, "Suppression hors projet ou de la racine interdite.");
#ifdef _WIN32
    char target[FS_PATH];
    if (!join(target, resolved, name))
        return fail(error, cap, "Chemin trop long.");
    return win_remove(target, 0, progress, user, error, cap);
#else
    int fd = open(resolved, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0)
        return fail(error, cap, "Dossier inaccessible.");
    bool ok = remove_at(fd, name, false, 0, progress, user, error, cap);
    close(fd);
    return ok;
#endif
}
static bool destination_name(const char *parent, const char *name, char *target, char *leaf, char *error,
                             size_t cap) {
    if (strlen(name) > 240)
        return fail(error, cap, "Nom source trop long.");
    char stem[256], extension[256];
    strcpy(stem, name);
    extension[0] = 0;
    const char *dot = strrchr(name, '.');
    if (dot && dot != name) {
        size_t n = (size_t)(dot - name);
        memcpy(stem, name, n);
        stem[n] = 0;
        strcpy(extension, dot);
    }
    for (int i = 0; i < 10000; i++) {
        int n = i == 0   ? snprintf(leaf, FS_PATH, "%s", name)
                : i == 1 ? snprintf(leaf, FS_PATH, "%s (copie)%s", stem, extension)
                         : snprintf(leaf, FS_PATH, "%s (copie %d)%s", stem, i, extension);
        if (n < 0 || n > 255 || !join(target, parent, leaf))
            return fail(error, cap, "Nom de copie ou chemin trop long.");
        if (fs_kind(target) == FS_MISSING)
            return true;
    }
    return fail(error, cap, "Trop de copies portant ce nom.");
}
bool fs_paste(const char *root, const char *parent, const FsPaths *sources, FsProgress progress, void *user,
              char *first_created, char *error, size_t cap) {
    if (first_created)
        first_created[0] = 0;
    if (!sources || sources->count < 1 || sources->count > FS_MAX_PATHS)
        return fail(error, cap, "Aucun fichier ou dossier copié.");
    char resolved[FS_PATH];
    if (!safe_parent(root, parent, resolved, error, cap))
        return false;
    FsPaths *created = calloc(1, sizeof(*created));
    if (!created)
        return fail(error, cap, "Mémoire insuffisante.");
#ifndef _WIN32
    int dst = open(resolved, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (dst < 0) {
        free(created);
        return fail(error, cap, "Destination inaccessible.");
    }
#endif
    bool ok = true;
    for (int i = 0; i < sources->count; i++) {
        char src_parent[FS_PATH], src_name[FS_PATH], actual[FS_PATH], target[FS_PATH], leaf[FS_PATH];
        const char *source = sources->paths[i];
        if (!guarded(source, false, error, cap)) {
            ok = false;
            break;
        }
        FsKind kind = fs_kind(source);
        if (!split(source, src_parent, src_name) || kind == FS_MISSING || kind == FS_OTHER) {
            ok = fail(error, cap, "Source introuvable ou non prise en charge.");
            break;
        }
        if (kind == FS_DIRECTORY && (!canonical(source, actual) || fs_under(actual, resolved))) {
            ok = fail(error, cap, "Un dossier ne peut pas être collé dans lui-même ou dans un sous-dossier.");
            break;
        }
        if (!destination_name(resolved, src_name, target, leaf, error, cap)) {
            ok = false;
            break;
        }
        /* Track ownership only AFTER an exclusive create succeeded. A racing
           EEXIST must never make rollback delete somebody else's entry. */
        bool owned = false;
#ifdef _WIN32
        ok = win_copy(source, target, &owned, 0, progress, user, error, cap);
#else
        char source_dir[FS_PATH];
        if (!canonical(src_parent, source_dir)) {
            ok = fail(error, cap, "Dossier source inaccessible.");
            break;
        }
        int src = open(source_dir, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (src < 0) {
            ok = fail(error, cap, "Dossier source inaccessible.");
            break;
        }
        ok = copy_at(src, src_name, dst, leaf, &owned, 0, progress, user, error, cap);
        close(src);
#endif
        if (owned)
            strcpy(created->paths[created->count++], target);
        if (!ok)
            break;
    }
    bool rollback_ok = true;
    if (!ok)
        for (int i = created->count - 1; i >= 0; i--) {
            if (fs_kind(created->paths[i]) == FS_MISSING)
                continue;
            char ignored[512];
#ifdef _WIN32
            if (!win_remove(created->paths[i], 0, NULL, NULL, ignored, sizeof(ignored)))
                rollback_ok = false;
#else
            char p[FS_PATH], n[FS_PATH];
            if (split(created->paths[i], p, n) &&
                !remove_at(dst, n, true, 0, NULL, NULL, ignored, sizeof(ignored)))
                rollback_ok = false;
#endif
        }
    if (!rollback_ok && cap) {
        size_t used = strlen(error);
        if (used < cap)
            snprintf(error + used, cap - used, " Une copie partielle peut subsister.");
    }
    if (ok && first_created)
        strcpy(first_created, created->paths[0]);
#ifndef _WIN32
    close(dst);
#endif
    free(created);
    return ok;
}
static int hex_digit(unsigned char c) {
    return c >= '0' && c <= '9'   ? c - '0'
           : c >= 'a' && c <= 'f' ? c - 'a' + 10
           : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                  : -1;
}
int fs_paths_parse(FsPaths *paths, const char *text) {
    paths->count = 0;
    if (!text)
        return 0;
    if (strlen(text) > FS_CLIP_LIMIT)
        return -1;
    const char *s = text;
    while (*s) {
        const char *end = strchr(s, '\n');
        size_t n = end ? (size_t)(end - s) : strlen(s);
        while (n && s[n - 1] == '\r')
            n--;
        char path[FS_PATH];
        size_t p = 0;
        bool valid = n > 0 && s[0] != '#';
        const char *start = s;
        size_t length = n;
        bool uri = n >= 7 && !strncmp(s, "file://", 7);
        if (uri) {
            start = s + 7;
            length = n - 7;
            if (length >= 10 && !strncmp(start, "localhost/", 10)) {
                start += 9;
                length -= 9;
            }
            if (!length || *start != '/')
                valid = false;
        }
        if (valid)
            for (size_t i = 0; i < length; i++) {
                unsigned char c = (unsigned char)start[i];
                if (uri && c == '%') {
                    if (i + 2 >= length || hex_digit((unsigned char)start[i + 1]) < 0 ||
                        hex_digit((unsigned char)start[i + 2]) < 0) {
                        valid = false;
                        break;
                    }
                    c = (unsigned char)(hex_digit((unsigned char)start[i + 1]) * 16 +
                                        hex_digit((unsigned char)start[i + 2]));
                    i += 2;
                }
                if (!c || p + 1 >= sizeof(path)) {
                    valid = false;
                    break;
                }
                path[p++] = (char)c;
            }
        path[p] = 0;
#ifdef _WIN32
        if (uri && path[0] == '/' && isalpha((unsigned char)path[1]) && path[2] == ':')
            memmove(path, path + 1, strlen(path));
#endif
        if (valid && absolute(path)) {
            bool duplicate = false;
            for (int i = 0; i < paths->count; i++)
                if (!strcmp(paths->paths[i], path))
                    duplicate = true;
            if (!duplicate) {
                if (paths->count == FS_MAX_PATHS) {
                    paths->count = 0;
                    return -1;
                }
                strcpy(paths->paths[paths->count++], path);
            }
        }
        if (!end)
            break;
        s = end + 1;
    }
    return paths->count;
}
bool fs_paths_text(const FsPaths *paths, char *out, size_t cap) {
    if (!cap)
        return false;
    out[0] = 0;
    if (!paths || paths->count < 1 || paths->count > FS_MAX_PATHS)
        return false;
    size_t n = 0;
    const char *digits = "0123456789ABCDEF";
    for (int i = 0; i < paths->count; i++) {
        const unsigned char *p = (const unsigned char *)paths->paths[i];
        if (!absolute((const char *)p))
            return false;
        const char *prefix = "file://";
#ifdef _WIN32
        if (p[0] != '\\')
            prefix = "file:///";
#endif
        size_t k = strlen(prefix);
        if (n + k >= cap)
            return false;
        memcpy(out + n, prefix, k);
        n += k;
        for (; *p; p++) {
            unsigned char c = *p;
#ifdef _WIN32
            if (c == '\\')
                c = '/';
#endif
            bool plain = isalnum(c) && c < 128;
            plain = plain || c == '/' || c == '-' || c == '_' || c == '.' || c == '~' || c == ':';
            size_t bytes = plain ? 1u : 3u;
            if (n + bytes + 2 >= cap)
                return false;
            if (plain)
                out[n++] = (char)c;
            else {
                out[n++] = '%';
                out[n++] = digits[c >> 4];
                out[n++] = digits[c & 15];
            }
        }
        out[n++] = '\n';
        out[n] = 0;
    }
    return true;
}
