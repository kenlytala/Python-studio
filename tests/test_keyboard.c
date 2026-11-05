#include "keyboard.h"
#include "test_keyboard_layouts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks, probes;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        checks++;                                                                                            \
        if (!(c)) {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c);                                                  \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static bool reader(unsigned symbols[KEYBOARD_KEYS], void *ctx) {
    probes++;
    int layout = *(int *)ctx;
    if (layout < 0)
        return false;
    keyboard_test_layout(symbols, layout);
    return true;
}
static int oracle(void) {
    printf("{\"layouts\":[");
    for (int layout = 0; layout < 4; layout++) {
        unsigned symbols[KEYBOARD_KEYS];
        keyboard_test_layout(symbols, layout);
        Keyboard k = {0};
        if (!keyboard_configure(&k, symbols))
            return 2;
        printf("%s{\"logical\":[", layout ? "," : "");
        for (int key = 0; key < KEYBOARD_KEYS; key++)
            printf("%s%d", key ? "," : "", keyboard_logical(&k, key));
        printf("],\"source\":[");
        for (int key = 0; key < KEYBOARD_KEYS; key++)
            printf("%s%d", key ? "," : "", keyboard_source(&k, key));
        printf("]}");
    }
    puts("]}");
    return 0;
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--oracle"))
        return oracle();
    Keyboard k = {0};
    unsigned symbols[KEYBOARD_KEYS];
    for (int key = 1; key < KEYBOARD_KEYS; key++) {
        CHECK(keyboard_logical(&k, key) == key && keyboard_source(&k, key) == key);
        CHECK(keyboard_logical(NULL, key) == key && keyboard_source(NULL, key) == key);
    }
    const int bad[] = {-1000, -1, 0, 512, 5000};
    for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); i++) {
        CHECK(!keyboard_logical(&k, bad[i]));
        CHECK(keyboard_source(&k, bad[i]) == -1);
    }
    CHECK(!keyboard_configure(NULL, symbols) && !keyboard_configure(&k, NULL));
    const KeyboardSlot *slots = NULL;
    size_t count = keyboard_slots(&slots);
    CHECK(count == 49 && keyboard_slots(NULL) == 49 && slots);
    for (size_t i = 0; i < count; i++)
        for (size_t j = 0; j < i; j++) {
            CHECK(slots[i].key != slots[j].key && slots[i].windows_scan != slots[j].windows_scan);
            CHECK(memcmp(slots[i].xkb_name, slots[j].xkb_name, 4));
        }
    const int undo[] = {90, 87, 89, 47}, close[] = {87, 90, 87, 44};
    for (int layout = 0; layout < 4; layout++) {
        keyboard_test_layout(symbols, layout);
        CHECK(keyboard_configure(&k, symbols));
        for (int letter = 'A'; letter <= 'Z'; letter++)
            CHECK(keyboard_logical(&k, keyboard_source(&k, letter)) == letter);
        CHECK(keyboard_source(&k, 'Z') == undo[layout] && keyboard_source(&k, 'W') == close[layout]);
        const int fixed[] = {256, 257, 258, 294, 300, 340, 341, 342, 346};
        for (size_t i = 0; i < sizeof(fixed) / sizeof(*fixed); i++)
            CHECK(keyboard_source(&k, fixed[i]) == fixed[i] && keyboard_logical(&k, fixed[i]) == fixed[i]);
    }
    keyboard_test_layout(symbols, 1);
    CHECK(keyboard_configure(&k, symbols));
    CHECK(!keyboard_logical(&k, 'M') && keyboard_source(&k, 'M') == ';' && keyboard_source(&k, ';') == -1);
    Keyboard before = k;
    for (int invalid = 0; invalid < 4; invalid++) {
        keyboard_test_layout(symbols, 0);
        if (!invalid)
            memset(symbols, 0, sizeof(symbols));
        else if (invalid == 1)
            symbols['Z'] = 0;
        else if (invalid == 2)
            symbols['Z'] = 'w';
        else {
            symbols[300] = symbols['Z'];
            symbols['Z'] = 0;
        }
        CHECK(!keyboard_configure(&k, symbols));
        CHECK(!memcmp(&k, &before, sizeof(k)));
    }
    unsigned state = 1160;
    for (int round = 0; round < 200; round++) {
        int positions[64];
        for (size_t i = 0; i < count; i++)
            positions[i] = slots[i].key;
        for (size_t i = count - 1; i > 0; i--) {
            state = state * 1664525u + 1013904223u;
            size_t j = state % (i + 1);
            int t = positions[i];
            positions[i] = positions[j];
            positions[j] = t;
        }
        memset(symbols, 0, sizeof(symbols));
        for (unsigned i = 0; i < 26; i++)
            symbols[positions[i]] = 97 + i;
        CHECK(keyboard_configure(&k, symbols));
        for (int letter = 'A'; letter <= 'Z'; letter++) {
            CHECK(keyboard_source(&k, letter) == positions[letter - 'A']);
            CHECK(keyboard_logical(&k, positions[letter - 'A']) == letter);
        }
    }
    memset(&k, 0, sizeof(k));
    int layout = 1;
    keyboard_poll(&k, false, true, 1, reader, &layout);
    CHECK(!probes && !k.ready);
    keyboard_poll(&k, true, false, 1.1, reader, &layout);
    CHECK(probes == 1 && keyboard_source(&k, 'Z') == 'W');
    keyboard_poll(&k, true, false, 1.2, reader, &layout);
    CHECK(probes == 1);
    layout = 2;
    keyboard_poll(&k, true, true, 1.3, reader, &layout);
    CHECK(probes == 2 && keyboard_source(&k, 'Z') == 'Y');
    layout = 1;
    keyboard_poll(&k, true, false, 2.1, reader, &layout);
    CHECK(probes == 3 && keyboard_source(&k, 'Z') == 'W');
    layout = -1;
    keyboard_poll(&k, true, true, 2.2, reader, &layout);
    CHECK(probes == 4 && keyboard_source(&k, 'Z') == 'W');
    keyboard_poll(&k, false, false, 2.3, reader, &layout);
    keyboard_poll(&k, true, false, 2.4, reader, &layout);
    CHECK(probes == 5 && keyboard_source(&k, 'Z') == 'W');
    printf("OK %d keyboard model checks: 4 layouts, 49 slots, inverse state, 200 permutations, atomic "
           "rejection, live refresh; no OS mutation/window\n",
           checks);
    return 0;
}
