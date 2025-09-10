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

static inline int zxy_index_l(int x, int y, int z) {
    return z + (x * C_SIZEP) + (y * C_SIZEP2);
}

#define ASSERT_CHUNK(c) \
    assert(c != NULL);  \
    assert(c->_canary == C_CANARY)

void chunk_initialize(struct chunk *c, int x, int y, int z) {
    memset(c, 0, sizeof(struct chunk));
    c->x = x;
    c->y = y;
    c->z = z;
    c->_canary = C_CANARY;
    c->tint = C32_CLEAR;
    c->min = (vec3){(float)c->x * C_SIZE, (float)c->y * C_SIZE, (float)c->z * C_SIZE};
    c->max = (vec3){(float)(c->x + 1) * C_SIZE, (float)(c->y + 1) * C_SIZE, (float)(c->z + 1) * C_SIZE};
}

static struct chunk *get_neighbour(struct chunk *c, int face) {
    if (c == NULL || face < 0 || face >= 6)
        return NULL;
    return c->neighbours[face];
}

static inline struct chunk *find_neighbor(struct chunk *chunk, int dx, int dy, int dz) {
    struct chunk *current = chunk;

    if (dx > 0) {
        current = current->neighbours[DIR_RIGHT];
    } else if (dx < 0) {
        current = current->neighbours[DIR_LEFT];
    }

    if (!current)
        return NULL;

    if (dy > 0) {
        current = current->neighbours[DIR_UP];
    } else if (dy < 0) {
        current = current->neighbours[DIR_DOWN];
    }
    if (!current)
        return NULL;

    if (dz > 0) {
        current = current->neighbours[DIR_FRONT];
    } else if (dz < 0) {
        current = current->neighbours[DIR_BACK];
    }

    return current;
}

static void set_padded_block(struct chunk *c, int x, int y, int z, block_t value) {

    if (!on_border(x, y, z))
        return;

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

            int idx = zxy_index_l(tx + 1, ty + 1, tz + 1);
            n->blocks[idx] = value;
            n->dirty = true;
        }
    }
}

bool chunk_set_block(struct chunk *c, int x, int y, int z, block_t value) {
    ASSERT_CHUNK(c);
    if (!inside(x, y, z)) {
        fprintf(stderr, "chunk_set_block: position (%d, %d, %d) is out of bounds in chunk (%d, %d, %d)\n", x, y, z, c->x, c->y, c->z);
        return false;
    }

    const int index = zxy_index_l(x + 1, y + 1, z + 1);
    block_t old_value = c->blocks[index];
    if (old_value != 0 && value == 0) {
        c->fill_count--;
    } else if (old_value == 0 && value != 0) {
        c->fill_count++;
    }

    c->blocks[index] = value;
    c->dirty = true;
    set_padded_block(c, x, y, z, value);
    return true;
}

block_t chunk_get_block(struct chunk *c, int x, int y, int z) {
    ASSERT_CHUNK(c);
    if (!inside_padded(x, y, z))
        return 0;
    return c->blocks[zxy_index_l(x + 1, y + 1, z + 1)];
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

static struct mesh_quad make_quad(int x, int y, int z, int face, block_t block, u8 ao[4], bool flipped) {
    struct mesh_quad quad = {0};
    quad.pos_face = (x & 0x3F) | ((y & 0x3F) << 6) | ((z & 0x3F) << 12);
    quad.pos_face |= (face & 0x7) << 18;
    quad.pos_face |= ((ao[0] & 0x3) << 21) | ((ao[1] & 0x3) << 23) | ((ao[2] & 0x3) << 25) | ((ao[3] & 0x3) << 27);
    quad.pos_face |= (flipped & 0x1) << 29;

    int texture_index = block_texture_index(block, face);
    assert(texture_index != NO_TEXTURE_INDEX);
    quad.texid = texture_index;
    return quad;
}

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
        block_t b = c->blocks[zxy_index_l(ox + 1, oy + 1, oz + 1)];
        solids[i] = (b != 0);
    }

#define ao_value(s1, c, s2) ((solids[s1] && solids[s2]) ? 1 : (3 - (solids[s1] + solids[s2] + solids[c])))
    ao_out[0] = ao_value(7, 0, 1); // BOT LEFT
    ao_out[1] = ao_value(1, 2, 3); // BOT RIGHT
    ao_out[2] = ao_value(3, 4, 5); // TOP LEFT
    ao_out[3] = ao_value(5, 6, 7); // TOP RIGHT
    *flip = !((ao_out[0] + ao_out[2] < ao_out[1] + ao_out[3]));
#undef ao_value
}

size_t chunk_generate_mesh_data(struct chunk *c, void *buffer) {
    ASSERT_CHUNK(c);
    if (c->fill_count <= 0 || !c->dirty) {
        return 0;
    }

    const size_t max_faces = c->fill_count * 6; // 6 faces per block
    struct mesh_quad *quads = buffer;
    // malloc(max_faces * sizeof(struct mesh_quad));

    if (!quads) {
        fprintf(stderr, "chunk_generate_mesh_data: failed to allocate memory for quads\n");
        return 0;
    }

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
                    int o_idx = zxy_index_l(ox + 1, oy + 1, oz + 1);

                    if (block_create_face(block, face, c->blocks[o_idx])) {
                        bool flip = false;
                        get_face_ao(c, x, y, z, face, ao, &flip);
                        struct mesh_quad quad = make_quad(x, y, z, face, block, ao, flip);
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
