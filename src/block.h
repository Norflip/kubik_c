#ifndef BLOCKS_H
#define BLOCKS_H
#include "load.h"
#include "utils.h"
#include <stdbool.h>
#include <stdint.h>

#define NO_TEXTURE_INDEX -1

typedef uint8_t block_t;
enum { BLK_AIR = 0, BLK_BASE, BLK_STONE, BLK_DIRT, BLK_GRASS, BLK_SAND, BLK_OCEAN, BLK_COUNT };

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
    case BLK_SAND:
        return 5;
    case BLK_OCEAN:
        return 6;
    }
    return NO_TEXTURE_INDEX;
}

static const char **block_texture_paths(int *out_count) {
    static const char *paths[BLK_COUNT] = {
        asset_path("blocks/base.png"),       // 0
        asset_path("blocks/stone.png"),      // 1
        asset_path("blocks/dirt.png"),       // 2
        asset_path("blocks/grass_top.png"),  // 3
        asset_path("blocks/grass_side.png"), // 4
        asset_path("blocks/sand.png"),       // 5
        asset_path("blocks/PH_water.png"),   // 6
    };
    *out_count = 7;
    return paths;
}

static u32 block_default_light(block_t b) {
    return 0;
}

static bool block_is_fluid(block_t b) {
    return b == BLK_OCEAN;
}

static bool block_create_face(block_t b, int face, block_t n) {

    if (block_is_fluid(n))
        return !block_is_fluid(b);

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