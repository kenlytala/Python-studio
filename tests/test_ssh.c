#define _GNU_SOURCE
#include "ssh.h"
#include "ssh_json.h"
#include "buffer.h"
#include "files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int checks;
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        checks++;                                                                                            \
        if (!(x)) {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                                  \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static void set(char *out, size_t cap, const char *s) {
    CHECK(strlen(s) < cap);
    strcpy(out, s);
}
static int profiles_cli(int argc, char **argv) {
    const char *path = argv[2];
    char error[1024] = "";
    bool ok = true;
    if (argc >= 4 && !strcmp(argv[3], "put") && argc == 10) {
        SshProfile p;
        ssh_profile_init(&p);
        set(p.host, sizeof(p.host), argv[4]);
        set(p.user, sizeof(p.user), argv[5]);
        set(p.port, sizeof(p.port), argv[6]);
        set(p.folder, sizeof(p.folder), argv[7]);
        set(p.identity, sizeof(p.identity), argv[8]);
        set(p.id, sizeof(p.id), argv[9]);
        ok = ssh_profiles_put(path, &p, error, sizeof(error));
    } else if (argc == 5 && !strcmp(argv[3], "select"))
        ok = ssh_profiles_select(path, argv[4], error, sizeof(error));
    else if (argc == 5 && !strcmp(argv[3], "delete"))
        ok = ssh_profiles_delete(path, argv[4], error, sizeof(error));
    else if (argc != 3)
        return 2;
    SshProfiles *list = calloc(1, sizeof(*list));
    CHECK(list);
    if (ok)
        ok = ssh_profiles_load(path, list, error, sizeof(error));
    free(list);
    if (!ok) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        puts("{\"schema\":1,\"last\":\"\",\"profiles\":[]}");
        return 0;
    }
    char data[8192];
    size_t n;
    while ((n = fread(data, 1, sizeof(data), file)))
        CHECK(fwrite(data, 1, n, stdout) == n);
    fclose(file);
    return 0;
}
int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "--profiles"))
        return profiles_cli(argc, argv);
    char root[] = "/tmp/prisme-c-ssh-unit-XXXXXX";
    CHECK(mkdtemp(root));
    char file[SSH_PATH], error[1024] = "", keyfile[SSH_PATH];
    CHECK(snprintf(file, sizeof(file), "%s/ssh-profiles.json", root) < SSH_PATH);
    CHECK(snprintf(keyfile, sizeof(keyfile), "%s/key with spaces", root) < SSH_PATH);
    FILE *key = fopen(keyfile, "w");
    CHECK(key);
    fputs("not a secret, QA key path only", key);
    fclose(key);
    SshProfile p;
    ssh_profile_init(&p);
    strcpy(p.host, "server");
    strcpy(p.user, "odoo");
    strcpy(p.port, "2222");
    strcpy(p.folder, "/opt/projet é");
    CHECK(ssh_profile_valid(&p, true, error, sizeof(error)));
    SshCommand command;
    CHECK(ssh_command(&command, &p, "/tmp/mount", "/usr/bin/sshfs", "/usr/bin/ssh", "/tmp/config", error,
                      sizeof(error)));
    CHECK(strstr(command.options, " -l odoo"));
    CHECK(strstr(command.options, " -p 2222"));
    CHECK(strstr(command.options, "StrictHostKeyChecking=ask"));
    CHECK(strstr(command.options, "PermitLocalCommand=no"));
    CHECK(strstr(command.options, "ControlPath=none"));
    CHECK(strstr(command.options, " -F /tmp/config"));
    SshProfile defaults;
    ssh_profile_init(&defaults);
    strcpy(defaults.host, "ssh-alias");
    SshCommand inherited;
    CHECK(ssh_command(&inherited, &defaults, "/tmp/mount", "sshfs", "ssh", NULL, error, sizeof(error)));
    CHECK(!strstr(inherited.options, " -l "));
    CHECK(!strstr(inherited.options, " -p "));
    bool contain = false;
    for (int i = 0; i < command.count; i++) {
        CHECK(!strstr(command.argv[i], "User="));
        CHECK(!strstr(command.argv[i], "Port="));
        if (!strcmp(command.argv[i], "contain_symlinks"))
            contain = true;
    }
    CHECK(contain);
    CHECK(!strstr(command.options, "follow_symlinks"));
    strcpy(p.host, "::1");
    CHECK(ssh_command(&command, &p, "/tmp/mount", "sshfs", "ssh", NULL, error, sizeof(error)));
    CHECK(!strcmp(command.source, "[::1]:/opt/projet é"));
    strcpy(p.host, "server");
    strcpy(p.identity, keyfile);
    CHECK(ssh_command(&command, &p, "/tmp/mount", "sshfs", "ssh", NULL, error, sizeof(error)));
    CHECK(strstr(command.identity_option, "IdentityFile=\""));
    CHECK(strstr(command.identity_option, "key with spaces\""));
    strcpy(p.host, "-oBad");
    CHECK(!ssh_profile_valid(&p, false, error, sizeof(error)));
    strcpy(p.host, "server");
    strcpy(p.port, "65536");
    CHECK(!ssh_profile_valid(&p, false, error, sizeof(error)));
    strcpy(p.port, "2222");
    strcpy(p.folder, "relative");
    CHECK(!ssh_profile_valid(&p, false, error, sizeof(error)));
    strcpy(p.folder, "/opt/projet é\"\\code");
    strcpy(p.identity, "/missing/key");
    CHECK(ssh_profile_valid(&p, false, error, sizeof(error)));
    CHECK(!ssh_profile_valid(&p, true, error, sizeof(error)));
    mode_t previous = umask(0);
    CHECK(ssh_profiles_put(file, &p, error, sizeof(error)));
    umask(previous);
    CHECK(strlen(p.id) == 32);
    SshProfiles *list = calloc(1, sizeof(*list));
    CHECK(list);
    CHECK(ssh_profiles_load(file, list, error, sizeof(error)));
    CHECK(list->count == 1);
    CHECK(ssh_profile_equal(&p, &list->items[0]));
    struct stat st;
    CHECK(!stat(file, &st));
    CHECK((st.st_mode & 0777) == 0600);
    char first[33];
    strcpy(first, p.id);
    strcpy(p.folder, "/new");
    CHECK(ssh_profiles_put(file, &p, error, sizeof(error)));
    CHECK(!strcmp(first, p.id));
    p.id[0] = 0;
    strcpy(p.host, "second");
    CHECK(ssh_profiles_put(file, &p, error, sizeof(error)));
    CHECK(ssh_profiles_load(file, list, error, sizeof(error)));
    CHECK(list->count == 2);
    CHECK(ssh_profiles_select(file, first, error, sizeof(error)));
    CHECK(ssh_profiles_load(file, list, error, sizeof(error)));
    CHECK(!strcmp(list->last, first));
    CHECK(ssh_profiles_delete(file, first, error, sizeof(error)));
    CHECK(ssh_profiles_load(file, list, error, sizeof(error)));
    CHECK(list->count == 1 && !strcmp(list->last, p.id));
    CHECK(!ssh_profiles_delete(file, first, error, sizeof(error)));
    for (int i = 1; i < SSH_PROFILES_MAX; i++) {
        p.id[0] = 0;
        snprintf(p.host, sizeof(p.host), "server-%d", i);
        CHECK(ssh_profiles_put(file, &p, error, sizeof(error)));
    }
    p.id[0] = 0;
    strcpy(p.host, "overflow");
    CHECK(!ssh_profiles_put(file, &p, error, sizeof(error)));
    CHECK(ssh_profiles_load(file, list, error, sizeof(error)));
    CHECK(list->count == SSH_PROFILES_MAX);
    FILE *bad = fopen(file, "w");
    CHECK(bad);
    fputs("{\"schema\":1,\"schema\":1,\"profiles\":[]}", bad);
    fclose(bad);
    CHECK(!ssh_profiles_load(file, list, error, sizeof(error)));
    CHECK(!ssh_profiles_put(file, &p, error, sizeof(error)));
    bad = fopen(file, "r");
    CHECK(bad);
    char text[128];
    CHECK(fgets(text, sizeof(text), bad));
    fclose(bad);
    CHECK(strstr(text, "\"schema\":1,\"schema\":1"));
    CHECK(!unlink(file));
    char legacy[SSH_PATH];
    snprintf(legacy, sizeof(legacy), "%s/ssh.json", root);
    bad = fopen(legacy, "w");
    CHECK(bad);
    fputs("{\"host\":\"legacy\",\"folder\":\"/opt/é\",\"password\":\"must-not-copy\"}", bad);
    fclose(bad);
    CHECK(ssh_profiles_load(file, list, error, sizeof(error)));
    CHECK(list->count == 1 && !strcmp(list->items[0].host, "legacy"));
    CHECK(ssh_profiles_delete(file, list->last, error, sizeof(error)));
    CHECK(ssh_profiles_load(file, list, error, sizeof(error)));
    CHECK(!list->count);
    SshSession *session = calloc(1, sizeof(*session));
    CHECK(session);
    ssh_init(session, root, "/missing/helper");
    CHECK(ssh_mount_id("/", false) > 0);
    CHECK(ssh_mount_id("/", true) == -2);
    session->state = SSH_MOUNTED;
    session->generation = 1;
    session->process_alive = true;
    snprintf(session->mountpoint, sizeof(session->mountpoint), "%s/current", root);
    char remote[SSH_PATH];
    snprintf(remote, sizeof(remote), "%s/current/draft.py", root);
    CHECK(ssh_owns(session, remote));
    CHECK(!ssh_guard(session, remote, false, error, sizeof(error)));
    Buffer buffer;
    CHECK(buf_init(&buffer, "old", "draft.py"));
    CHECK(buf_insert(&buffer, "new"));
    CHECK(buf_dirty(&buffer));
    CHECK(!buf_save_guarded(&buffer, remote, ssh_save_guard, session, error, sizeof(error)));
    CHECK(buf_dirty(&buffer));
    CHECK(access(remote, F_OK));
    char alias[SSH_PATH], through_alias[SSH_PATH];
    snprintf(alias, sizeof(alias), "/tmp/prisme-c-ssh-alias-%ld", (long)getpid());
    CHECK(!symlink(root, alias));
    CHECK(snprintf(through_alias, sizeof(through_alias), "%s/current/draft.py", alias) < SSH_PATH);
    CHECK(!ssh_guard(session, through_alias, false, error, sizeof(error)));
    CHECK(!buf_save_guarded(&buffer, through_alias, ssh_save_guard, session, error, sizeof(error)));
    CHECK(buf_dirty(&buffer));
    CHECK(!unlink(alias));

    char local[SSH_PATH];
    snprintf(local, sizeof(local), "/tmp/prisme-ssh-salvage-%ld.py", (long)getpid());
    CHECK(buf_save_guarded(&buffer, local, ssh_save_guard, session, error, sizeof(error)));
    CHECK(!buf_dirty(&buffer));
    CHECK(!unlink(local));
    buf_free(&buffer);
    /* A failed second connection must never unmount the first one's mount. */
    strcpy(session->mountpoint, "/");
    session->generation = 0;
    session->process_alive = false;
    session->state = SSH_ERROR;
    long before = ssh_mount_id("/", false);
    CHECK(ssh_close(session, error, sizeof(error)));
    CHECK(ssh_mount_id("/", false) == before);
    free(session);
    free(list);
    char removed[512] = "";
    CHECK(fs_remove("/tmp", root, NULL, NULL, removed, sizeof(removed)));
    printf("OK %d checks: native SSH argv, strict profiles, permissions, migration, limits, stale mounts and "
           "local Save As\n",
           checks);
    return 0;
}
