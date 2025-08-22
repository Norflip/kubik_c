#ifndef BLOCKS_H
#define BLOCKS_H
#include <stdint.h>

typedef uint8_t block_t;
#define NO_TEXTURE_INDEX -1

enum { BLOCK_AIR = 0, BLOCK_BASE = 1, BLOCK_STONE = 2, BLOCK_DIRT = 3, BLOCK_COUNT };

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
    }

    return NO_TEXTURE_INDEX;
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