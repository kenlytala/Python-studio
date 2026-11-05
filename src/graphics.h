#ifndef PRISME_GRAPHICS_H
#define PRISME_GRAPHICS_H
#include <stdbool.h>

#define PRISME_GRAPHICS_ERROR 3

const char *graphics_version(void);
bool graphics_request_software(void);
bool graphics_open(int width, int height, const char *title, bool msaa, bool verbose);
#endif
