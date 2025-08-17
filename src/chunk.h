#ifndef CHUNK_H
#define CHUNK_H
#include "maf.h"
#include "utils.h"
#include <glad/gl.h>
#include <stdbool.h>

#define C_SIZE 32
#define C_SIZEP (C_SIZE + 2)
#define C_SIZEP2 (C_SIZEP * C_SIZEP)
#define C_SIZEP3 (C_SIZEP * C_SIZEP * C_SIZEP)

typedef uint8_t block_t;

// directions
enum dir { DIR_LEFT, DIR_RIGHT, DIR_UP, DIR_DOWN, DIR_FORWARD, DIR_BACKWARD };
#define opposite_dir(i) (i + (!((i) % 2) ? 1 : -1))

static const int FACE_NORMALS[6 * 3] = {1, 0, 0, -1, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 1, 0, 0, -1};

struct chunk {
    int _canary;
    int x, y, z;

    block_t blocks[C_SIZEP3];
    int fill_count; // excludes the padded blocks

    struct chunk *neighbours[6];

    // rendering stuff
    col32_t tint;
    GLuint quad_buffer;
    int quad_count;

    int dirty;
};

void chunk_initialize(struct chunk *c, int x, int y, int z);
void chunk_set_block(struct chunk *c, u8 x, u8 y, u8 z, block_t value);
void chunk_set_corner (struct chunk *c, u8 x, u8 y, u8 z, block_t value);
block_t chunk_get_block(struct chunk *c, u8 x, u8 y, u8 z);

void chunk_set_neighbour(struct chunk *c, int face, struct chunk *neighbour);

int chunk_generate_mesh_data(struct chunk *c, void **data);
void chunk_upload_mesh_data(struct chunk *c, void *data);

void tmp_draw_chunk (struct chunk *c, float* vp, GLuint program, GLuint atlas, int atlas_count);
void tmp_fill_chunk_data(struct chunk *c, bool fill_padding);

#endif // CHUNK_H