#ifndef CHUNK_H
#define CHUNK_H
#include "block.h"
#include "maf.h"
#include "utils.h"
#include <glad/gl.h>
#include <stdbool.h>

#define C_SIZE 32
#define C_SIZE3 (C_SIZE * C_SIZE * C_SIZE)
#define C_SIZEP (C_SIZE + 2)
#define C_SIZEP2 (C_SIZEP * C_SIZEP)
#define C_SIZEP3 (C_SIZEP * C_SIZEP * C_SIZEP)

// moved to .h only so that we can get it's size in world.c... for now
struct mesh_quad {
    u32 pos_face; // 3*6 bits voxel pos + 3 bits face + (2*4) bits AO + 1 bit flipped = 30 bits, 2 bits remaining
    u32 texid;
};

struct chunk {
    int _canary;
    int x, y, z;
    vec3 min, max;

    block_t blocks[C_SIZEP3];
    int fill_count; // excludes the padded blocks

    struct chunk *neighbours[6];

    // rendering stuff
    col32_t tint;
    int quad_count;
    int dirty;

    // move into a struct?
    u64 mdi_page;
    u64 mdi_size;
    void *mdiptr;
    bool mdi_valid;
};

void chunk_initialize(struct chunk *c, int x, int y, int z);
bool chunk_set_block(struct chunk *c, int x, int y, int z, block_t value);
block_t chunk_get_block(struct chunk *c, int x, int y, int z);

void chunk_set_neighbour(struct chunk *c, struct chunk *neighbour, int face);
size_t chunk_generate_mesh_data(struct chunk *c, void* buffer);

#endif // CHUNK_H