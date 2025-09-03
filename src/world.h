#ifndef WORLD_H
#define WORLD_H
#include "cam.h"
#include "chunk.h"
#include "maf.h"
#include "render.h"
#include "utils.h"

#define H_SIZE 16
#define H_SIZE3 (H_SIZE * H_SIZE * H_SIZE)

struct hunk {
    int x, z;
    struct chunk *chunks[H_SIZE3];
    int chunk_count;
};

struct world {
    bool initialized;
    int seed;

    struct chunk **chunks;
    int chunk_count, chunk_cap;

    struct hunk **hunks;
    int hunk_count, hunk_cap;

    struct worldrnd rnd;
};

struct world_hit {
    block_t block;
    ivec3 pos, normal;
    float distance;
    bool did_hit;
};

void world_init(struct world *w, int seed);
void world_generate_initiale(struct world *w);

struct chunk *world_get_chunk(struct world *w, int x, int y, int z);
block_t world_get_block(struct world *w, int x, int y, int z);
void world_draw(struct world *w, struct cam *cam, GLuint program, GLuint atlas, int atlas_depth);
bool world_trace(struct world *w, vec3 o, vec3 dir, float max_distance, struct world_hit *hit);

void world_gen_chunk_base(struct world *w, struct chunk *c);

#endif // WORLD_H