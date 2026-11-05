#ifndef PYTHON_STUDIO_SESSION_H
#define PYTHON_STUDIO_SESSION_H
#include "buffer.h"
#include "ssh.h"
#include <stdint.h>
#define STUDIO_TABS 24
#define STUDIO_EXPANDED 48
#define STUDIO_SESSION_LIMIT (104u * 1024u * 1024u)
enum { ST_SPLIT = 1, ST_SIDEBAR = 2, ST_TOPBAR = 4, ST_TERMINAL = 8, ST_MINIMAP = 16, ST_TREE_OPEN = 32 };
typedef struct {
    Buffer *tabs[STUDIO_TABS];
    bool remote[STUDIO_TABS], discard[STUDIO_TABS];
    int count, active, panes[2], active_pane;
    uint32_t flags;
    double ratio, terminal_height;
    int font_size, sidebar_width, tree_scroll, console_mode;
    char project[BUF_PATH], remote_root[BUF_PATH];
    SshProfile profile;
    char expanded[STUDIO_EXPANDED][BUF_PATH];
    int expanded_count;
} StudioImage;
typedef struct {
    char directory[BUF_PATH], path[BUF_PATH], project[BUF_PATH];
    int lock_fd;
    bool enabled;
    unsigned long serial;
} StudioStore;
void studio_default_directory(char *out, size_t cap);
void studio_store_init(StudioStore *s, const char *directory, bool enabled);
void studio_store_close(StudioStore *s);
bool studio_last_project(StudioStore *s, char *out, size_t cap);
/* Missing returns 0, valid returns 1, malformed/I/O returns -1, preserving the file. */
int studio_load(StudioStore *s, const char *project, StudioImage *image, char *error, size_t cap);
bool studio_save(StudioStore *s, const StudioImage *image, char *error, size_t cap);
void studio_image_free(StudioImage *image);
uint64_t studio_signature(const StudioImage *image);
/* Disk reads are regular-file-only, bounded and guarded; never run at startup for SSH. */
int studio_reconcile(StudioImage *image, SshSession *ssh, bool remote_only);
#endif
