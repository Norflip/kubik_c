#ifndef BLOCKS_H
#define BLOCKS_H
#include <stdbool.h>
#include <stdint.h>
#include "utils.h"

typedef uint8_t block_t;
#define NO_TEXTURE_INDEX -1
enum { BLK_AIR = 0, BLK_BASE, BLK_STONE, BLK_DIRT, BLK_GRASS, BLK_COUNT };

static inline int block_is_solid(block_t b) {
    return b != BLK_AIR;
}

static int block_texture_index(block_t b, int face) {
    (void)(face);
    switch (b) {
    case BLK_BASE:
        return 0;
    case BLK_STONE:
        return 1;
    case BLK_DIRT:
        return 2;
    case BLK_GRASS:
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
    case BLK_AIR:
        return "Air";
    case BLK_BASE:
        return "Base";
    case BLK_STONE:
        return "Stone";
    case BLK_DIRT:
        return "Dirt";
    case BLK_GRASS:
        return "Grass";
    default:
        return "Unknown";
    }
}

#endif // BLOCKS_H