/* Deterministic test data only, NEVER used to detect the user's layout. */
#ifndef PRISME_TEST_KEYBOARD_LAYOUTS_H
#define PRISME_TEST_KEYBOARD_LAYOUTS_H
#include "keyboard.h"
#include <string.h>
static void keyboard_test_layout(unsigned symbols[KEYBOARD_KEYS], int layout) {
    memset(symbols, 0, sizeof(*symbols) * KEYBOARD_KEYS);
    for (int key = 'A'; key <= 'Z'; key++)
        symbols[key] = (unsigned)key + 32;
    if (layout == 1) {
        symbols['A'] = 'q';
        symbols['Q'] = 'a';
        symbols['W'] = 'z';
        symbols['Z'] = 'w';
        symbols['M'] = ',';
        symbols[';'] = 'm';
    } else if (layout == 2) {
        symbols['Y'] = 'z';
        symbols['Z'] = 'y';
    } else if (layout == 3) {
        const char *physical[] = {"QWERTYUIOP[]", "ASDFGHJKL;'", "ZXCVBNM,./"};
        const char *logical[] = {"',.pyfgcrl/=", "aoeuidhtns-", ";qjkxbmwvz"};
        memset(symbols, 0, sizeof(*symbols) * KEYBOARD_KEYS);
        for (int row = 0; row < 3; row++)
            for (size_t i = 0; physical[row][i]; i++)
                symbols[(unsigned char)physical[row][i]] = (unsigned char)logical[row][i];
    }
}
#endif
