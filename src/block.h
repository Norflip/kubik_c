#ifndef BLOCKS_H
#define BLOCKS_H
#include <stdbool.h>
#include <stdint.h>
#include "utils.h"

typedef uint8_t block_t;
#define NO_TEXTURE_INDEX -1
enum { BLOCK_AIR = 0, BLOCK_BASE, BLOCK_STONE, BLOCK_DIRT, BLOCK_GRASS, BLOCK_COUNT };

static inline int block_is_solid(block_t b) {
    return b != BLOCK_AIR;
}

static int block_texture_index(block_t b, int face) {
    (void)(face);
    switch (b) {
    case BLOCK_BASE:
        return 0;
    case BLOCK_STONE:
        return 1;
    case BLOCK_DIRT:
        return 2;
    case BLOCK_GRASS:
        if (face == DIR_UP)
            return 3;
        else if (face == DIR_DOWN)
            return 2;
        return 4;
    }
    return NO_TEXTURE_INDEX;
}

static bool block_create_face(block_t b, int face, block_t n) {
    return block_is_solid(b) && !block_is_solid(n);
}

static const char *block_name(block_t b) {
    switch (b) {
    case BLOCK_AIR:
        return "Air";
    case BLOCK_BASE:
        return "Base";
    case BLOCK_STONE:
        return "Stone";
    case BLOCK_DIRT:
        return "Dirt";
    default:
        return "Unknown";
    }
}

#endif // BLOCKS_H