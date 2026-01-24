#ifndef MESH_H
#define MESH_H
#include "thread.h"
#include "utils.h"
#include <glad/gl.h>
#include <stdbool.h>

struct mesh_quad {
    u32 pos_face; // 3*6 bits voxel pos + 3 bits face + (2*4) bits AO + 1 bit flipped = 30 bits, 2 bits remaining
    u32 texid;
    u32 light;
    u32 _padding;
};

#define FLUSH_INSTANTLY 0

struct draw_command {
    GLuint count;
    GLuint instanceCount;
    GLuint first;
    GLuint baseInstance;
};

struct mesh_buffer {
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

typedef struct mesh_buffer mesh_buffer_t;

// might collide with other handle types in the future?
typedef struct mesh_handle {
    void *ptr;
    u64 page, size;
    bool valid;
} mesh_handle_t;

mesh_buffer_t *mesh_buffer_init(const char *name, u64 buffer_size, u64 page_size, u64 max_items, u64 object_stride);
// void mesh_buffer_destroy(mesh_buffer_t *buffer);
bool mesh_buffer_alloc(mesh_buffer_t *buffer, void *data, u64 size, mesh_handle_t *handle);
// bool mesh_buffer_realloc(mesh_buffer_t *buffer, mesh_handle_t *handle, void *new_data, u64 new_size);
bool mesh_buffer_free(mesh_buffer_t *buffer, mesh_handle_t *handle);

void mesh_buffer_add_command(mesh_buffer_t *buffer, const struct draw_command *cmd, const void *object_data);
void mesh_buffer_clear_commands(mesh_buffer_t *buffer);

bool mesh_buffer_handle_is_valid(const mesh_handle_t *handle);
void *mesh_buffer_handle_ptr(const mesh_handle_t *handle);
int mesh_buffer_calculate_offset(const mesh_buffer_t *buffer, const mesh_handle_t *handle, size_t element_stride, size_t elements_per_item);

// flushes any pending memory updates and issues the draw calls
// buffer_loc: binding slot for the buffer
// data_loc: binding slot for the per-object data
void mesh_buffer_multidraw(mesh_buffer_t *buffer, GLuint buffer_loc, GLuint data_loc);

#endif // MESH_H