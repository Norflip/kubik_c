#ifndef WORLD_H
#define WORLD_H
#include "cam.h"
#include "chunk.h"
#include "gfx.h"
#include "maf.h"
#include "thread.h"
#include "utils.h"
#include "mesh.h"

#define H_SIZE 16
#define H_SIZE2 (H_SIZE * H_SIZE)
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

   // struct worldrnd rnd;
    mesh_batch_t* batch;

    struct work *work_heap;
    size_t work_heap_count, work_heap_cap;
    mutex_t work_lock;
    cond_t work_cond;
};

struct world_hit {
    block_t block;
    ivec3 pos, normal;
    float distance;
    bool did_hit;
};

void world_init(struct world *w, int seed, int num_worker_threads);
void world_generate_initial(struct world *w, int load_radius);

struct chunk *world_get_chunk(struct world *w, int x, int y, int z);
void world_set_block(struct world *w, int x, int y, int z, block_t block);
block_t world_get_block(struct world *w, int x, int y, int z);
int world_sample_height(struct world *w, int x, int z);
bool world_is_working(struct world *w);

void world_draw(struct world *w, struct cam *cam, GLuint program, GLuint atlas, int atlas_depth);
bool world_trace(struct world *w, vec3 o, vec3 dir, float max_distance, struct world_hit *hit);

// Debugging
// validates the chunks and their data
void validate_world(struct world *w);

#endif // WORLD_H