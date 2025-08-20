#include "chunk.h"
#include "gfx.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define C_CANARY 0xBABEDAD

struct mesh_quad {
    u32 pos_face; // 3*6 bits voxel pos + 3 bits face + (2*4) bits AO + 1 bit flipped = 30 bits, 2 bits remaining
    u32 texid;
};

// this should probably be moved to maf.h
typedef struct {
    int x, y, z;
} int3;

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

static inline int zxy_index(int x, int y, int z) {
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
    c->tint = C32_WHITE;
}

static struct chunk *get_neighbour(struct chunk *c, int face) {
    if (c == NULL || face < 0 || face >= 6)
        return NULL;
    return c->neighbours[face];
}

// Optimized neighbor traversal
static inline struct chunk *get_neighbor2(struct chunk *chunk, int dx, int dy, int dz) {
    struct chunk *current = chunk;

    // Traverse X direction
    if (dx > 0) {
        current = current->neighbours[DIR_RIGHT];
    } else if (dx < 0) {
        current = current->neighbours[DIR_LEFT];
    }
    if (!current)
        return NULL;

    // Traverse Y direction
    if (dy > 0) {
        current = current->neighbours[DIR_UP];
    } else if (dy < 0) {
        current = current->neighbours[DIR_DOWN];
    }
    if (!current)
        return NULL;

    // Traverse Z direction
    if (dz > 0) {
        current = current->neighbours[DIR_FRONT];
    } else if (dz < 0) {
        current = current->neighbours[DIR_BACK];
    }

    return current;
}

static void set_padded_block(struct chunk *c, int x, int y, int z, block_t value) {
    if (!on_padding(x, y, z)) {
        fprintf(stderr, "set_padded_block: position (%d, %d, %d) is out of bounds in chunk (%d, %d, %d)\n", x, y, z, c->x, c->y, c->z);
        return;
    }

    int idx = zxy_index(x + 1, y + 1, z + 1);
    c->blocks[idx] = value;
    c->dirty = true;
}

void chunk_set_block(struct chunk *c, int x, int y, int z, block_t value) {
    ASSERT_CHUNK(c);
    // offset to fit the padding

    if (!inside(x, y, z)) {
        fprintf(stderr, "chunk_set_block: position (%d, %d, %d) is out of bounds in chunk (%d, %d, %d)\n", x, y, z, c->x, c->y, c->z);
        return;
    }

    const int index = zxy_index(x + 1, y + 1, z + 1);
    block_t old_value = c->blocks[index];
    if (old_value != 0 && value == 0) {
        c->fill_count--;
    } else if (old_value == 0 && value != 0) {
        c->fill_count++;
    }

    c->blocks[index] = value;
    c->dirty = true;

    if (on_border(x, y, z)) {
        // Create boundary bitmask and count boundaries
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

        for (int face = 0; face < 6; face++) {
            if ((boundary_mask & (1 << face)) && c->neighbours[face]) {

                // printf("on %i boundary\n", face);
                int tx = x;
                int ty = y;
                int tz = z;

                // Adjust target coordinate for the specific face
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

                // printf("on border pos (%d, %d, %d) will set pos (%d, %d, %d) in neighbour (%d, %d, %d) face(%i) from chunk (%d, %d, %d)\n", x, y, z, tx, ty, tz, c->neighbours[face]->x, c->neighbours[face]->y, c->neighbours[face]->z, face, c->x,
                set_padded_block(c->neighbours[face], tx, ty, tz, value);
            }
        }
    }
}

block_t chunk_get_block(struct chunk *c, int x, int y, int z) {
    ASSERT_CHUNK(c);
    if (!inside_padded(x, y, z))
        return 0;
    return c->blocks[zxy_index(x + 1, y + 1, z + 1)];
}

void chunk_link(struct chunk *c, struct chunk *neighbour, int face) {
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
    quad.texid = 0; // block_get_texture_id(block, face);
    return quad;
}

static void get_face_ao(struct chunk *c, int x, int y, int z, int face, uint8_t *ao_out, bool *flip) {
    assert(face >= 0 && face < 6);
    // counter clockwise
    // took fucking ages
    const int3 ao_offsets[6][8] = {{{1, -1, -1}, {1, -1, 0}, {1, -1, 1}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}, {1, 1, -1}, {1, 0, -1}}, {{-1, -1, 1}, {-1, -1, 0}, {-1, -1, -1}, {-1, 0, -1}, {-1, 1, -1}, {-1, 1, 0}, {-1, 1, 1}, {-1, 0, 1}},
                                   {{-1, 1, -1}, {0, 1, -1}, {1, 1, -1}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}, {-1, 1, 1}, {-1, 1, 0}}, {{-1, -1, -1}, {-1, -1, 0}, {-1, -1, 1}, {0, -1, 1}, {1, -1, 1}, {1, -1, 0}, {1, -1, -1}, {0, -1, -1}},
                                   {{1, -1, 1}, {0, -1, 1}, {-1, -1, 1}, {-1, 0, 1}, {-1, 1, 1}, {0, 1, 1}, {1, 1, 1}, {1, 0, 1}}, {{-1, -1, -1}, {0, -1, -1}, {1, -1, -1}, {1, 0, -1}, {1, 1, -1}, {0, 1, -1}, {-1, 1, -1}, {-1, 0, -1}}};
    bool solids[8];
    for (int i = 0; i < 8; i++) {
        int ox = x + ao_offsets[face][i].x;
        int oy = y + ao_offsets[face][i].y;
        int oz = z + ao_offsets[face][i].z;
        block_t b = c->blocks[zxy_index(ox + 1, oy + 1, oz + 1)];
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

int chunk_generate_mesh_data(struct chunk *c, void **data) {
    ASSERT_CHUNK(c);
    if (c->fill_count <= 0) {
        fprintf(stderr, "chunk_generate_mesh_data: chunk is empty\n");
        return -1;
    }

    if (!c->dirty) {
        fprintf(stderr, "chunk_generate_mesh_data: chunk is not dirty\n");
        return -1;
    }

    const size_t max_faces = c->fill_count * 6; // 6 faces per block
    struct mesh_quad *quads = malloc(max_faces * sizeof(struct mesh_quad));
    if (!quads) {
        fprintf(stderr, "chunk_generate_mesh_data: failed to allocate memory for quads\n");
        return -1;
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
                    int o_idx = zxy_index(ox + 1, oy + 1, oz + 1);

                    // Check if the neighbour block is empty or out of bounds
                    if (!block_is_solid(c->blocks[o_idx])) {
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
    *data = quads;
    return added;
}

void chunk_upload_mesh_data(struct chunk *c, void *data) {
    ASSERT_CHUNK(c);
    if (data == NULL || c->fill_count == 0)
        return;

    // Upload the mesh data to the GPU
    glCreateBuffers(1, &(c->quad_buffer));
    glNamedBufferData(c->quad_buffer, c->quad_count * sizeof(struct mesh_quad), data, GL_STATIC_DRAW);
    free(data);
}

void tmp_draw_chunk(struct chunk *c, float *vp, GLuint program, GLuint atlas, int atlas_depth) {
    static GLuint empty_vao = GL_NONE;
    if (empty_vao == GL_NONE)
        glGenVertexArrays(1, &empty_vao);

    ASSERT_CHUNK(c);
    if (c->quad_count == 0 || c->quad_buffer == GL_NONE) {
        return;
    }

    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "vp"), 1, GL_FALSE, vp);
    glUniform1ui(glGetUniformLocation(program, "atlas_depth"), (u32)atlas_depth);
    glUniform3i(glGetUniformLocation(program, "chunk_pos"), (u32)c->x * C_SIZE, (u32)c->y * C_SIZE, (u32)c->z * C_SIZE);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, c->quad_buffer);
    glBindTextureUnit(0, atlas);

    glBindVertexArray(empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, c->quad_count * 6);
}

void tmp_fill_chunk_data(struct chunk *c, bool fill_padding) {

    ASSERT_CHUNK(c);
    //memset(c->blocks, 0, sizeof(c->blocks));

    for (int z = 0; z < C_SIZE; z++) {
        for (int x = 0; x < C_SIZE; x++) {
            int height = 10;

            if (x > 2 && x < 12 && z > 2 && z < 12)
                height = 4;

            if (x > 14 && x < 20 && z > 14 && z < 20)
                height = C_SIZE;

            for (int y = 0; y < height; y++) {
                int index = zxy_index(x, y, z);
                chunk_set_block(c, x, y, z, 1);
            }
        }
    }

    if (fill_padding) {
        for (int z = -1; z <= C_SIZE; z++) {
            for (int y = -1; y <= C_SIZE; y++) {
                for (int x = -1; x <= C_SIZE; x++) {
                    if (x == -1 || y == -1 || z == -1 || x == C_SIZE || y == C_SIZE || z == C_SIZE) {
                        int index = zxy_index(x + 1, y + 1, z + 1);
                        c->blocks[index] = 1;
                    }
                }
            }
        }
    }
}
