#ifndef MESH_H
#define MESH_H
#include "thread.h"
#include "utils.h"
#include <glad/gl.h>
#include <stdbool.h>

#define FLUSH_INSTANTLY 0

struct draw_command {
    GLuint count;
    GLuint instanceCount;
    GLuint first;
    GLuint baseInstance;
};

struct mesh_batch {
    GLuint buffer;
    void *mapped;
    u64 buffer_size;
    u64 page_size;
    u64 page_count;

    u64 *page_bitmap;
    u64 bitmap_word_count;
    u64 free_pages;
    u64 last_alloc_word;

#if !FLUSH_INSTANTLY
    u64 *flush_ranges;
    u64 flush_range_count;
    u64 flush_range_cap;
#endif

    // commands and objects has en same count
    u64 items_added;
    u64 items_cap;
    // draw commands
    GLuint cmd_buf;
    struct draw_command *cmd_ptr;
    // per object data
    GLuint obj_buf;
    void *obj_ptr;
    u64 obj_stride;

    mutex_t lock;
};

typedef struct mesh_batch mesh_batch_t;

// might collide with other handle types in the future?
typedef struct mesh_handle {
    void *ptr;
    u64 page, size;
    bool valid;
} mesh_handle_t;

mesh_batch_t *mesh_batch_init(const char *name, u64 buffer_size, u64 page_size, u64 max_items, u64 object_stride);
// void mesh_batch_destroy(mesh_batch_t *batch);
bool mesh_batch_alloc(mesh_batch_t *batch, void *data, u64 size, mesh_handle_t *handle);
// bool mesh_batch_realloc(mesh_batch_t *batch, mesh_handle_t *handle, void *new_data, u64 new_size);
bool mesh_batch_free(mesh_batch_t *batch, mesh_handle_t *handle);
void mesh_batch_add_command(mesh_batch_t *batch, const struct draw_command *cmd, const void *object_data);
void mesh_batch_clear_commands(mesh_batch_t *batch);
bool mesh_batch_handle_is_valid(const mesh_handle_t *handle);
void *mesh_batch_handle_ptr(const mesh_handle_t *handle);
int mesh_batch_calculate_offset(const mesh_batch_t *batch, const mesh_handle_t *handle, size_t element_stride, size_t elements_per_item);

// flushes any pending memory updates and issues the draw calls
// buffer_loc: binding slot for the buffer
// data_loc: binding slot for the per-object data
void mesh_batch_multidraw(mesh_batch_t *batch, GLuint buffer_loc, GLuint data_loc);

#endif // MESH_H