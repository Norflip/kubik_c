#ifndef RND_H
#define RND_H
#include "cam.h"
#include "platf.h"
#include "utils.h"
#include <glad/gl.h>
#include <stdbool.h>
#include <stdint.h>

struct world;
struct chunk;
struct drawbuf;

#define BUFFER_SIZE MB(64)
#define PAGE_SIZE KB(4)
#define PAGE_COUNT (BUFFER_SIZE / PAGE_SIZE)
#define BITS_PER_WORD (sizeof(uint64_t) * 8)
#define BITMAP_WORD_COUNT ((PAGE_COUNT + BITS_PER_WORD - 1) / BITS_PER_WORD)

// chunk MDI buffer stuff
struct draw_command {
    GLuint count;
    GLuint instanceCount;
    GLuint first;
    GLuint baseInstance;
};

struct per_chunk_data {
    int x, y, z;
    col32_t tint;
};

// chunk draw buffer
// TODO: rename it?
struct drawbuf {
    GLuint buffer;
    void *mapped;

    uint64_t page_bitmap[BITMAP_WORD_COUNT];
    size_t free_pages;
    size_t last_alloc_word;

    uint64_t *flush_ranges;
    size_t flush_range_count, flush_range_cap;

    // draw commands
    GLuint cmd_buffer;
    struct draw_command *cmds;
    int cmds_count, cmds_cap;

    // per chunk data, same count as cmds_count
    GLuint chunk_data_buffer;
    struct per_chunk_data *chunk_data;
};

struct worldrnd {
    struct world *world;
    struct drawbuf **drawbuf;
    int drawbuf_count, drawbuf_cap;
    int max_commands;
    mtx_t upload_lock;
    mtx_t cmd_lock;

    // stats
    int draw_count;
    int added_cmds_count;
};

void worldrnd_init(struct worldrnd *rnd, int max_commands);
bool worldrnd_upload(struct worldrnd *rnd, struct chunk *c, const void *data, uint32_t size);
bool worldrnd_unload(struct worldrnd *rnd, struct chunk *c);

void worldrnd_clear(struct worldrnd *rnd);
void worldrnd_queue_chunk(struct worldrnd *rnd, struct chunk *c);
void worldrnd_draw(struct worldrnd *rnd, struct cam *cam, GLuint program, GLuint atlas, int atlas_count);

#endif