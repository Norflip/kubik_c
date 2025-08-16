#ifndef UTILS_H
#define UTILS_H
#include <stdint.h>

#define u_align(x, a) (((x) + (a) - 1) / (a) * (a))
#define u_offsetof(T, _m) ((size_t)&((T *)0)->_m)

typedef uint32_t col32_t;
#define C32_WHITE 0xFFFFFFFF
#define C32_BLACK 0x000000FF

#define C32_RED 0xFF0000FF
#define C32_GREEN 0x00FF00FF
#define C32_BLUE 0x0000FFFF
#define C32_YELLOW 0xFFCC00FF
#define C32_CYAN 0x00FFFFFF
#define C32_MAGENTA 0xFF00FFFF

#endif // UTILS_H