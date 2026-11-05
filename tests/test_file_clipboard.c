/* Exercise the actual X11 clipboard transport with a simulated selection owner.
   No real Display, X window, graphics server or preview is opened. */
#define _POSIX_C_SOURCE 200809L
#define PRISME_CLIPBOARD_X11

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/file_clipboard.c"
static int checks;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        checks++;                                                                                            \
        if (!(c)) {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c);                                                  \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static _XPrivDisplay display;
static Screen screen;
static const char *payload;
static bool gnome, incremental, no_owner, unavailable;
static int requested, stage, chunk, created, destroyed, closed;
static Atom current_target;
static const Atom A_CLIP = 1, A_PROP = 2, A_INCR = 3, A_URI = 4, A_GNOME = 5;
Display *XOpenDisplay(const char *name) {
    (void)name;
    return unavailable ? NULL : (Display *)display;
}
int XCloseDisplay(Display *d) {
    CHECK(d == (Display *)display);
    closed++;
    return 0;
}
Atom XInternAtom(Display *d, const char *name, Bool only) {
    (void)d;
    (void)only;
    if (!strcmp(name, "CLIPBOARD"))
        return A_CLIP;
    if (!strcmp(name, "INCR"))
        return A_INCR;
    if (!strcmp(name, "text/uri-list"))
        return A_URI;
    if (!strcmp(name, "x-special/gnome-copied-files"))
        return A_GNOME;
    return A_PROP;
}
Window XGetSelectionOwner(Display *d, Atom selection) {
    (void)d;
    CHECK(selection == A_CLIP);
    return no_owner ? None : 77;
}
Window XCreateSimpleWindow(Display *d, Window parent, int x, int y, unsigned int w, unsigned int h,
                           unsigned int border, unsigned long color, unsigned long background) {
    (void)d;
    (void)x;
    (void)y;
    (void)border;
    (void)color;
    (void)background;
    CHECK(parent == 42 && w == 1 && h == 1);
    created++;
    return 99;
}
int XSelectInput(Display *d, Window window, long mask) {
    (void)d;
    CHECK(window == 99 && mask == PropertyChangeMask);
    return 0;
}
int XDeleteProperty(Display *d, Window window, Atom property) {
    (void)d;
    CHECK(window == 99 && property == A_PROP);
    return 0;
}
int XConvertSelection(Display *d, Atom clipboard, Atom target, Atom property, Window window, Time time) {
    (void)d;
    (void)time;
    CHECK(clipboard == A_CLIP && property == A_PROP && window == 99);
    current_target = target;
    requested++;
    stage = 0;
    chunk = 0;
    return 0;
}
int XFlush(Display *d) {
    (void)d;
    return 0;
}
int XPending(Display *d) {
    (void)d;
    return 1;
}
int XNextEvent(Display *d, XEvent *event) {
    (void)d;
    memset(event, 0, sizeof(*event));
    if (stage == 0) {
        event->type = SelectionNotify;
        event->xselection.property = gnome && current_target == A_URI ? None : A_PROP;
        stage = 1;
    } else {
        event->type = PropertyNotify;
        event->xproperty.state = PropertyNewValue;
    }
    return 0;
}
int XGetWindowProperty(Display *d, Window window, Atom property, long offset, long length, Bool remove,
                       Atom requested_type, Atom *actual, int *format, unsigned long *count,
                       unsigned long *left, unsigned char **data) {
    (void)d;
    (void)offset;
    (void)length;
    (void)requested_type;
    CHECK(window == 99 && property == A_PROP && remove);
    *left = 0;
    *actual = A_URI;
    *format = 8;
    if (incremental && chunk == 0) {
        *actual = A_INCR;
        *format = 32;
        *count = 1;
        *data = calloc(1, sizeof(long));
        chunk++;
        return Success;
    }
    size_t n = strlen(payload), start = 0;
    if (incremental) {
        if (chunk == 1)
            n /= 2;
        else if (chunk == 2) {
            start = n / 2;
            n -= start;
        } else
            n = 0;
        chunk++;
    }
    *count = (unsigned long)n;
    *data = malloc(n + 1);
    if (n)
        memcpy(*data, payload + start, n);
    return Success;
}
int XFree(void *data) {
    free(data);
    return 0;
}
int XDestroyWindow(Display *d, Window window) {
    (void)d;
    CHECK(window == 99);
    destroyed++;
    return 0;
}
static void reset(void) {
    gnome = incremental = no_owner = unavailable = false;
    created = destroyed = closed = requested = 0;
}
int main(void) {
    display = calloc(1, sizeof(*display));
    CHECK(display);
    display->screens = &screen;
    screen.root = 42;
    FsPaths *paths = calloc(1, sizeof(*paths));
    CHECK(paths);
    reset();
    payload = "file:///home/user/Mon%20dossier/étoile.py\r\nfile:///tmp/second\r\n";
    CHECK(file_clipboard_read(paths) == 2);
    CHECK(!strcmp(paths->paths[0], "/home/user/Mon dossier/étoile.py"));
    CHECK(created == 1 && destroyed == 1 && closed == 1 && requested == 1);
    reset();
    gnome = true;
    payload = "copy\nfile:///tmp/a\n";
    CHECK(file_clipboard_read(paths) == 1 && requested == 2);
    reset();
    gnome = true;
    payload = "cut\nfile:///tmp/a\n";
    CHECK(file_clipboard_read(paths) == 1); /* Treated as copy, never deletes the source. */
    reset();
    incremental = true;
    payload = "file:///tmp/a%20b\nfile:///tmp/c\n";
    CHECK(file_clipboard_read(paths) == 2 && chunk == 4);
    CHECK(!strcmp(paths->paths[0], "/tmp/a b"));
    reset();
    no_owner = true;
    CHECK(file_clipboard_read(paths) == 0 && !created && closed == 1);
    reset();
    unavailable = true;
    CHECK(file_clipboard_read(paths) == 0 && !created && !closed);
    reset();
    payload = "file://remote/not-supported\nnot a path\n";
    CHECK(file_clipboard_read(paths) == 0 && created == destroyed);
    char overflow[4096] = {0};
    size_t used = 0;
    for (int i = 0; i < 65; i++)
        used += (size_t)snprintf(overflow + used, sizeof(overflow) - used, "file:///tmp/clip-%d\n", i);
    reset();
    payload = overflow;
    CHECK(file_clipboard_read(paths) == -1 && paths->count == 0 && created == destroyed);
    free(paths);
    free(display);
    printf("OK %d X11 transport assertions\n", checks);
    puts("OK actual X11 clipboard transport: URI-list, GNOME copy/cut, INCR chunks, unavailable "
         "selection/display; no GUI");
    return 0;
}
