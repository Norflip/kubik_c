#ifndef UTILS_H
#define UTILS_H
#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

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


// directions
typedef struct {
    int x, y, z;
} ivec3;

enum dir { DIR_RIGHT = 0, DIR_LEFT, DIR_UP, DIR_DOWN, DIR_FRONT, DIR_BACK };
#define opposite_dir(i) (i + (!((i) % 2) ? 1 : -1))
static const ivec3 FACE_NORMALS[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

#endif // UTILS_H