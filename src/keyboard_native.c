#include "keyboard.h"
#include <stdlib.h>
#include <string.h>

#if defined(PRISME_KEYBOARD_X11)
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

static unsigned keycode_named(XkbDescPtr desc, const char *name) {
    for (unsigned code = desc->min_key_code; code <= desc->max_key_code; code++)
        if (!memcmp(desc->names->keys[code].name, name, 4))
            return code;
    if (desc->names->key_aliases)
        for (unsigned i = 0; i < desc->names->num_key_aliases; i++)
            if (!memcmp(desc->names->key_aliases[i].alias, name, 4))
                for (unsigned code = desc->min_key_code; code <= desc->max_key_code; code++)
                    if (!memcmp(desc->names->keys[code].name, desc->names->key_aliases[i].real, 4))
                        return code;
    return 0;
}
#elif defined(_WIN32)
#include <windows.h>
#endif

bool keyboard_native_read(unsigned symbols[KEYBOARD_KEYS], void *context) {
    (void)context;
    const KeyboardSlot *slots = NULL;
    size_t count = keyboard_slots(&slots);
#if defined(PRISME_KEYBOARD_X11)
    const char *environment = getenv("DISPLAY");
    if (!environment || !*environment)
        return false;
    Display *display = XOpenDisplay(NULL);
    if (!display)
        return false;
    XkbDescPtr desc = XkbGetMap(display, 0, XkbUseCoreKbd);
    bool success = false;
    if (desc && !XkbGetNames(display, XkbKeyNamesMask | XkbKeyAliasesMask, desc) && desc->names &&
        desc->names->keys && desc->min_key_code <= desc->max_key_code) {
        unsigned codes[64] = {0};
        if (count > sizeof(codes) / sizeof(*codes))
            goto cleanup;
        for (size_t i = 0; i < count; i++)
            codes[i] = keycode_named(desc, slots[i].xkb_name);
        XkbStateRec state = {0};
        unsigned active = XkbGetState(display, XkbUseCoreKbd, &state) == Success ? state.group % 4 : 0;
        for (unsigned attempt = 0; attempt < 5; attempt++) {
            unsigned group = attempt ? attempt - 1 : active;
            if (attempt && group == active)
                continue;
            memset(symbols, 0, sizeof(*symbols) * KEYBOARD_KEYS);
            for (size_t i = 0; i < count; i++)
                if (codes[i]) {
                    KeySym symbol = XkbKeycodeToKeysym(display, (KeyCode)codes[i], (int)group, 0);
                    if ((symbol & 0xff000000ul) == 0x01000000ul)
                        symbol &= 0xfffffful;
                    symbols[slots[i].key] = (unsigned)symbol;
                }
            Keyboard candidate = {0};
            if (keyboard_configure(&candidate, symbols)) {
                success = true;
                break;
            }
        }
    }
cleanup:
    if (desc)
        XkbFreeKeyboard(desc, 0, True);
    XCloseDisplay(display);
    return success;
#elif defined(_WIN32)
    /* MapVirtualKey is read-only; do not use ToUnicode/ActivateKeyboardLayout. */
    HKL layouts[17] = {GetKeyboardLayout(0)};
    int installed = GetKeyboardLayoutList(16, layouts + 1);
    if (installed < 0)
        installed = 0;
    if (installed > 16)
        installed = 16;
    for (int j = 0; j <= installed; j++) {
        memset(symbols, 0, sizeof(*symbols) * KEYBOARD_KEYS);
        for (size_t i = 0; i < count; i++) {
            UINT virtual_key = MapVirtualKeyExW(slots[i].windows_scan, MAPVK_VSC_TO_VK_EX, layouts[j]);
            UINT symbol = virtual_key ? MapVirtualKeyExW(virtual_key, MAPVK_VK_TO_CHAR, layouts[j]) : 0;
            if (!(symbol & 0x80000000u))
                symbols[slots[i].key] = symbol;
        }
        Keyboard candidate = {0};
        if (keyboard_configure(&candidate, symbols))
            return true;
    }
    return false;
#elif defined(__APPLE__)
    /* These are provided by the statically linked GLFW in the native edition. */
    extern const char *glfwGetKeyName(int key, int scancode);
    extern int glfwGetKeyScancode(int key);
    memset(symbols, 0, sizeof(*symbols) * KEYBOARD_KEYS);
    for (size_t i = 0; i < count; i++) {
        const char *name = glfwGetKeyName(slots[i].key, glfwGetKeyScancode(slots[i].key));
        if (name && name[0] && !name[1])
            symbols[slots[i].key] = (unsigned char)name[0];
    }
    Keyboard candidate = {0};
    return keyboard_configure(&candidate, symbols);
#else
    (void)slots;
    (void)count;
    (void)symbols;
    return false;
#endif
}
