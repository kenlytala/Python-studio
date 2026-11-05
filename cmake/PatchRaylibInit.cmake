# Safety patch for the pinned raylib 5.5 source distribution.
# Applies idempotently BEFORE compilation. No Python or patch utility is needed.
# Modified source is explicitly marked PRISME_INIT_GUARD (zlib notice).
if(NOT DEFINED RAYLIB_DIR)
    message(FATAL_ERROR "RAYLIB_DIR is required")
endif()

set(_core "${RAYLIB_DIR}/src/rcore.c")
set(_glfw "${RAYLIB_DIR}/src/platforms/rcore_desktop_glfw.c")
if(NOT EXISTS "${_core}" OR NOT EXISTS "${_glfw}")
    message(FATAL_ERROR "Prisme requires the pinned raylib 5.5 GLFW sources")
endif()

file(READ "${_core}" _source)
set(_original "${_source}")
if(NOT _source MATCHES "PRISME_INIT_GUARD: platform result")
    set(_before "    InitPlatform();")
    string(FIND "${_source}" "${_before}" _position)
    if(_position LESS 0)
        message(FATAL_ERROR "raylib InitWindow changed; refusing to omit the safety guard")
    endif()
    set(_after [=[    // PRISME_INIT_GUARD: platform result must precede ANY GPU call.
    if (InitPlatform() != 0)
    {
        CORE.Window.ready = false;
        isGpuReady = false;
        TRACELOG(LOG_WARNING, "PRISME: graphics initialization failed; skipping GPU setup");
        return;
    }]=])
    string(REPLACE "${_before}" "${_after}" _source "${_source}")
endif()
if(NOT _source MATCHES "PRISME_INIT_GUARD: close")
    set(_before "void CloseWindow(void)\n{")
    string(FIND "${_source}" "${_before}" _position)
    if(_position LESS 0)
        message(FATAL_ERROR "raylib CloseWindow changed; missing safety guard")
    endif()
    set(_after "void CloseWindow(void)\n{\n    // PRISME_INIT_GUARD: close only a successfully initialized GPU/window.\n    if (!CORE.Window.ready) return;")
    string(REPLACE "${_before}" "${_after}" _source "${_source}")
endif()
if(NOT "${_source}" STREQUAL "${_original}")
    file(WRITE "${_core}" "${_source}")
endif()

file(READ "${_glfw}" _source)
set(_original "${_source}")
if(NOT _source MATCHES "PRISME_INIT_GUARD: window creation")
    # Guard both fullscreen and windowed paths immediately after glfwCreateWindow.
    # In unpatched 5.5, GetCurrentMonitor can dereference a NULL window BEFORE
    # the existing later NULL check, so patching InitWindow alone is insufficient.
    foreach(_creation IN ITEMS
        "        platform.handle = glfwCreateWindow(CORE.Window.display.width, CORE.Window.display.height, (CORE.Window.title != 0)? CORE.Window.title : \" \", monitor, NULL);"
        "        platform.handle = glfwCreateWindow(creationWidth, creationHeight, (CORE.Window.title != 0)? CORE.Window.title : \" \", NULL, NULL);"
    )
        string(FIND "${_source}" "${_creation}" _position)
        if(_position LESS 0)
            message(FATAL_ERROR "raylib GLFW creation changed; missing NULL-window safety patch")
        endif()
        set(_guard [=[

        // PRISME_INIT_GUARD: window creation must succeed before monitor access.
        if (platform.handle == NULL)
        {
            glfwTerminate();
            TRACELOG(LOG_WARNING, "PRISME: GLFW returned no window/context; aborting safely");
            return -1;
        }]=])
        string(REPLACE "${_creation}" "${_creation}${_guard}" _source "${_source}")
    endforeach()
endif()
if(NOT "${_source}" STREQUAL "${_original}")
    file(WRITE "${_glfw}" "${_source}")
endif()
message(STATUS "Prisme: raylib 5.5 initialization guards verified")
