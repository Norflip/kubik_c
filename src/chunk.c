#include "chunk.h"
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

static bool pos_inside(int x, int y, int z) {
    return (x >= 0 && x < C_SIZE && y >= 0 && y < C_SIZE && z >= 0 && z < C_SIZE);
}

static bool pos_inside_padded(int x, int y, int z) {
    return (x >= -1 && x < C_SIZE + 1 && y >= -1 && y < C_SIZE + 1 && z >= -1 && z < C_SIZE + 1);
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

void chunk_set_block(struct chunk *c, u8 x, u8 y, u8 z, block_t value) {
    ASSERT_CHUNK(c);
    // offset to fit the padding
    x += 1;
    y += 1;
    z += 1;

    if (!pos_inside_padded(x, y, z)) {
        fprintf(stderr, "chunk_set_block: position (%d, %d, %d) is out of bounds in chunk (%d, %d, %d)\n", x, y, z, c->x, c->y, c->z);
        return;
    }

    bool inside = pos_inside(x, y, z);
    const int index = zxy_index(x, y, z);

    if (inside) {
        block_t old_value = c->blocks[index];
        if (old_value != 0 && value == 0) {
            c->fill_count--;
        } else if (old_value == 0 && value != 0) {
            c->fill_count++;
        }
    }

    c->blocks[index] = value;
    c->dirty = true;
}

void chunk_set_corner(struct chunk *c, u8 x, u8 y, u8 z, block_t value) {
    ASSERT_CHUNK(c);
    // offset to fit the padding
    x += 1;
    y += 1;
    z += 1;

    // make sure the position is in the padded area
    if (pos_inside_padded(x, y, z) && !pos_inside(x, y, z)) {
        fprintf(stderr, "chunk_set_corner: position (%d, %d, %d) is out of bounds in chunk (%d, %d, %d)\n", x, y, z, c->x, c->y, c->z);
        return;
    }

    c->blocks[zxy_index(x, y, z)] = value;
    c->dirty = true;
}

block_t chunk_get_block(struct chunk *c, u8 x, u8 y, u8 z) {
    ASSERT_CHUNK(c);
    if (!pos_inside_padded(x, y, z))
        return 0;
    return c->blocks[zxy_index(x + 1, y + 1, z + 1)];
}

void chunk_set_neighbour(struct chunk *c, int face, struct chunk *neighbour) {
    ASSERT_CHUNK(c);
    assert(face >= 0 && face < 6);
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

    for (int z = 0; z < C_SIZE; z++) {
        for (int y = 0; y < C_SIZE; y++) {
            for (int x = 0; x < C_SIZE; x++) {
                block_t block = chunk_get_block(c, x, y, z);
                if (block == 0)
                    continue;

                for (int face = 0; face < 6; face++) {
                    int ox = x + FACE_NORMALS[face * 3 + 0];
                    int oy = y + FACE_NORMALS[face * 3 + 1];
                    int oz = z + FACE_NORMALS[face * 3 + 2];
                    int o_idx = zxy_index(ox + 1, oy + 1, oz + 1);

                    // Check if the neighbour block is empty or out of bounds
                    if (c->blocks[o_idx] == 0) {
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

void tmp_draw_chunk(struct chunk *c, float *vp, GLuint program, GLuint atlas, int atlas_count) {
    static GLuint empty_vao = GL_NONE;
    if (empty_vao == GL_NONE)
        glGenVertexArrays(1, &empty_vao);

    ASSERT_CHUNK(c);
    if (c->quad_count == 0 || c->quad_buffer == GL_NONE) {
        return;
    }

    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "vp"), 1, GL_FALSE, vp);
    glUniform1ui(glGetUniformLocation(program, "atlas_count"), (u32)atlas_count);
    glUniform3i(glGetUniformLocation(program, "chunk_pos"), (u32)c->x * C_SIZE, (u32)c->y * C_SIZE, (u32)c->z * C_SIZE);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, c->quad_buffer);
    glBindTextureUnit(0, atlas);

    glBindVertexArray(empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, c->quad_count * 6);
}

void tmp_fill_chunk_data(struct chunk *c, bool fill_padding) {

    ASSERT_CHUNK(c);
    memset(c->blocks, 0, sizeof(c->blocks));

#if 1
    for (int z = 0; z < C_SIZE; z++) {
        for (int x = 0; x < C_SIZE; x++) {
            int height = 10;

            if (x > 2 && x < 12 && z > 2 && z < 12)
                height = 4;

            if(x > 14 && x < 20 && z > 14 && z < 20)
                height = 11;

            for (int y = 0; y < height; y++) {
                int index = zxy_index(x, y, z);
                chunk_set_block(c, x, y, z, 1);
            }
        }
    }
#else
    for (int z = 0; z < 3; z++) {
        for (int x = 0; x < 3; x++) {
            for (int y = 0; y < 3; y++) {
                int index = zxy_index(x, y, z);
                chunk_set_block(c, x, y, z, 1);
            }
        }
    }
#endif

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