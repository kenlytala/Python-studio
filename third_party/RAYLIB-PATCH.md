# Local safety, fullscreen and font-atlas modifications to raylib 5.5

Raylib 5.5 is downloaded from the upstream release with its original SHA-256 checked by CMake. The downloaded files are then **locally modified**, not represented as unmodified upstream source.

`cmake/PatchRaylibInit.cmake` applies these idempotent C changes before compilation:

1. In `src/platforms/rcore_desktop_glfw.c`, check `glfwCreateWindow()` for NULL immediately after both creation calls. On failure, terminate GLFW and return -1 **before** calling `GetCurrentMonitor()` or any other window operation.
2. In `src/rcore.c`, check `InitPlatform()`'s result inside `InitWindow()`. If initialization fails, leave the window/GPU readiness flags false and return **before** `rlglInit()`, viewport setup or default-font loading.
3. Make `CloseWindow()` a no-op for an uninitialized window, avoiding cleanup of nonexistent GPU resources.

Each changed section is marked `PRISME_INIT_GUARD`. Configuration fails if the expected pinned source patterns cannot be found; it never silently builds without the guards.

These changes address an initialization failure path reproduced with a GLX context creation error, not an assertion that every graphics driver problem can be repaired by the application. OpenGL context creation still requires a working driver/display.

Raylib's original zlib/libpng license is retained in `raylib-LICENSE.txt`. Original authorship is unchanged. Prisme's patch is distributed under the project MIT license.

## Native fullscreen surface correction (Prisme 1.2.3)

`cmake/PatchRaylibFullscreen.cmake` additionally modifies the pinned GLFW backend:

1. `ToggleFullscreen()` saves the window rectangle and maximized state, requests the active monitor's current video mode rather than the old window dimensions, and restores the saved window state on exit. Invalid monitor/video-mode results leave the window unchanged.
2. `WindowSizeCallback()` updates the logical screen size even while fullscreen. Upstream 5.5 returned early here after updating the viewport, leaving `GetScreenHeight()` smaller than the actual projection; this caused an uncovered strip below the editor's status bar.
3. Fullscreen transitions clear stale render offsets. Logical coordinates are preserved; high-DPI physical viewport scaling is not used as an editor layout size.

Changed sections are marked `PRISME_FULLSCREEN_FIT`. Function-boundary checks reject incompatible upstream code; repeated configuration is idempotent. Both backend patches have a content fingerprint on the raylib target, so stale objects cannot silently keep the previous fullscreen code.

`cmake/FullscreenTestSources.cmake` extracts the actual patched fullscreen, resize, viewport and size-accessor functions into CPU-only regression tests. GLFW and rlgl side effects are mocked; the production backend functions are not replaced by a reimplementation. Tests reproduce the original 1,040/1,080 height mismatch (40 pixels uncovered), then check zero uncovered pixels, native video-mode selection, delayed callbacks, restoration, multiple monitors and the Retina viewport branch. No display server or preview is created.

## Full glyph bitmap extents (Prisme 1.4.2)

`cmake/PatchRaylibFont.cmake` modifies `src/rtext.c`, marked `PRISME_FONT_EXTENTS`:

1. A glyph taller than the nominal em height is not intrinsically invalid (the bundled Inter `|` is 35 px at a requested 34 px). That specific diagnostic is informational/debug instead of a warning; other font warnings and global log levels are unchanged.
2. Atlas area estimates, row advances and bottom-edge checks use the maximum actual glyph bitmap height, bounded below by the nominal font size. Glyph bitmaps, advances, offsets and the font base size are not cropped, rescaled or renamed.
3. When an atlas cannot package the remaining glyphs, the first failed glyph also gets an initialized empty rectangle.

The patch is checked against the pinned source, applied idempotently and included in the raylib object fingerprint along with the initialization/fullscreen corrections. It does not modify or redistribute altered font files.

The CPU-only typography regression calls the real `LoadFontData` and `GenImageFontAtlas`, using the application's 409 codepoints. It verifies all alpha pixels and rectangle bounds/non-overlap for 15 atlases: Inter, JetBrains Mono Medium and Regular at 17, 20, 34, 40 and 64 px. The original warning is reproduced in the previous version; the corrected tests preserve 1,819,614 alpha pixels with zero font warnings. No GPU context, exported image, desktop, preview or screenshot is used.
