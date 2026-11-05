#ifndef PRISME_FILE_CLIPBOARD_H
#define PRISME_FILE_CLIPBOARD_H
#include "files.h"
/* Native file-manager clipboard (X11 URI selections / Windows CF_HDROP /
   macOS Pasteboard). Never executes a shell command. */
int file_clipboard_read(FsPaths *paths);
bool file_clipboard_write(const FsPaths *paths);
#endif
