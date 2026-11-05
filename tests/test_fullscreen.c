/* Headless regression of the actual raylib 5.5 fullscreen/resize functions.
   Only GLFW and rlgl side effects are simulated: no display or GPU is opened. */
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "layout.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                                     \
    do {                                                                                                     \
        if (!(condition)) {                                                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                             \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)

/* Field names/types used by the extracted backend functions match rcore.c. */
typedef struct {
    int x, y;
} TestPoint;
typedef struct {
    int width, height;
} TestSize;
static struct {
    struct {
        bool fullscreen, resizedLastFrame;
        unsigned int flags;
        TestPoint position, previousPosition, renderOffset;
        TestSize screen, previousScreen, render, currentFbo;
    } Window;
} CORE;
struct GLFWwindow {
    int unused;
};
struct GLFWmonitor {
    int index;
};
static GLFWwindow window;
static GLFWmonitor monitors[2] = {{0}, {1}};
static GLFWmonitor *monitor_list[2] = {&monitors[0], &monitors[1]};
static struct {
    GLFWwindow *handle;
} platform = {&window};
static GLFWvidmode modes[2];
static int selected_monitor, available_monitors;
static bool missing_modes, missing_monitors, is_maximized, delay_callback, pending_callback;
static int pending_width, pending_height, work_width, work_height;
static int requested_width, requested_height, requested_refresh, requested_monitor;
static int switch_calls, maximize_calls, swap_calls, warnings;
static Vector2 dpi;
static struct {
    int x, y, width, height, projection_width, projection_height;
} viewport;
static void WindowSizeCallback(GLFWwindow *handle, int width, int height);
static void SetupViewport(int width, int height);

#define LOG_WARNING_TEST 4
#define TRACELOG(level, ...) ((void)(level), (void)++warnings)
#define RL_PROJECTION 0x1701
#define RL_MODELVIEW 0x1700
static void rlViewport(int x, int y, int width, int height) {
    viewport.x = x;
    viewport.y = y;
    viewport.width = width;
    viewport.height = height;
}
static void rlMatrixMode(int mode) {
    (void)mode;
}
static void rlLoadIdentity(void) {}
static void rlOrtho(double left, double right, double bottom, double top, double near, double far) {
    (void)near;
    (void)far;
    CHECK(left == 0 && top == 0);
    viewport.projection_width = (int)right;
    viewport.projection_height = (int)bottom;
}
bool IsWindowFullscreen(void) {
    return CORE.Window.fullscreen;
}
Vector2 GetWindowScaleDPI(void) {
    return dpi;
}
int GetCurrentMonitor(void) {
    return selected_monitor;
}
GLFWmonitor **glfwGetMonitors(int *count) {
    *count = available_monitors;
    return missing_monitors ? NULL : monitor_list;
}
const GLFWvidmode *glfwGetVideoMode(GLFWmonitor *monitor) {
    CHECK(monitor == &monitors[0] || monitor == &monitors[1]);
    return missing_modes ? NULL : &modes[monitor->index];
}
int glfwGetWindowAttrib(GLFWwindow *handle, int attribute) {
    CHECK(handle == &window && attribute == GLFW_MAXIMIZED);
    return is_maximized ? GLFW_TRUE : GLFW_FALSE;
}
static void resize_callback(int width, int height) {
    if (delay_callback) {
        pending_callback = true;
        pending_width = width;
        pending_height = height;
    } else
        WindowSizeCallback(&window, width, height);
}
static void flush_callback(void) {
    if (pending_callback) {
        pending_callback = false;
        WindowSizeCallback(&window, pending_width, pending_height);
    }
}
void glfwSetWindowMonitor(GLFWwindow *handle, GLFWmonitor *monitor, int x, int y, int width, int height,
                          int refresh) {
    CHECK(handle == &window);
    CHECK(width > 0 && height > 0);
    switch_calls++;
    requested_width = width;
    requested_height = height;
    requested_refresh = refresh;
    requested_monitor = monitor ? monitor->index : -1;
    is_maximized = false;
    CORE.Window.flags &= ~FLAG_WINDOW_MAXIMIZED;
    if (monitor) {
        /* GLFW can select a mode taller than the requested window (the bug). */
        CORE.Window.position = (TestPoint){monitor->index ? -2560 : 0, monitor->index ? -120 : 0};
        resize_callback(modes[monitor->index].width, modes[monitor->index].height);
    } else {
        CORE.Window.position = (TestPoint){x, y};
        resize_callback(width, height);
    }
}
void glfwMaximizeWindow(GLFWwindow *handle) {
    CHECK(handle == &window);
    maximize_calls++;
    is_maximized = true;
    CORE.Window.flags |= FLAG_WINDOW_MAXIMIZED;
    resize_callback(work_width, work_height);
}
void glfwSwapInterval(int interval) {
    CHECK(interval == 1);
    swap_calls++;
}

#ifndef PRISME_FULLSCREEN_FUNCTIONS
#define PRISME_FULLSCREEN_FUNCTIONS "raylib_fullscreen_functions.inc"
#endif
/* The callback's unused GLFWwindow parameter is unchanged upstream code. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include PRISME_FULLSCREEN_FUNCTIONS
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static void setup(int width, int height, int native_width, int native_height, int monitor, bool maximized,
                  float scale, bool delayed) {
    memset(&CORE, 0, sizeof(CORE));
    memset(&viewport, 0, sizeof(viewport));
    selected_monitor = monitor;
    available_monitors = 2;
    missing_modes = missing_monitors = false;
    is_maximized = maximized;
    delay_callback = delayed;
    pending_callback = false;
    switch_calls = maximize_calls = swap_calls = warnings = 0;
    requested_monitor = -1;
    dpi = (Vector2){scale, scale};
    modes[0] = (GLFWvidmode){1280, 720, 8, 8, 8, 60};
    modes[1] = (GLFWvidmode){2560, 1440, 8, 8, 8, 75};
    modes[monitor] = (GLFWvidmode){native_width, native_height, 8, 8, 8, 144};
    work_width = width;
    work_height = height;
    CORE.Window.position = (TestPoint){monitor ? -2493 : 67, monitor ? -91 : 29};
    CORE.Window.screen = (TestSize){width, height};
    CORE.Window.currentFbo = CORE.Window.screen;
    CORE.Window.flags = FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED | FLAG_VSYNC_HINT |
                        (maximized ? FLAG_WINDOW_MAXIMIZED : 0);
    SetupViewport(width, height);
}
static void check_fills_surface(void) {
    CHECK(GetScreenWidth() == CORE.Window.currentFbo.width);
    CHECK(GetScreenHeight() == CORE.Window.currentFbo.height);
    CHECK(GetScreenWidth() == viewport.projection_width);
    CHECK(GetScreenHeight() == viewport.projection_height);
    CHECK(GetRenderWidth() == viewport.width && GetRenderHeight() == viewport.height);
    CHECK(viewport.x == 0 && viewport.y == 0);
    WorkspaceLayout layout =
        layout_workspace(GetScreenWidth(), GetScreenHeight(), true, true, true, false, 226, 211);
    CHECK(layout.width == viewport.projection_width);
    CHECK(layout.status_y + layout.status_h == viewport.projection_height);
#if defined(__APPLE__)
    CHECK((int)((float)(layout.status_y + layout.status_h) * dpi.y) == viewport.height);
    CHECK((int)((float)layout.width * dpi.x) == viewport.width);
#else
    CHECK(layout.status_y + layout.status_h == viewport.height);
    CHECK(layout.width == viewport.width);
#endif
}
static void check_rejected_monitor(int kind) {
    setup(1100, 740, 1920, 1080, 0, true, 1, false);
    TestPoint position = CORE.Window.position;
    if (kind == 0)
        available_monitors = 0;
    if (kind == 1)
        selected_monitor = -1;
    if (kind == 2)
        selected_monitor = 2;
    if (kind == 3)
        missing_monitors = true;
    if (kind == 4)
        missing_modes = true;
    if (kind == 5)
        modes[0].width = 0;
    if (kind == 6)
        modes[0].height = 0;
    ToggleFullscreen();
    CHECK(!IsWindowFullscreen() && !(CORE.Window.flags & FLAG_FULLSCREEN_MODE));
    CHECK(GetScreenWidth() == 1100 && GetScreenHeight() == 740);
    CHECK(CORE.Window.position.x == position.x && CORE.Window.position.y == position.y);
    CHECK(is_maximized && switch_calls == 0 && warnings == 1);
    check_fills_surface();
}
int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--diagnose")) {
        setup(1920, 1040, 1920, 1080, 0, true, 1, false);
        ToggleFullscreen();
        WorkspaceLayout layout =
            layout_workspace(GetScreenWidth(), GetScreenHeight(), true, true, true, false, 226, 211);
        int gap = viewport.projection_height - layout.status_y - layout.status_h;
        printf("Fullscreen: requested %dx%d, viewport %dx%d, logical %dx%d, bottom gap %d px\n",
               requested_width, requested_height, viewport.projection_width, viewport.projection_height,
               GetScreenWidth(), GetScreenHeight(), gap);
        return gap == 0 ? 0 : 1;
    }
    const int screens[][2] = {{1280, 720},  {1366, 768},  {1440, 900},  {1600, 900},
                              {1920, 1080}, {1920, 1200}, {2560, 1440}, {2560, 1600},
                              {3440, 1440}, {3840, 2160}, {1080, 1920}, {2160, 3840}};
    const int bottom_insets[] = {0, 28, 40, 80};
    const float scales[] = {1, 1.25f, 1.5f, 2};
    int cases = 0, cycles = 0;
    for (size_t s = 0; s < sizeof(screens) / sizeof(*screens); s++)
        for (size_t inset = 0; inset < sizeof(bottom_insets) / sizeof(*bottom_insets); inset++)
            for (int style = 0; style < 2; style++)
                for (int maximized = 0; maximized < 2; maximized++)
                    for (size_t scale = 0; scale < sizeof(scales) / sizeof(*scales); scale++)
                        for (int delayed = 0; delayed < 2; delayed++) {
                            int nw = screens[s][0], nh = screens[s][1];
                            int w = style ? nw * 3 / 4 : nw;
                            int h = (style ? nh * 3 / 4 : nh) - bottom_insets[inset];
                            int monitor = (int)(s + inset + (size_t)style) % 2;
                            setup(w, h, nw, nh, monitor, maximized != 0, scales[scale], delayed != 0);
                            TestPoint old_position = CORE.Window.position;
                            check_fills_surface();
                            for (int cycle = 0; cycle < 3; cycle++) {
                                ToggleFullscreen();
                                CHECK(IsWindowFullscreen() && (CORE.Window.flags & FLAG_FULLSCREEN_MODE));
                                CHECK(CORE.Window.flags & FLAG_WINDOW_UNDECORATED);
                                CHECK(requested_monitor == monitor);
                                CHECK(requested_width == nw && requested_height == nh);
                                CHECK(requested_refresh == modes[monitor].refreshRate);
                                CHECK(GetScreenWidth() == nw && GetScreenHeight() == nh);
                                CHECK(CORE.Window.previousScreen.width == w &&
                                      CORE.Window.previousScreen.height == h);
                                CHECK(CORE.Window.previousPosition.x == old_position.x);
                                CHECK(CORE.Window.previousPosition.y == old_position.y);
                                flush_callback();
                                check_fills_surface();
                                /* A later mode/size event must also update the logical canvas. */
                                CORE.Window.resizedLastFrame = false;
                                WindowSizeCallback(&window, nw - 64, nh - 32);
                                CHECK(CORE.Window.resizedLastFrame);
                                check_fills_surface();
                                ToggleFullscreen();
                                CHECK(!IsWindowFullscreen() && !(CORE.Window.flags & FLAG_FULLSCREEN_MODE));
                                CHECK(requested_monitor == -1 && requested_width == w &&
                                      requested_height == h);
                                flush_callback();
                                CHECK(GetScreenWidth() == w && GetScreenHeight() == h);
                                CHECK(CORE.Window.position.x == old_position.x &&
                                      CORE.Window.position.y == old_position.y);
                                CHECK(is_maximized == (maximized != 0));
                                CHECK(maximize_calls == (cycle + 1) * maximized);
                                CHECK(swap_calls == (cycle + 1) * 2);
                                check_fills_surface();
                                cycles++;
                            }
                            CHECK(switch_calls == 6 && warnings == 0);
                            cases++;
                        }
    for (int kind = 0; kind < 7; kind++)
        check_rejected_monitor(kind);
    puts("OK actual raylib fullscreen + resize functions, no display/window/GPU");
    printf("OK %d configurations, %d fullscreen/restore cycles, 7 unavailable-monitor cases\n", cases,
           cycles);
#if defined(__APPLE__)
    puts("OK Retina viewport and footer coverage at 100%, 125%, 150%, 200% DPI");
#else
    puts("OK native viewport and footer coverage, maximized restoration, two monitors, delayed callbacks");
#endif
    return 0;
}
