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
#define BLOCK_AIR 0
#define block_is_solid(b) ((b) != BLOCK_AIR)

// directions
enum dir { DIR_RIGHT = 0, DIR_LEFT, DIR_UP, DIR_DOWN, DIR_FRONT, DIR_BACK };
#define opposite_dir(i) (i + (!((i) % 2) ? 1 : -1))

typedef struct {
    int x, y, z;
} ivec3;

static const ivec3 FACE_NORMALS[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

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
void chunk_set_block(struct chunk *c, int x, int y, int z, block_t value);
block_t chunk_get_block(struct chunk *c, int x, int y, int z);

void chunk_link(struct chunk *c, struct chunk *neighbour, int face);

int chunk_generate_mesh_data(struct chunk *c, void **data);
void chunk_upload_mesh_data(struct chunk *c, void *data);

void tmp_draw_chunk(struct chunk *c, float *vp, GLuint program, GLuint atlas, int atlas_count);
void tmp_fill_chunk_data(struct chunk *c, bool fill_padding);

#endif // CHUNK_H