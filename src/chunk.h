#ifndef CHUNK_H
#define CHUNK_H
#include "block.h"
#include "maf.h"
#include "utils.h"
#include <glad/gl.h>
#include <stdbool.h>

#define C_SIZE 32
#define C_SIZEP (C_SIZE + 2)
#define C_SIZEP2 (C_SIZEP * C_SIZEP)
#define C_SIZEP3 (C_SIZEP * C_SIZEP * C_SIZEP)

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

// allocates memory for the buffer, frees the memory in upload_mesh_data
int chunk_generate_mesh_data(struct chunk *c, void **data);
void chunk_upload_mesh_data(struct chunk *c, void *data);

void tmp_fill_chunk_data(struct chunk *c, bool fill_padding);

#endif // CHUNK_H