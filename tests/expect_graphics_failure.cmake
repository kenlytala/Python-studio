# Linux/X11 regression: a failed InitWindow must return code 3, not a signal.
# The invalid display needs neither Xvfb nor an OpenGL driver.
if(NOT DEFINED PRISME_TEST_APP)
    message(FATAL_ERROR "PRISME_TEST_APP is required")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "DISPLAY=:65530" "WAYLAND_DISPLAY="
        "${PRISME_TEST_APP}" --frames 1
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 10
)
if(NOT "${result}" STREQUAL "3")
    message(FATAL_ERROR "Expected clean graphics error 3; got ${result}\n${output}\n${error}")
endif()
message(STATUS "Failed display/context handled cleanly (exit 3): ${PRISME_TEST_APP}")
