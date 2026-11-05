#define _POSIX_C_SOURCE 200809L
#include "file_clipboard.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
int file_clipboard_read(FsPaths *paths) {
    paths->count = 0;
    if (!OpenClipboard(NULL))
        return 0;
    HDROP drop = (HDROP)GetClipboardData(CF_HDROP);
    if (drop) {
        UINT count = DragQueryFileW(drop, 0xFFFFFFFF, NULL, 0);
        if (count > FS_MAX_PATHS) {
            CloseClipboard();
            return -1;
        }
        for (UINT i = 0; i < count && paths->count < FS_MAX_PATHS; i++) {
            wchar_t path[FS_PATH];
            UINT n = DragQueryFileW(drop, i, path, FS_PATH);
            if (!n || n >= FS_PATH ||
                !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path, -1, paths->paths[paths->count],
                                     FS_PATH, NULL, NULL)) {
                paths->count = 0;
                CloseClipboard();
                return -1;
            }
            paths->count++;
        }
    }
    CloseClipboard();
    return paths->count;
}
bool file_clipboard_write(const FsPaths *paths) {
    if (!paths || paths->count < 1 || paths->count > FS_MAX_PATHS)
        return false;
    size_t count = 1;
    for (int i = 0; i < paths->count; i++) {
        int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, paths->paths[i], -1, NULL, 0);
        if (!n)
            return false;
        count += (size_t)n;
    }
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DROPFILES) + count * sizeof(wchar_t));
    if (!data)
        return false;
    DROPFILES *drop = GlobalLock(data);
    if (!drop) {
        GlobalFree(data);
        return false;
    }
    drop->pFiles = sizeof(DROPFILES);
    drop->fWide = TRUE;
    wchar_t *p = (wchar_t *)(drop + 1);
    for (int i = 0; i < paths->count; i++)
        p += MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, paths->paths[i], -1, p, (int)count);
    GlobalUnlock(data);
    HWND owner = GetActiveWindow();
    if (!owner || !OpenClipboard(owner)) {
        GlobalFree(data);
        return false;
    }
    bool ok = EmptyClipboard() && SetClipboardData(CF_HDROP, data) != NULL;
    CloseClipboard();
    if (!ok)
        GlobalFree(data);
    return ok;
}
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
int file_clipboard_read(FsPaths *paths) {
    paths->count = 0;
    PasteboardRef board = NULL;
    if (PasteboardCreate(kPasteboardClipboard, &board) != noErr)
        return 0;
    PasteboardSynchronize(board);
    ItemCount count = 0;
    PasteboardGetItemCount(board, &count);
    if (count > FS_MAX_PATHS) {
        CFRelease(board);
        return -1;
    }
    bool overflow = false;
    char *text = calloc(1, FS_CLIP_LIMIT + 1);
    size_t used = 0;
    if (text)
        for (ItemCount i = 1; i <= count && i <= FS_MAX_PATHS; i++) {
            PasteboardItemID item;
            CFDataRef data = NULL;
            if (PasteboardGetItemIdentifier(board, i, &item) == noErr &&
                PasteboardCopyItemFlavorData(board, item, CFSTR("public.file-url"), &data) == noErr && data) {
                size_t n = (size_t)CFDataGetLength(data);
                const UInt8 *bytes = CFDataGetBytePtr(data);
                while (n && bytes[n - 1] == 0)
                    n--;
                if (n && !memchr(bytes, 0, n)) {
                    if (n + 1 > FS_CLIP_LIMIT - used)
                        overflow = true;
                    else {
                        memcpy(text + used, bytes, n);
                        used += n;
                        text[used++] = '\n';
                    }
                }
                CFRelease(data);
                if (overflow)
                    break;
            }
        }
    int result = overflow ? -1 : (text ? fs_paths_parse(paths, text) : 0);
    free(text);
    CFRelease(board);
    return result;
}
bool file_clipboard_write(const FsPaths *paths) {
    (void)paths;
    return false;
}
#elif defined(PRISME_CLIPBOARD_X11)
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <poll.h>
#include <time.h>
static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
static bool next_event(Display *display, XEvent *event, double deadline) {
    if (now() >= deadline)
        return false;
    while (!XPending(display)) {
        double remaining = deadline - now();
        if (remaining <= 0)
            return false;
        struct pollfd fd = {ConnectionNumber(display), POLLIN, 0};
        if (poll(&fd, 1, (int)(remaining * 1000) + 1) <= 0)
            return false;
    }
    XNextEvent(display, event);
    return true;
}
static bool selection(Display *d, Window window, Atom clipboard, Atom target, char *out, double deadline) {
    Atom property = XInternAtom(d, "PRISME_FILE_CLIPBOARD", False), incr = XInternAtom(d, "INCR", False);
    XDeleteProperty(d, window, property);
    XConvertSelection(d, clipboard, target, property, window, CurrentTime);
    XFlush(d);
    XEvent event;
    bool incremental = false;
    size_t used = 0;
    while (next_event(d, &event, deadline)) {
        bool read = false;
        if (event.type == SelectionNotify) {
            if (event.xselection.property == None)
                return false;
            read = true;
        }
        if (incremental && event.type == PropertyNotify && event.xproperty.state == PropertyNewValue)
            read = true;
        if (!read)
            continue;
        Atom actual;
        int format;
        unsigned long count, left;
        unsigned char *data = NULL;
        if (XGetWindowProperty(d, window, property, 0, FS_CLIP_LIMIT / 4, True, AnyPropertyType, &actual,
                               &format, &count, &left, &data) != Success)
            return false;
        if (actual == incr) {
            incremental = true;
            if (data)
                XFree(data);
            XFlush(d);
            continue;
        }
        if (format != 8 || left || used + count > FS_CLIP_LIMIT) {
            if (data)
                XFree(data);
            return false;
        }
        if (count && data) {
            memcpy(out + used, data, count);
            used += count;
        }
        if (data)
            XFree(data);
        out[used] = 0;
        if (!incremental || !count)
            return used > 0;
        XFlush(d);
    }
    return false;
}
int file_clipboard_read(FsPaths *paths) {
    paths->count = 0;
    Display *d = XOpenDisplay(NULL);
    if (!d)
        return 0;
    Atom clipboard = XInternAtom(d, "CLIPBOARD", False);
    if (XGetSelectionOwner(d, clipboard) == None) {
        XCloseDisplay(d);
        return 0;
    }
    /* Unmapped event receiver, not a visible window. No display is started. */
    Window window = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, 1, 1, 0, 0, 0);
    XSelectInput(d, window, PropertyChangeMask);
    char *text = calloc(1, FS_CLIP_LIMIT + 1);
    const char *types[] = {"text/uri-list", "x-special/gnome-copied-files"};
    double deadline = now() + .5;
    int result = 0;
    if (text)
        for (int i = 0; i < 2; i++)
            if (selection(d, window, clipboard, XInternAtom(d, types[i], False), text, deadline)) {
                result = fs_paths_parse(paths, text);
                if (result)
                    break;
            }
    free(text);
    XDestroyWindow(d, window);
    XCloseDisplay(d);
    return result;
}
bool file_clipboard_write(const FsPaths *paths) {
    (void)paths;
    return false;
}
#else
int file_clipboard_read(FsPaths *paths) {
    paths->count = 0;
    return 0;
}
bool file_clipboard_write(const FsPaths *paths) {
    (void)paths;
    return false;
}
#endif
