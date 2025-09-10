#ifndef WORLD_H
#define WORLD_H
#include "cam.h"
#include "chunk.h"
#include "maf.h"
#include "platf.h"
#include "render.h"
#include "utils.h"

#define H_SIZE 16
#define H_SIZE3 (H_SIZE * H_SIZE * H_SIZE)
#define HW_SIZE (H_SIZE * C_SIZE)
#define HW_SIZE2 (HW_SIZE * HW_SIZE)

struct hunk {
    int x, z;
    struct chunk *chunks[H_SIZE3];
    int chunk_count;
    uint16_t height_map[HW_SIZE2];
};

struct work;

struct world {
    bool initialized;
    int seed;
    int num_worker_threads;

    struct chunk **chunks;
    int chunk_count, chunk_cap;

    struct hunk **hunks;
    int hunk_count, hunk_cap;
    struct worldrnd rnd;

    struct work *heap;
    size_t heap_count, heap_cap;
    mtx_t work_lock;
    cnd_t work_cond;
};

struct world_hit {
    block_t block;
    ivec3 pos, normal;
    float distance;
    bool did_hit;
};

void world_init(struct world *w, int seed, int num_worker_threads);
void world_generate_initiale(struct world *w, int size);

struct chunk *world_get_chunk(struct world *w, int x, int y, int z);
void world_set_block(struct world *w, int x, int y, int z, block_t block);
block_t world_get_block(struct world *w, int x, int y, int z);

int world_sample_height(struct world *w, int x, int z);

void world_draw(struct world *w, struct cam *cam, GLuint program, GLuint atlas, int atlas_depth);
bool world_trace(struct world *w, vec3 o, vec3 dir, float max_distance, struct world_hit *hit);
void gen_chunk_base(struct world *w, struct chunk *c);

#endif // WORLD_H