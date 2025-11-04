#include "chunk.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define C_CANARY 0xBABEDAD

static bool inside(int x, int y, int z) {
    return (x >= 0 && x < C_SIZE && y >= 0 && y < C_SIZE && z >= 0 && z < C_SIZE);
}

static bool inside_padded(int x, int y, int z) {
    return (x >= -1 && x <= (C_SIZE) && y >= -1 && y <= (C_SIZE) && z >= -1 && z <= (C_SIZE));
}

static bool on_padding(int x, int y, int z) {
    return (x == -1 || x == C_SIZE || y == -1 || y == C_SIZE || z == -1 || z == C_SIZE);
}

static bool on_border(int x, int y, int z) {
    return (x == 0 || x == (C_SIZE - 1) || y == 0 || y == (C_SIZE - 1) || z == 0 || z == (C_SIZE - 1));
}

#define ASSERT_CHUNK(c) \
    assert(c != NULL);  \
    assert(c->_canary == C_CANARY)

void chunk_initialize(struct chunk *c, ivec3 pos, enum chunk_flag flags) {
    memset(c, 0, sizeof(struct chunk));
    c->pos = pos;
    c->_canary = C_CANARY;
    c->tint = C32_CLEAR;

    c->min = (vec3){c->pos.x * C_SIZE, c->pos.y * C_SIZE, c->pos.z * C_SIZE};
    c->max = v3_addf(c->min, C_SIZE);

    c->blocks = NULL;
    c->state = C_STATE_EMPTY;
    c->flags = flags;
    mutex_init(&c->lock);
}

static struct chunk *get_neighbour(struct chunk *c, int face) {
    if (c == NULL || face < 0 || face >= 6)
        return NULL;
    return c->neighbours[face];
}

static inline struct chunk *find_neighbour(struct chunk *chunk, int dx, int dy, int dz) {
    struct chunk *current = chunk;

    if (dx > 0)
        current = current->neighbours[DIR_RIGHT];
    else if (dx < 0)
        current = current->neighbours[DIR_LEFT];

    if (!current)
        return NULL;

    if (dy > 0)
        current = current->neighbours[DIR_UP];
    else if (dy < 0)
        current = current->neighbours[DIR_DOWN];

    if (!current)
        return NULL;

    if (dz > 0)
        current = current->neighbours[DIR_FRONT];
    else if (dz < 0)
        current = current->neighbours[DIR_BACK];

    return current;
}

static bool try_alloc_block_data(struct chunk *c) {
    ASSERT_CHUNK(c);
    mutex_lock(&c->lock);

    if (c->blocks != NULL) {

        mutex_unlock(&c->lock);

        return true;
    }

    c->blocks = calloc(C_SIZEP3, sizeof(struct block_state));
    if (c->blocks == NULL) {
        fprintf(stderr, "chunk_set_block: failed to allocate memory for chunk blocks\n");
        mutex_unlock(&c->lock);
        return false;
    }

    mutex_unlock(&c->lock);
    return true;
}

static void set_padded_blockstate(struct chunk *c, int x, int y, int z, struct block_state value) {

    if (!on_border(x, y, z))
        return;

    // seems a bit overkill to make a bitmask for this tbh
    uint8_t boundary_mask = 0;
    if (x == C_SIZE - 1)
        boundary_mask |= (1 << DIR_RIGHT); // +X boundary
    if (x == 0)
        boundary_mask |= (1 << DIR_LEFT); // -X boundary
    if (y == C_SIZE - 1)
        boundary_mask |= (1 << DIR_UP); // +Y boundary
    if (y == 0)
        boundary_mask |= (1 << DIR_DOWN); // -Y boundary
    if (z == C_SIZE - 1)
        boundary_mask |= (1 << DIR_FRONT); // +Z boundary
    if (z == 0)
        boundary_mask |= (1 << DIR_BACK); // -Z boundary

    struct chunk *n;
    for (int face = 0; face < 6; face++) {
        n = c->neighbours[face];
        if ((boundary_mask & (1 << face)) && n) {
            int tx = x;
            int ty = y;
            int tz = z;

            // Adjust target coordinate for the specific face, slow?
            switch (face) {
            case DIR_RIGHT:
                tx = -1;
                break; // +X face
            case DIR_LEFT:
                tx = C_SIZE;
                break; // -X face
            case DIR_UP:
                ty = -1;
                break; // +Y face
            case DIR_DOWN:
                ty = C_SIZE;
                break; // -Y face
            case DIR_FRONT:
                tz = -1;
                break; // +Z face
            case DIR_BACK:
                tz = C_SIZE;
                break; // -Z face
            }

            if (value.type != BLK_AIR && c->blocks != NULL) {
                if (try_alloc_block_data(n)) {
                    int idx = u_index_zxy(tx + 1, ty + 1, tz + 1, C_SIZEP);
                    n->blocks[idx] = value;
                    n->dirty = true;
                }
            }
        }
    }
}

bool chunk_set_block(struct chunk *c, int x, int y, int z, block_t value) {
    ASSERT_CHUNK(c);
    if (!inside(x, y, z)) {
        fprintf(stderr, "chunk_set_block: position (%d, %d, %d) is out of bounds in chunk (%d, %d, %d)\n", x, y, z, c->pos.x, c->pos.y, c->pos.z);
        return false;
    }

    if (value == BLK_AIR && c->blocks == NULL)
        return true;

    if (!try_alloc_block_data(c)) {
        return false;
    }

    const int index = u_index_zxy(x + 1, y + 1, z + 1, C_SIZEP);
    block_t old_value = c->blocks[index].type;
    if (old_value != 0 && value == 0) {
        c->fill_count--;
    } else if (old_value == 0 && value != 0) {
        c->fill_count++;
    }

    struct block_state new_state = {.type = value, .light = 0xFF};
    c->blocks[index] = new_state;
    c->dirty = true;
    set_padded_blockstate(c, x, y, z, new_state);
    return true;
}

block_t chunk_get_block(struct chunk *c, int x, int y, int z) {
    ASSERT_CHUNK(c);
    if (!inside_padded(x, y, z))
        return BLK_AIR;
    if (c->blocks == NULL)
        return BLK_AIR;
    return c->blocks[u_index_zxy(x + 1, y + 1, z + 1, C_SIZEP)].type;
}

bool chunk_block_is_solid(struct chunk *c, int x, int y, int z) {
    return chunk_get_block(c, x, y, z) != BLK_AIR;
}

bool chunk_set_light(struct chunk *c, int x, int y, int z, u32 value) {
    ASSERT_CHUNK(c);
    if (!inside(x, y, z)) {
        fprintf(stderr, "chunk_set_light: position (%d, %d, %d) is out of bounds in chunk (%d, %d, %d)\n", x, y, z, c->pos.x, c->pos.y, c->pos.z);
        return false;
    }

    return false;
}

ivec3 chunk_world_pos(struct chunk *c) {
    ASSERT_CHUNK(c);
    return (ivec3){c->pos.x * C_SIZE, c->pos.y * C_SIZE, c->pos.z * C_SIZE};
}

uint32_t chunk_seed(struct chunk *c) {
    ASSERT_CHUNK(c);

    // FNV-1a hash https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
    const uint32_t prime = 16777619u;
    const uint32_t basis = 2166136261u;

    uint32_t hash = basis;
    hash ^= (uint32_t)c->pos.x;
    hash *= prime;
    hash ^= (uint32_t)c->pos.y;
    hash *= prime;
    hash ^= (uint32_t)c->pos.z;
    hash *= prime;
    return hash;
}

void chunk_set_neighbour(struct chunk *c, struct chunk *neighbour, int face) {
    ASSERT_CHUNK(c);
    assert(face >= 0 && face < 6);
    if (c == neighbour) {
        fprintf(stderr, "chunk_link: chunk is linking to itself\n");
        return;
    }

    c->neighbours[face] = neighbour;
    if (neighbour)
        neighbour->neighbours[opposite_dir(face)] = c;
}

static struct mesh_quad make_quad(int x, int y, int z, int face, block_t block, u32 light, u8 ao[4], bool flipped) {
    struct mesh_quad quad = {0};
    quad.pos_face = (x & 0x3F) | ((y & 0x3F) << 6) | ((z & 0x3F) << 12);
    quad.pos_face |= (face & 0x7) << 18;
    quad.pos_face |= ((ao[0] & 0x3) << 21) | ((ao[1] & 0x3) << 23) | ((ao[2] & 0x3) << 25) | ((ao[3] & 0x3) << 27);
    quad.pos_face |= (flipped & 0x1) << 29;
    quad.light = light;

    int texture_index = block_texture_index(block, face);
    assert(texture_index != NO_TEXTURE_INDEX);
    quad.texid = texture_index;
    return quad;
}

#define ao_value(s1, c, s2) ((solids[s1] && solids[s2]) ? 1 : (3 - (solids[s1] + solids[s2] + solids[c])))

static void get_face_ao(struct chunk *c, int x, int y, int z, int face, uint8_t *ao_out, bool *flip) {
    assert(face >= 0 && face < 6);
    // counter clockwise
    // took fucking ages
    const ivec3 ao_offsets[6][8] = {{{1, -1, -1}, {1, -1, 0}, {1, -1, 1}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}, {1, 1, -1}, {1, 0, -1}}, {{-1, -1, 1}, {-1, -1, 0}, {-1, -1, -1}, {-1, 0, -1}, {-1, 1, -1}, {-1, 1, 0}, {-1, 1, 1}, {-1, 0, 1}},
                                    {{-1, 1, -1}, {0, 1, -1}, {1, 1, -1}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}, {-1, 1, 1}, {-1, 1, 0}}, {{-1, -1, -1}, {-1, -1, 0}, {-1, -1, 1}, {0, -1, 1}, {1, -1, 1}, {1, -1, 0}, {1, -1, -1}, {0, -1, -1}},
                                    {{1, -1, 1}, {0, -1, 1}, {-1, -1, 1}, {-1, 0, 1}, {-1, 1, 1}, {0, 1, 1}, {1, 1, 1}, {1, 0, 1}}, {{-1, -1, -1}, {0, -1, -1}, {1, -1, -1}, {1, 0, -1}, {1, 1, -1}, {0, 1, -1}, {-1, 1, -1}, {-1, 0, -1}}};
    bool solids[8];
    for (int i = 0; i < 8; i++) {
        int ox = x + ao_offsets[face][i].x;
        int oy = y + ao_offsets[face][i].y;
        int oz = z + ao_offsets[face][i].z;
        solids[i] = chunk_block_is_solid(c, ox, oy, oz);
    }

    ao_out[0] = ao_value(7, 0, 1); // BOT LEFT
    ao_out[1] = ao_value(1, 2, 3); // BOT RIGHT
    ao_out[2] = ao_value(3, 4, 5); // TOP LEFT
    ao_out[3] = ao_value(5, 6, 7); // TOP RIGHT
    *flip = !((ao_out[0] + ao_out[2] < ao_out[1] + ao_out[3]));
}

size_t chunk_generate_mesh_data(struct chunk *c, void *buffer) {
    ASSERT_CHUNK(c);
    assert(buffer != NULL);
    if (c->fill_count <= 0 || !c->dirty)
        return 0;

    if (!c->blocks) {
        c->quad_count = 0;
        c->dirty = false;
        return 0;
    }

    struct mesh_quad *quads = buffer;

    size_t added = 0;
    u8 ao[4] = {0, 0, 0, 0};
    for (int y = 0; y < C_SIZE; y++) {
        for (int x = 0; x < C_SIZE; x++) {
            for (int z = 0; z < C_SIZE; z++) {
                block_t block = chunk_get_block(c, x, y, z);
                if (!block_is_solid(block))
                    continue;

                for (int face = 0; face < 6; face++) {
                    ivec3 normal = FACE_NORMALS[face];
                    int ox = x + normal.x;
                    int oy = y + normal.y;
                    int oz = z + normal.z;

                    if (block_create_face(block, face, chunk_get_block(c, ox, oy, oz))) {
                        bool flip = false;
                        get_face_ao(c, x, y, z, face, ao, &flip);
                        struct mesh_quad quad = make_quad(x, y, z, face, block, 0xFF, ao, flip);
                        quads[added++] = quad;
                    }
                }
            }
        }
    }

    c->quad_count = added;
    c->dirty = false;
    return added;
}

size_t chunk_generate_face_mesh_data(struct chunk *c, int face, void *buffer) {
    ASSERT_CHUNK(c);
    assert(buffer != NULL);

    if (c->fill_count <= 0 || !c->dirty)
        return 0;

    if (!c->blocks) {
        c->quad_count = 0;
        c->dirty = false;
        return 0;
    }

    struct mesh_quad *quads = buffer;
    size_t added = 0;
    u8 ao[4] = {0, 0, 0, 0};
    for (int y = 0; y < C_SIZE; y++) {
        for (int x = 0; x < C_SIZE; x++) {
            for (int z = 0; z < C_SIZE; z++) {

                u32 light = 0;
                if (x == 0 || y == 0 || z == 0)
                    light = 1;

                block_t block = chunk_get_block(c, x, y, z);
                if (!block_is_solid(block))
                    continue;

                ivec3 normal = FACE_NORMALS[face];
                int ox = x + normal.x;
                int oy = y + normal.y;
                int oz = z + normal.z;
                if (block_create_face(block, face, chunk_get_block(c, ox, oy, oz))) {
                    bool flip = false;
                    get_face_ao(c, x, y, z, face, ao, &flip);
                    quads[added++] = make_quad(x, y, z, face, block, light, ao, flip);
                }
            }
        }
    }

    c->quad_count = added;
    c->dirty = false;
    return added;
}
