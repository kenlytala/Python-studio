#include "keyboard.h"
#include <string.h>
static const KeyboardSlot slots[] = {
    {49, 0x02, "AE01"}, {50, 0x03, "AE02"}, {51, 0x04, "AE03"},  {52, 0x05, "AE04"}, {53, 0x06, "AE05"},
    {54, 0x07, "AE06"}, {55, 0x08, "AE07"}, {56, 0x09, "AE08"},  {57, 0x0a, "AE09"}, {48, 0x0b, "AE10"},
    {45, 0x0c, "AE11"}, {61, 0x0d, "AE12"}, {81, 0x10, "AD01"},  {87, 0x11, "AD02"}, {69, 0x12, "AD03"},
    {82, 0x13, "AD04"}, {84, 0x14, "AD05"}, {89, 0x15, "AD06"},  {85, 0x16, "AD07"}, {73, 0x17, "AD08"},
    {79, 0x18, "AD09"}, {80, 0x19, "AD10"}, {91, 0x1a, "AD11"},  {93, 0x1b, "AD12"}, {65, 0x1e, "AC01"},
    {83, 0x1f, "AC02"}, {68, 0x20, "AC03"}, {70, 0x21, "AC04"},  {71, 0x22, "AC05"}, {72, 0x23, "AC06"},
    {74, 0x24, "AC07"}, {75, 0x25, "AC08"}, {76, 0x26, "AC09"},  {59, 0x27, "AC10"}, {39, 0x28, "AC11"},
    {90, 0x2c, "AB01"}, {88, 0x2d, "AB02"}, {67, 0x2e, "AB03"},  {86, 0x2f, "AB04"}, {66, 0x30, "AB05"},
    {78, 0x31, "AB06"}, {77, 0x32, "AB07"}, {44, 0x33, "AB08"},  {46, 0x34, "AB09"}, {47, 0x35, "AB10"},
    {96, 0x29, "TLDE"}, {92, 0x2b, "BKSL"}, {161, 0x56, "LSGT"}, {32, 0x39, "SPCE"},
};
size_t keyboard_slots(const KeyboardSlot **out) {
    if (out)
        *out = slots;
    return sizeof(slots) / sizeof(*slots);
}
static unsigned letter(unsigned value) {
    return value >= 97 && value <= 122 ? value - 32 : value >= 65 && value <= 90 ? value : 0;
}
bool keyboard_configure(Keyboard *keyboard, const unsigned symbols[KEYBOARD_KEYS]) {
    if (!keyboard || !symbols)
        return false;
    unsigned short mapping[KEYBOARD_KEYS], inverse[26] = {0};
    unsigned count = 0;
    bool printable[KEYBOARD_KEYS] = {0};
    for (size_t i = 0; i < sizeof(slots) / sizeof(*slots); i++)
        printable[slots[i].key] = true;
    for (unsigned key = 0; key < KEYBOARD_KEYS; key++) {
        mapping[key] = (unsigned short)key;
        unsigned value = letter(symbols[key]);
        if (value) {
            if (!printable[key] || inverse[value - 65])
                return false;
            inverse[value - 65] = (unsigned short)key;
            mapping[key] = (unsigned short)value;
            count++;
        } else if (key >= 65 && key <= 90)
            mapping[key] = 0;
    }
    if (count != 26)
        return false;
    memcpy(keyboard->mapping, mapping, sizeof(mapping));
    memcpy(keyboard->inverse, inverse, sizeof(inverse));
    keyboard->ready = true;
    return true;
}
int keyboard_logical(const Keyboard *keyboard, int physical) {
    if (physical <= 0 || physical >= KEYBOARD_KEYS)
        return 0;
    return keyboard && keyboard->ready ? keyboard->mapping[physical] : physical;
}
int keyboard_source(const Keyboard *keyboard, int logical) {
    if (logical <= 0 || logical >= KEYBOARD_KEYS)
        return -1;
    if (!keyboard || !keyboard->ready)
        return logical;
    if (logical >= 65 && logical <= 90)
        return keyboard->inverse[logical - 65];
    return keyboard->mapping[logical] == logical ? logical : -1;
}
void keyboard_poll(Keyboard *keyboard, bool control, bool activity, double now, KeyboardReader reader,
                   void *ctx) {
    if (control && (activity || !keyboard->ctrl_down || now - keyboard->last_probe >= .75)) {
        unsigned symbols[KEYBOARD_KEYS] = {0};
        keyboard->last_probe = now;
        if (reader && reader(symbols, ctx))
            keyboard_configure(keyboard, symbols);
    }
    keyboard->ctrl_down = control;
}
