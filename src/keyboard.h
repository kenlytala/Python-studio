#ifndef PYTHON_STUDIO_KEYBOARD_H
#define PYTHON_STUDIO_KEYBOARD_H
#include <stdbool.h>
#include <stddef.h>

#define KEYBOARD_KEYS 512
/* Backend key codes are physical QWERTY positions; characters are separate. */
typedef struct {
    unsigned short mapping[KEYBOARD_KEYS], inverse[26];
    bool ready, ctrl_down;
    double last_probe;
} Keyboard;
typedef struct {
    unsigned short key, windows_scan;
    char xkb_name[5];
} KeyboardSlot;
typedef bool (*KeyboardReader)(unsigned symbols[KEYBOARD_KEYS], void *context);

size_t keyboard_slots(const KeyboardSlot **slots);
bool keyboard_configure(Keyboard *keyboard, const unsigned symbols[KEYBOARD_KEYS]);
int keyboard_logical(const Keyboard *keyboard, int physical);
int keyboard_source(const Keyboard *keyboard, int logical);
void keyboard_poll(Keyboard *keyboard, bool control, bool activity, double now, KeyboardReader reader,
                   void *context);
/* UI platform provider; never loads/activates a layout or consumes dead keys. */
bool keyboard_native_read(unsigned symbols[KEYBOARD_KEYS], void *context);
#endif
