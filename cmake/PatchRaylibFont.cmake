# raylib 5.5 assumes that every glyph bitmap is <= the requested font size.
# Real fonts may legitimately overshoot their nominal em height (e.g. '|').
# Pack their full bitmaps with the ACTUAL maximum height; keep baseSize/metrics.
if(NOT DEFINED RAYLIB_DIR)
    message(FATAL_ERROR "RAYLIB_DIR is required")
endif()
set(_text "${RAYLIB_DIR}/src/rtext.c")
file(READ "${_text}" _source)
set(_original "${_source}")
if(NOT _source MATCHES "PRISME_FONT_EXTENTS: raster")
    set(_before "                        if (chh > fontSize) TRACELOG(LOG_WARNING, \"FONT: Character [0x%08x] size is bigger than expected font size\", ch);")
    string(FIND "${_source}" "${_before}" _found)
    if(_found LESS 0)
        message(FATAL_ERROR "Pinned raylib font rasterizer changed; missing glyph-extents correction")
    endif()
    set(_after [=[                        // PRISME_FONT_EXTENTS: raster overshoot is valid, not a malformed font.
                        // GenImageFontAtlas below packs the complete bitmap using actual extents.
                        if (chh > fontSize) TRACELOG(LOG_DEBUG, "FONT: Glyph [0x%08x] extends beyond the nominal em height; packing full bitmap", ch);]=])
    string(REPLACE "${_before}" "${_after}" _source "${_source}")
endif()
if(NOT _source MATCHES "PRISME_FONT_EXTENTS: atlas")
    set(_before "    int maxGlyphWidth = 0;\n\n    for (int i = 0; i < glyphCount; i++)")
    set(_after [=[    int maxGlyphWidth = 0;
    // PRISME_FONT_EXTENTS: atlas rows must fit tall glyphs, including Inter '|'.
    int maxGlyphHeight = fontSize;

    for (int i = 0; i < glyphCount; i++)]=])
    string(FIND "${_source}" "${_before}" _found)
    if(_found LESS 0)
        message(FATAL_ERROR "Pinned raylib atlas layout changed; missing glyph-height calculation")
    endif()
    string(REPLACE "${_before}" "${_after}" _source "${_source}")
    set(_before "        if (glyphs[i].image.width > maxGlyphWidth) maxGlyphWidth = glyphs[i].image.width;")
    set(_after "${_before}\n        if (glyphs[i].image.height > maxGlyphHeight) maxGlyphHeight = glyphs[i].image.height;")
    string(REPLACE "${_before}" "${_after}" _source "${_source}")
    # Estimate atlas area AND use the same full-height bound for every row and
    # bottom-edge check. Do not crop glyphs or change advance/baseline metrics.
    foreach(_before IN ITEMS "fontSize + 2*padding" "atlas.height - fontSize - padding")
        string(FIND "${_source}" "${_before}" _found)
        if(_found LESS 0)
            message(FATAL_ERROR "Pinned raylib atlas sizing changed; refusing an incomplete font correction")
        endif()
        string(REPLACE "fontSize" "maxGlyphHeight" _after "${_before}")
        string(REPLACE "${_before}" "${_after}" _source "${_source}")
    endforeach()
    # The first unpacked glyph also needs an initialized empty rectangle.
    string(REPLACE "for (int j = i + 1; j < glyphCount; j++)" "for (int j = i; j < glyphCount; j++)" _source "${_source}")
endif()
if(NOT "${_source}" STREQUAL "${_original}")
    file(WRITE "${_text}" "${_source}")
endif()
message(STATUS "Prisme: raylib 5.5 full glyph extents and atlas rows verified")
