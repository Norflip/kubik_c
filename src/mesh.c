#include "mesh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BITS_PER_WORD (sizeof(uint64_t) * 8)
#define set_bit(bitmap, bit) bitmap[(bit) / BITS_PER_WORD] |= (1ULL << ((bit) % BITS_PER_WORD))
#define clear_bit(bitmap, bit) bitmap[(bit) / BITS_PER_WORD] &= ~(1ULL << ((bit) % BITS_PER_WORD))
#define test_bit(bitmap, bit) ((bitmap[(bit) / BITS_PER_WORD] & (1ULL << ((bit) % BITS_PER_WORD))) != 0)

static const GLbitfield mapping_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;
static const GLbitfield storage_flags = GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;

static inline u64 pages_required(u64 size, u64 page_size) {
    return (size + page_size - 1) / page_size;
}

static inline void set_object_label(GLuint buffer, const char *format, const char *name) {
    char label[64];
    snprintf(label, sizeof(label), "%s_obj_buffer", name);
    glObjectLabel(GL_BUFFER, buffer, -1, label);
}

static u64 find_free_page(const mesh_batch_t *batch) {

    // start search after the hint index
    for (u64 word = batch->last_alloc_word; word < batch->bitmap_word_count; word++) {
        if (batch->page_bitmap[word] != 0) {
            int bit = __builtin_ctzll(batch->page_bitmap[word]);
            if (bit >= 0) {
                u64 page_idx = word * BITS_PER_WORD + bit;
                if (page_idx < batch->page_count) {
                    return page_idx;
                }
            }
        }
    }

    // search from beginning to hint
    for (u64 word = 0; word < batch->last_alloc_word && word < batch->bitmap_word_count; word++) {
        if (batch->page_bitmap[word] != 0) {
            int bit = __builtin_ctzll(batch->page_bitmap[word]);
            if (bit >= 0) {
                u64 page_idx = word * BITS_PER_WORD + bit;
                if (page_idx < batch->page_count) {
                    return page_idx;
                }
            }
        }
    }

    return -1;
}

static u64 find_contiguous_pages(mesh_batch_t *batch, u64 page_count) {
    if (page_count == 0 || page_count > batch->page_count) {
        printf("find_contiguous_pages: invalid request for %zu pages\n", page_count);
        return -1;
    }

    if (page_count == 1)
        return find_free_page(batch);

    u64 start = 0;
    while (start <= batch->page_count - page_count) {
        u64 count = 0;
        u64 current = start;
        while (current < batch->page_count && count < page_count) {
            if (test_bit(batch->page_bitmap, current)) {
                count++;
                current++;
            } else
                break;
        }
        if (count == page_count)
            return start;
        start = current + 1;
    }

    fprintf(stderr, "find_contiguous_pages: failed to find contiguous pages\n");
    return -1;
}

mesh_batch_t *mesh_batch_init(const char *name, u64 buffer_size, u64 page_size, u64 max_items, u64 object_stride) {
    mesh_batch_t *batch = malloc(sizeof(mesh_batch_t));
    if (!batch) {
        fprintf(stderr, "mesh_batch_init: failed to allocate mesh_batch\n");
        return NULL;
    }
    memset(batch, 0, sizeof(mesh_batch_t));
    batch->items_added = 0;
    batch->items_cap = max_items;
    batch->page_size = page_size;
    batch->buffer_size = buffer_size;
    batch->page_count = buffer_size / page_size;
    batch->last_alloc_word = 0;
    batch->bitmap_word_count = (batch->page_count + BITS_PER_WORD - 1) / BITS_PER_WORD;
    batch->page_bitmap = malloc(sizeof(u64) * batch->bitmap_word_count);
    mutex_init(&batch->lock);

    if (!batch->page_bitmap) {
        fprintf(stderr, "initialize_paged_allocator: failed to allocate page bitmap\n");
        return NULL;
    }
    memset(batch->page_bitmap, 0xFF, batch->bitmap_word_count * sizeof(uint64_t));

#if !FLUSH_INSTANTLY
    batch->flush_range_cap = max_items;
    batch->flush_ranges = malloc(sizeof(u64) * batch->flush_range_cap);
    batch->free_pages = batch->page_count;
#endif

    // primary mesh buffer
    glCreateBuffers(1, &batch->buffer);
    glNamedBufferStorage(batch->buffer, buffer_size, NULL, storage_flags);
    batch->mapped = glMapNamedBufferRange(batch->buffer, 0, buffer_size, mapping_flags);
    set_object_label(batch->buffer, "%s_buf", name);

    u64 unused_bits = (batch->bitmap_word_count * BITS_PER_WORD) - batch->page_count;
    if (unused_bits > 0) {
        uint64_t mask = (1ULL << (BITS_PER_WORD - unused_bits)) - 1;
        batch->page_bitmap[batch->bitmap_word_count - 1] &= mask;
    }

    // command buffer
    glCreateBuffers(1, &batch->cmd_buf);
    glNamedBufferStorage(batch->cmd_buf, sizeof(struct draw_command) * max_items, NULL, storage_flags);
    batch->cmd_ptr = glMapNamedBufferRange(batch->cmd_buf, 0, sizeof(struct draw_command) * max_items, mapping_flags);
    set_object_label(batch->cmd_buf, "%s_cmd_buf", name);

    // per instance data
    glCreateBuffers(1, &batch->obj_buf);
    glNamedBufferStorage(batch->obj_buf, object_stride * max_items, NULL, storage_flags);
    batch->obj_ptr = glMapNamedBufferRange(batch->obj_buf, 0, object_stride * max_items, mapping_flags);
    batch->obj_stride = object_stride;
    set_object_label(batch->obj_buf, "%s_obj_buf", name);

    return batch;
}

bool mesh_batch_alloc(mesh_batch_t *batch, void *data, u64 size, mesh_handle_t *handle) {
    if (!batch || !data || !handle)
        return false;

    if (handle->valid) {
        fprintf(stderr, "mesh_batch_alloc: handle is already valid?\n");
        return false;
    }

    memset(handle, 0, sizeof(mesh_handle_t));

    if (size == 0) {
        fprintf(stderr, "mesh_batch_alloc: cannot allocate 0 bytes\n");
        return false;
    }

    if (batch->free_pages == 0) {
        fprintf(stderr, "mesh_batch_alloc: no free pages available\n");
        return false;
    }

    if (size > batch->buffer_size) {
        fprintf(stderr, "mesh_batch_alloc: requested size (%zu bytes) exceeds buffer size (%zu bytes)\n", size, batch->buffer_size);
        return false;
    }

    mutex_lock(&batch->lock);
    u64 page_count = pages_required(size, batch->page_size);
    u64 start = find_contiguous_pages(batch, page_count);

    // printf("mesh_batch_alloc: requested %zu bytes (%zu pages), allocated at page %zu\n", size, page_count, start);
    if (start == -1) {
        fprintf(stderr, "mesh_batch_alloc: failed to find space for %zu bytes (%zu pages)\n", size, page_count);

        mutex_unlock(&batch->lock);
        return false;
    }

    if (start >= batch->page_count) {
        fprintf(stderr, "mesh_batch_alloc: position is out of bounds\n");

        mutex_unlock(&batch->lock);
        return false;
    }

    // mark pages allocated
    for (u64 i = 0; i < page_count; i++)
        clear_bit(batch->page_bitmap, start + i);

    batch->free_pages -= page_count;
    batch->last_alloc_word = start / BITS_PER_WORD;

    void *ptr = (char *)batch->mapped + (start * batch->page_size);
    memmove(ptr, data, size);

#if FLUSH_INSTANTLY
    glFlushMappedNamedBufferRange(batch->buffer, start, size);
#else
    // add the dirty range
    uint64_t *dirty_range = &(batch->flush_ranges[batch->flush_range_count++]);
    *dirty_range = ((uint64_t)start << 32) | (uint32_t)size;
#endif

    handle->ptr = ptr;
    handle->page = start;
    handle->size = size;
    handle->valid = true;
    mutex_unlock(&batch->lock);
    return true;
}

bool mesh_batch_free(mesh_batch_t *batch, mesh_handle_t *handle) {
    if (!mesh_batch_handle_is_valid(handle))
        return false;

    mutex_lock(&batch->lock);
    const u64 page_count = pages_required(handle->size, batch->page_size);
    for (u64 i = 0; i < page_count; i++) {
        u64 pos = handle->page + i;
        set_bit(batch->page_bitmap, pos);
    }

    batch->free_pages += page_count;
    handle->valid = false;
    handle->ptr = NULL;
    handle->page = 0;
    handle->size = 0;
    mutex_unlock(&batch->lock);
    return true;
}

void mesh_batch_add_command(mesh_batch_t *batch, const struct draw_command *cmd, const void *object_data) {
    if (batch->items_added >= batch->items_cap) {
        fprintf(stderr, "mesh_batch_add_command: command buffer full (%zu)\n", batch->items_cap);
        return;
    }

    mutex_lock(&batch->lock);

    // batch->cmd_ptr[batch->items_added] = cmd;
    memcpy(&batch->cmd_ptr[batch->items_added], cmd, sizeof(struct draw_command));
    if (object_data && batch->obj_ptr)
        memcpy((char *)batch->obj_ptr + batch->obj_stride * batch->items_added, object_data, batch->obj_stride);

    batch->items_added++;
    mutex_unlock(&batch->lock);
}

void mesh_batch_clear_commands(mesh_batch_t *batch) {
    mutex_lock(&batch->lock);
    batch->items_added = 0;
    mutex_unlock(&batch->lock);
}

bool mesh_batch_handle_is_valid(const mesh_handle_t *handle) {
    return handle && handle->valid;
}

void *mesh_batch_handle_ptr(const mesh_handle_t *handle) {
    if (!mesh_batch_handle_is_valid(handle))
        return NULL;
    return handle->ptr;
}

int mesh_batch_calculate_offset(const mesh_batch_t *batch, const mesh_handle_t *handle, size_t element_stride, size_t elements_per_item) {
    if (!mesh_batch_handle_is_valid(handle))
        return -1;
    return (handle->page * batch->page_size / element_stride) * elements_per_item;
}

void mesh_batch_multidraw(mesh_batch_t *batch, GLuint buffer_loc, GLuint data_loc) {
    if (batch->items_added <= 0)
        return;

    mutex_lock(&batch->lock);

    glFlushMappedNamedBufferRange(batch->obj_buf, 0, batch->obj_stride * batch->items_added);
    glFlushMappedNamedBufferRange(batch->cmd_buf, 0, sizeof(struct draw_command) * batch->items_added);

#if !FLUSH_INSTANTLY
    for (int i = 0; i < batch->flush_range_count; i++) {
        uint64_t *range = &batch->flush_ranges[i];
        uint32_t size = *range & 0xFFFFFFFF;
        uint32_t offset = (*range >> 32) & 0xFFFFFFFF;
        glFlushMappedNamedBufferRange(batch->buffer, offset, size);
    }
    batch->flush_range_count = 0;
#endif

    glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
    // glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, batch->buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, batch->obj_buf);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, batch->cmd_buf);
    glMultiDrawArraysIndirect(GL_TRIANGLES, 0, batch->items_added, 0);
    mutex_unlock(&batch->lock);
}
