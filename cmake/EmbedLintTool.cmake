# The Python code is only an adapter for the external Pylint process.
# Embed bytes in C rather than relying on a runtime script location or a
# compiler-specific overlong string literal. The editor itself stays native C.
function(prisme_embed_lint_tool output)
    set(_script "${CMAKE_CURRENT_SOURCE_DIR}/tools/lint_bridge.py")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_script}")
    set(_rules "${CMAKE_CURRENT_SOURCE_DIR}/tools/lint_rules.py")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_rules}")
    file(READ "${_rules}" _rules_hex HEX)
    file(READ "${_script}" _script_hex HEX)
    set(_xml "${CMAKE_CURRENT_SOURCE_DIR}/tools/odoo_xml.py")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_xml}")
    file(READ "${_xml}" _xml_hex HEX)
    set(_hex "${_rules_hex}0a${_xml_hex}0a${_script_hex}")
    string(REGEX REPLACE "(..)" "0x\\1," _bytes "${_hex}")
    file(WRITE "${output}" "/* Generated from tools/lint_bridge.py; do not edit. */\nstatic const unsigned char lint_bridge[] = {${_bytes}0};\n")
endfunction()

# A standard-library-only, read-only AST service, also embedded in the C binary.
function(prisme_embed_completion_tool output)
    set(_script "${CMAKE_CURRENT_SOURCE_DIR}/tools/completion_bridge.py")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_script}")
    file(READ "${_script}" _hex HEX)
    string(REGEX REPLACE "(..)" "0x\\1," _bytes "${_hex}")
    file(WRITE "${output}" "/* Generated from tools/completion_bridge.py. */\nstatic const unsigned char completion_bridge[] = {${_bytes}0};\n")
endfunction()
