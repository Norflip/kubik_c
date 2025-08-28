#ifndef WORLD_H
#define WORLD_H
#include "cam.h"
#include "chunk.h"
#include "maf.h"
#include "utils.h"
#include "render.h"

#define H_SIZE 16
#define H_SIZE3 (H_SIZE * H_SIZE * H_SIZE)

struct hunk {
    int x, z;
    struct chunk *chunks[H_SIZE3];
    int chunk_count;
};

struct drawbuf2;

struct world {
    struct chunk **chunks;
    int chunk_count, chunk_cap;

    struct hunk **hunks;
    int hunk_count, hunk_cap;

    bool initialized;
    struct worldrnd rnd;
};

void world_init_demo(struct world *w);
struct chunk *world_get_chunk(struct world *w, int x, int y, int z);
void world_draw(struct world *w, struct cam *cam, GLuint program, GLuint atlas, int atlas_depth);

#endif // WORLD_H