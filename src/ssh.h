#ifndef PRISME_SSH_H
#define PRISME_SSH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#define SSH_PATH 4096
#define SSH_PROFILES_MAX 32
#define SSH_JSON_MAX (1024 * 1024)
#define SSH_AUTH_MAX 8192
#define SSH_PROMPT_MAX 16000

typedef struct {
    char id[33], host[254], user[129], port[6], folder[SSH_PATH], identity[SSH_PATH];
} SshProfile;
typedef struct {
    SshProfile items[SSH_PROFILES_MAX];
    int count;
    char last[33];
} SshProfiles;
void ssh_profile_init(SshProfile *p);
bool ssh_profile_valid(const SshProfile *p, bool check_key, char *error, size_t cap);
bool ssh_profile_equal(const SshProfile *a, const SshProfile *b);
void ssh_profile_label(const SshProfile *p, char *out, size_t cap);
bool ssh_profiles_default_path(char *out, size_t cap);
bool ssh_profiles_load(const char *path, SshProfiles *out, char *error, size_t cap);
/* Atomic read/modify/write under a common cross-edition flock. */
bool ssh_profiles_put(const char *path, SshProfile *profile, char *error, size_t cap);
bool ssh_profiles_select(const char *path, const char *id, char *error, size_t cap);
bool ssh_profiles_delete(const char *path, const char *id, char *error, size_t cap);

/* argv belongs to the command, no password/passphrase is ever in it. */
typedef struct {
    char source[SSH_PATH + 280], options[24576], identity_option[SSH_PATH * 2 + 32], *argv[40];
    int count;
} SshCommand;
bool ssh_command(SshCommand *c, const SshProfile *p, const char *mountpoint, const char *sshfs,
                 const char *ssh, const char *config, char *error, size_t cap);
typedef enum { SSH_IDLE, SSH_CONNECTING, SSH_MOUNTED, SSH_LOST, SSH_DISCONNECTING, SSH_ERROR } SshState;
typedef enum {
    SSH_EVENT_NONE,
    SSH_EVENT_READY,
    SSH_EVENT_DISCONNECTED,
    SSH_EVENT_ERROR,
    SSH_EVENT_LOST
} SshEvent;
typedef struct {
    bool initialized;
    SshProfile profile;
    _Atomic(SshState) state;
    char cache[SSH_PATH], mountpoint[SSH_PATH], helper[SSH_PATH], config_override[SSH_PATH];
    char runtime[128], socket_path[160], token[65], unmount_program[SSH_PATH];
    char error[1024], log[32769], prompt[SSH_PROMPT_MAX + 1];
    size_t log_len;
    int listener, client, log_fd, lock_fd;
    long process, probe, unmount_process;
    atomic_long generation;
    double deadline, client_deadline, stop_deadline;
    atomic_bool process_alive;
    bool auth_pending, auth_confirm, cancelled, failing, stopping, probe_stopping;
    char incoming[65537];
    size_t incoming_len;
} SshSession;
void ssh_init(SshSession *s, const char *cache, const char *helper);
bool ssh_connect(SshSession *s, const SshProfile *profile, char *error, size_t cap);
SshEvent ssh_poll(SshSession *s);
void ssh_cancel(SshSession *s);
bool ssh_disconnect(SshSession *s, char *error, size_t cap);
bool ssh_answer(SshSession *s, const char *answer, char *error, size_t cap);
bool ssh_owns(const SshSession *s, const char *path);
bool ssh_guard(void *session, const char *path, bool allow_leaf_link, char *error, size_t cap);
bool ssh_save_guard(void *session, const char *path, char *error, size_t cap);
bool ssh_is_dir(SshSession *s, const char *path);
bool ssh_mounted(SshSession *s);
/* 0 absent, >0 unique mount ID, -1 unknown/unreadable, -2 stacked/wrong type. */
long ssh_mount_id(const char *path, bool sshfs_only);
const char *ssh_state_label(SshState state);
/* Normal unmount only. Returns false rather than unmounting another mount or forcing busy files. */
bool ssh_close(SshSession *s, char *error, size_t cap);
void ssh_erase(void *data, size_t len);
/* Native helper entry point: askpass via private Unix IPC, or directory probe. */
int ssh_helper_main(int argc, char **argv);
#endif
