#ifndef CHUNK_H
#define CHUNK_H
#include "block.h"
#include "maf.h"
#include "mesh.h"
#include "thread.h"
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
    u32 light;
    u32 _padding;
};

enum chunk_state { C_STATE_EMPTY = 0, C_STATE_GENERATING, C_STATE_GENERATED, C_STATE_MESHING, C_STATE_MESHED };

enum chunk_flag {
    CHUNK_FLAG_DIRTY = 1 << 0,
    CHUNK_FLAG_ONLY_DATA = 1 << 1,
};

struct instance_data {
    ivec3 pos;
    col32_t tint;
};

struct block_state {
    block_t type;
    u32 light;
};

struct chunk {
    int _canary;
    ivec3 pos;
    vec3 min, max;
    enum chunk_state state;
    enum chunk_flag flags;

    struct block_state *blocks;
    int fill_count; // excludes the padded blocks
    struct chunk *neighbours[6];

    // rendering stuff
    mesh_handle_t mesh_handle;
    col32_t tint;
    int quad_count;
    int dirty;

    mutex_t lock;
};

void chunk_initialize(struct chunk *c, ivec3 pos, enum chunk_flag flags);
bool chunk_set_block(struct chunk *c, int x, int y, int z, block_t value);
block_t chunk_get_block(struct chunk *c, int x, int y, int z);
bool chunk_block_is_solid(struct chunk *c, int x, int y, int z);

ivec3 chunk_world_pos(struct chunk *c);
uint32_t chunk_seed(struct chunk *c);

void chunk_set_neighbour(struct chunk *c, struct chunk *neighbour, int face);
size_t chunk_generate_mesh_data(struct chunk *c, void *buffer);
size_t chunk_generate_face_mesh_data(struct chunk *c, int face, void *buffer);

#endif // CHUNK_H