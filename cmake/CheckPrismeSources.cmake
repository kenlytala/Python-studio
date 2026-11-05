# Keep project objects coherent after an archive is extracted over an older
# release. Extractors may preserve mtimes older than existing .o files, so Make
# can otherwise keep an old buffer.c.o next to a new multicursor.c.o.
set(_prisme_root "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(_required IN ITEMS src/buffer.c src/buffer.h src/multicursor.c src/multicursor.h)
    if(NOT EXISTS "${_prisme_root}/${_required}")
        message(FATAL_ERROR
            "Archive Prisme incomplete : ${_required} manque. "
            "Extrayez l'archive complete dans un dossier neuf.")
    endif()
endforeach()
file(READ "${_prisme_root}/src/buffer.c" _prisme_buffer_source)
if(NOT _prisme_buffer_source MATCHES
    "bool[ \t\r\n]+buf_apply_edits[ \t\r\n]*\\([^;{}]*\\)[ \t\r\n]*\\{")
    message(FATAL_ERROR
        "Sources Prisme incompatibles : src/buffer.c ne definit pas buf_apply_edits. "
        "Ne melangez pas les anciennes et les nouvelles sources. "
        "Extrayez l'archive complete, puis configurez un nouveau dossier de construction.")
endif()

# An explicit configure (also performed by lancer.sh) fingerprints file CONTENT,
# not its timestamp. Any source/header change updates the compiler command for
# every Prisme target, invalidating stale object files and their old Buffer ABI.
# raylib is deliberately excluded: there is no need to rebuild it for this fix.
file(GLOB_RECURSE _prisme_source_files CONFIGURE_DEPENDS
    "${_prisme_root}/src/*.c" "${_prisme_root}/src/*.h"
    "${_prisme_root}/tests/*.c" "${_prisme_root}/tests/*.h" "${_prisme_root}/tests/*.py" "${_prisme_root}/demos/*.c"
    "${_prisme_root}/cmake/*.cmake" "${_prisme_root}/tools/*.py")
list(APPEND _prisme_source_files
    "${_prisme_root}/CMakeLists.txt"
    "${CMAKE_CURRENT_LIST_FILE}")
list(REMOVE_DUPLICATES _prisme_source_files)
list(SORT _prisme_source_files)
set(_prisme_signature "Prisme ${PROJECT_VERSION}\n")
foreach(_source IN LISTS _prisme_source_files)
    file(SHA256 "${_source}" _hash)
    file(RELATIVE_PATH _name "${_prisme_root}" "${_source}")
    string(APPEND _prisme_signature "${_name}=${_hash}\n")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_prisme_source_files})
string(SHA256 PRISME_SOURCE_FINGERPRINT "${_prisme_signature}")
message(STATUS "Prisme: coherent source fingerprint ${PRISME_SOURCE_FINGERPRINT}")
