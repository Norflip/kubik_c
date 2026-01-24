#ifndef WORLD_H
#define WORLD_H
#include "chunk.h"
#include "maf.h"
#include "mesh.h"
#include "queue.h"
#include "thread.h"
#include "utils.h"

struct hunk;
struct world {
    bool initialized;
    int seed;
    int num_worker_threads;
    bool flatgrass; // should probably be a flag 
    int load_radius;

    struct chunk **chunks;
    int chunk_count, chunk_cap;
    
    struct hunk **hunks;
    int hunk_count, hunk_cap;

    struct chunk **dirty_chunks;
    int dirty_chunk_count, dirty_chunk_cap;

    mesh_buffer_t *batch;
    pqueue_t work_queue;
    mutex_t work_lock;
    cond_t work_cond;

    mutex_t alloc_lock;
    mutex_t mesh_lock;
    void *worker_arena;
};

struct world_hit {
    block_t block;
    ivec3 pos, normal;
    float distance;
    bool did_hit;
};

void world_init(struct world *w, int seed, int num_workers, int load_radius, bool flatgrass);
void world_generate_initial(struct world *w);

struct chunk *world_get_chunk(struct world *w, int x, int y, int z);
void world_set_block(struct world *w, int x, int y, int z, block_t block);
block_t world_get_block(struct world *w, int x, int y, int z);
int world_sample_height(struct world *w, int x, int z);
bool world_is_working(struct world *w);
void world_tick(struct world *w);
bool world_trace(struct world *w, vec3 ro, vec3 rd, float max_distance, struct world_hit *hit);
aabb_t world_get_hunk_bounds(struct world *w, struct hunk* h);

// Debugging
// validates the chunks and their data
void validate_world(struct world *w);

#endif // WORLD_H