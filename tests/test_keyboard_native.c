#define _POSIX_C_SOURCE 200809L
/* The actual XKB provider linked to readonly Xlib doubles, never an X server. */
#include "keyboard.h"
#include "test_keyboard_layouts.h"
#include <X11/XKBlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks, opened, closed, freed, fail, active;
static bool alias_mode, unicode_mode;
static unsigned groups[4][KEYBOARD_KEYS], raw_by_code[256];
static XkbKeyNameRec key_names[256];
static XkbKeyAliasRec aliases[1];
static XkbNamesRec names;
static XkbDescRec desc;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        checks++;                                                                                            \
        if (!(c)) {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c);                                                  \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
Display *XOpenDisplay(const char *name) {
    CHECK(!name);
    opened++;
    return fail == 1 ? NULL : (Display *)(uintptr_t)123;
}
int XCloseDisplay(Display *display) {
    CHECK(display == (Display *)(uintptr_t)123);
    closed++;
    return 0;
}
XkbDescPtr XkbGetMap(Display *display, unsigned mask, unsigned device) {
    CHECK(display == (Display *)(uintptr_t)123 && !mask && device == XkbUseCoreKbd);
    return fail == 2 ? NULL : &desc;
}
Status XkbGetNames(Display *display, unsigned mask, XkbDescPtr description) {
    CHECK(display == (Display *)(uintptr_t)123 && mask == (XkbKeyNamesMask | XkbKeyAliasesMask) &&
          description == &desc);
    return fail == 3 ? 1 : Success;
}
Status XkbGetState(Display *display, unsigned device, XkbStatePtr state) {
    CHECK(display == (Display *)(uintptr_t)123 && device == XkbUseCoreKbd);
    state->group = (unsigned char)active;
    return fail == 4 ? 1 : Success;
}
KeySym XkbKeycodeToKeysym(Display *display, KeyCode code, int group, int level) {
    CHECK(display == (Display *)(uintptr_t)123 && group >= 0 && group < 4 && !level);
    unsigned symbol = groups[group][raw_by_code[code]];
    return unicode_mode && symbol ? (KeySym)(symbol | 0x01000000u) : symbol;
}
void XkbFreeKeyboard(XkbDescPtr description, unsigned which, Bool all) {
    CHECK(description == &desc && !which && all);
    freed++;
}
static void prepare(void) {
    memset(groups, 0, sizeof(groups));
    memset(key_names, 0, sizeof(key_names));
    memset(aliases, 0, sizeof(aliases));
    memset(&names, 0, sizeof(names));
    memset(&desc, 0, sizeof(desc));
    memset(raw_by_code, 0, sizeof(raw_by_code));
    keyboard_test_layout(groups[0], 1);
    const KeyboardSlot *slots = NULL;
    size_t count = keyboard_slots(&slots);
    for (size_t i = 0; i < count; i++) {
        unsigned code = 17 + (unsigned)i * 3; /* Intentionally not evdev/GLFW scancodes. */
        memcpy(key_names[code].name, slots[i].xkb_name, 4);
        raw_by_code[code] = (unsigned)slots[i].key;
        if (alias_mode && slots[i].key == 'W') {
            memcpy(key_names[code].name, "TEST", 4);
            memcpy(aliases[0].real, "TEST", 4);
            memcpy(aliases[0].alias, "AD02", 4);
        }
    }
    names.keys = key_names;
    names.key_aliases = aliases;
    names.num_key_aliases = (unsigned char)alias_mode;
    desc.names = &names;
    desc.min_key_code = 8;
    desc.max_key_code = 250;
    opened = closed = freed = fail = active = 0;
    unicode_mode = false;
}
static void french(void) {
    unsigned symbols[KEYBOARD_KEYS] = {0};
    CHECK(keyboard_native_read(symbols, NULL));
    Keyboard k = {0};
    CHECK(keyboard_configure(&k, symbols));
    CHECK(keyboard_source(&k, 'Z') == 'W' && keyboard_source(&k, 'W') == 'Z' &&
          keyboard_source(&k, 'M') == ';');
    CHECK(opened == 1 && closed == 1 && freed == 1);
}
static int abi(void) {
#define MEMBER(type, field) printf(",\"" #field "\":%zu", offsetof(type, field))
    printf("{\"XkbState\":{\"size\":%zu", sizeof(XkbStateRec));
    MEMBER(XkbStateRec, group);
    MEMBER(XkbStateRec, ptr_buttons);
    printf("},\"XkbKeyName\":{\"size\":%zu},\"XkbAlias\":{\"size\":%zu", sizeof(XkbKeyNameRec),
           sizeof(XkbKeyAliasRec));
    MEMBER(XkbKeyAliasRec, real);
    MEMBER(XkbKeyAliasRec, alias);
    printf("},\"XkbNames\":{\"size\":%zu", sizeof(XkbNamesRec));
    MEMBER(XkbNamesRec, keys);
    MEMBER(XkbNamesRec, key_aliases);
    MEMBER(XkbNamesRec, num_key_aliases);
    MEMBER(XkbNamesRec, phys_symbols);
    printf("},\"XkbDesc\":{\"size\":%zu", sizeof(XkbDescRec));
    MEMBER(XkbDescRec, min_key_code);
    MEMBER(XkbDescRec, max_key_code);
    MEMBER(XkbDescRec, names);
    MEMBER(XkbDescRec, map);
    puts("}}");
    return 0;
#undef MEMBER
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--abi"))
        return abi();
    const char *prior = getenv("DISPLAY");
    char *old = prior ? strdup(prior) : NULL;
    CHECK(!prior || old);
    CHECK(!setenv("DISPLAY", "readonly-mocked-no-server", 1));
    prepare();
    french();
    alias_mode = true;
    prepare();
    french();
    alias_mode = false;
    prepare();
    keyboard_test_layout(groups[0], 0);
    keyboard_test_layout(groups[1], 1);
    active = 1;
    french();
    prepare();
    for (unsigned i = 0; i < KEYBOARD_KEYS; i++)
        groups[2][i] = 0x01000627;
    active = 2;
    french();
    prepare();
    unicode_mode = true;
    french();
    prepare();
    fail = 4;
    french(); /* Missing group state safely reads group zero. */
    for (int failure = 1; failure <= 3; failure++) {
        prepare();
        fail = failure;
        unsigned symbols[KEYBOARD_KEYS] = {0};
        CHECK(!keyboard_native_read(symbols, NULL));
        CHECK(opened == 1 && closed == (failure != 1) && freed == (failure == 3));
    }
    for (int invalid = 0; invalid < 4; invalid++) {
        prepare();
        if (!invalid)
            desc.names = NULL;
        else if (invalid == 1)
            names.keys = NULL;
        else if (invalid == 2) {
            desc.min_key_code = 100;
            desc.max_key_code = 20;
        } else
            memset(groups, 0, sizeof(groups));
        unsigned symbols[KEYBOARD_KEYS] = {0};
        CHECK(!keyboard_native_read(symbols, NULL));
        CHECK(opened == 1 && closed == 1 && freed == 1);
    }
    prepare();
    CHECK(!setenv("DISPLAY", "", 1));
    unsigned symbols[KEYBOARD_KEYS] = {0};
    CHECK(!keyboard_native_read(symbols, NULL) && !opened && !closed && !freed);
    if (old) {
        CHECK(!setenv("DISPLAY", old, 1));
        free(old);
    } else
        CHECK(!unsetenv("DISPLAY"));
    printf("OK %d actual readonly XKB provider checks: current/fallback groups, names, aliases, non-evdev "
           "keycodes, Unicode, failure cleanup; simulated Xlib only\n",
           checks);
    return 0;
}
