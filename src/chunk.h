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

enum chunk_state { 
    C_STATE_EMPTY = 0, 
    C_STATE_GENERATING, 
    C_STATE_GENERATED, 
    C_STATE_MESHING, 
    C_STATE_MESHED 
};

enum chunk_flag {
    CHUNK_FLAG_DIRTY = 1 << 0,
    CHUNK_FLAG_ONLY_DATA = 1 << 1,
};

struct instance_data {
    ivec3 pos;
    col32_t tint;
};

typedef uint32_t light_t;

struct block_state {
    block_t type;
    light_t light;
};

struct chunk {
    int _canary;
    ivec3 pos;
    vec3 min, max;
    enum chunk_state state;
    enum chunk_flag flags;

    struct block_state *blocks;
    struct chunk *neighbours[6];
    int fill_count; // excludes the padded blocks

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
void tmp_floodfill_light(struct chunk *c, int x, int y, int z);

//
typedef struct {
    struct chunk *chunk;
    int x, y, z;
} iterator_t;

iterator_t iterator_make(struct chunk *c, int x, int y, int z);
void iterator_up(iterator_t *it);
void iterator_down(iterator_t *it);
void iterator_move(iterator_t *it, enum dir dir);
void iterator_update(iterator_t *it);
bool iterator_is_valid(iterator_t *it);
struct block_state *iterator_get_block_state_ptr(iterator_t *it);

// Helper functions for positions
static inline bool pos_inside(int x, int y, int z) {
    return (x >= 0 && x < C_SIZE && y >= 0 && y < C_SIZE && z >= 0 && z < C_SIZE);
}

static inline bool pos_inside_padded(int x, int y, int z) {
    return (x >= -1 && x <= (C_SIZE) && y >= -1 && y <= (C_SIZE) && z >= -1 && z <= (C_SIZE));
}

static inline bool pos_on_padding(int x, int y, int z) {
    return (x == -1 || x == C_SIZE || y == -1 || y == C_SIZE || z == -1 || z == C_SIZE);
}

static inline bool pos_on_border(int x, int y, int z) {
    return (x == 0 || x == (C_SIZE - 1) || y == 0 || y == (C_SIZE - 1) || z == 0 || z == (C_SIZE - 1));
}

#endif // CHUNK_H