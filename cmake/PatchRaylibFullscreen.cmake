# Prisme's desktop fullscreen correction for the pinned raylib 5.5 backend.
# Keep the editor's logical canvas synchronized with the actual viewport.
# No Python, graphics server or additional runtime dependency is required.
if(NOT DEFINED RAYLIB_DIR)
    message(FATAL_ERROR "RAYLIB_DIR is required")
endif()
set(_glfw "${RAYLIB_DIR}/src/platforms/rcore_desktop_glfw.c")
file(READ "${_glfw}" _source)
set(_original "${_source}")

set(_begin "void ToggleFullscreen(void)\n{")
set(_end "// Toggle borderless windowed mode")
string(FIND "${_source}" "${_begin}" _first)
string(FIND "${_source}" "${_end}" _last)
if(_first LESS 0 OR _last LESS_EQUAL _first)
    message(FATAL_ERROR "raylib ToggleFullscreen changed; refusing an incomplete fullscreen correction")
endif()
# Replace the whole function, including an earlier copy of this patch. This is
# idempotent and also updates a previously patched dependency after an upgrade.
set(_replacement [=[void ToggleFullscreen(void)
{
    // PRISME_FULLSCREEN_FIT: native mode, canvas synchronization and restoration.
    static bool restoreMaximized = false;

    if (!CORE.Window.fullscreen)
    {
        int monitorCount = 0;
        int monitorIndex = GetCurrentMonitor();
        GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);
        if ((monitors == NULL) || (monitorIndex < 0) || (monitorIndex >= monitorCount))
        {
            TRACELOG(LOG_WARNING, "PRISME: no current monitor; keeping the window unchanged");
            return;
        }
        GLFWmonitor *monitor = monitors[monitorIndex];
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);
        if ((mode == NULL) || (mode->width <= 0) || (mode->height <= 0))
        {
            TRACELOG(LOG_WARNING, "PRISME: no valid native video mode; keeping the window unchanged");
            return;
        }

        CORE.Window.previousPosition = CORE.Window.position;
        CORE.Window.previousScreen = CORE.Window.screen;
        restoreMaximized = (glfwGetWindowAttrib(platform.handle, GLFW_MAXIMIZED) == GLFW_TRUE);

        CORE.Window.fullscreen = true;
        CORE.Window.flags |= FLAG_FULLSCREEN_MODE;
        CORE.Window.renderOffset.x = 0;
        CORE.Window.renderOffset.y = 0;
        CORE.Window.screen.width = mode->width;
        CORE.Window.screen.height = mode->height;

        // Never ask GLFW to fit the OLD window size to a nearby video mode.
        // A maximized window is typically shorter than the monitor (taskbar).
        glfwSetWindowMonitor(platform.handle, monitor, 0, 0, mode->width, mode->height,
                             (mode->refreshRate > 0)? mode->refreshRate : GLFW_DONT_CARE);
    }
    else
    {
        CORE.Window.fullscreen = false;
        CORE.Window.flags &= ~FLAG_FULLSCREEN_MODE;
        CORE.Window.screen = CORE.Window.previousScreen;
        CORE.Window.renderOffset.x = 0;
        CORE.Window.renderOffset.y = 0;

        glfwSetWindowMonitor(platform.handle, NULL, CORE.Window.previousPosition.x,
                             CORE.Window.previousPosition.y, CORE.Window.previousScreen.width,
                             CORE.Window.previousScreen.height, GLFW_DONT_CARE);
        CORE.Window.position = CORE.Window.previousPosition;
        if (restoreMaximized)
        {
            glfwMaximizeWindow(platform.handle);
            CORE.Window.flags |= FLAG_WINDOW_MAXIMIZED;
        }
    }

    if (CORE.Window.flags & FLAG_VSYNC_HINT) glfwSwapInterval(1);
}

]=])
string(SUBSTRING "${_source}" 0 ${_first} _prefix)
string(SUBSTRING "${_source}" ${_last} -1 _suffix)
set(_source "${_prefix}${_replacement}${_suffix}")

set(_before "    if (IsWindowFullscreen()) return;\n\n    // Set current screen size")
set(_after "    // PRISME_FULLSCREEN_FIT: a resized fullscreen canvas must follow its viewport too.\n    // Set current screen size")
string(FIND "${_source}" "${_after}" _already)
if(_already LESS 0)
    string(FIND "${_source}" "${_before}" _position)
    if(_position LESS 0)
        message(FATAL_ERROR "raylib WindowSizeCallback changed; fullscreen dimensions would remain stale")
    endif()
    string(REPLACE "${_before}" "${_after}" _source "${_source}")
endif()
if(NOT "${_source}" STREQUAL "${_original}")
    file(WRITE "${_glfw}" "${_source}")
endif()
message(STATUS "Prisme: raylib fullscreen native sizing and resize synchronization verified")
