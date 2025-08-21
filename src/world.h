#ifndef WORLD_H
#define WORLD_H
#include "chunk.h"
#include "maf.h"

struct world {
    struct chunk **chunks;
    int chunk_count, chunk_cap;
};

void world_init_demo(struct world *w);
struct chunk *world_get_chunk(struct world *w, int x, int y, int z);
void tmp_world_draw(struct world *w, mat4 vp, GLuint program, GLuint atlas, int atlas_depth);

#endif // WORLD_H