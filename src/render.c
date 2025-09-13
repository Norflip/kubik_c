#include "render.h"
#include "chunk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define set_bit(bitmap, bit) bitmap[(bit) / BITS_PER_WORD] |= (1ULL << ((bit) % BITS_PER_WORD))
#define clear_bit(bitmap, bit) bitmap[(bit) / BITS_PER_WORD] &= ~(1ULL << ((bit) % BITS_PER_WORD))
#define test_bit(bitmap, bit) (bitmap[(bit) / BITS_PER_WORD] & (1ULL << ((bit) % BITS_PER_WORD))) != 0

static inline size_t size_to_pages(size_t size) {
    return (size + PAGE_SIZE - 1) / PAGE_SIZE;
}

static struct drawbuf *alloc_drawbuf(int max_commands) {
    struct drawbuf *buf = malloc(sizeof(struct drawbuf));
    if (!buf)
        return NULL;
    memset(buf, 0, sizeof(struct drawbuf));
    buf->last_alloc_word = 0;
    buf->flush_range_cap = max_commands;
    buf->flush_ranges = malloc(sizeof(uint64_t) * buf->flush_range_cap);
    buf->free_pages = PAGE_COUNT;
    memset(buf->page_bitmap, 0xFF, BITMAP_WORD_COUNT * sizeof(uint64_t));

    const GLbitfield mapping_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;
    const GLbitfield storage_flags = GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;

    // primary data buffer
    glCreateBuffers(1, &buf->buffer);
    glNamedBufferStorage(buf->buffer, BUFFER_SIZE, NULL, storage_flags);
    buf->mapped = glMapNamedBufferRange(buf->buffer, 0, BUFFER_SIZE, mapping_flags);

    if (!buf->mapped) {
        fprintf(stderr, "mdi_buffer_init: failed to map buffer\n");
        return NULL;
    }

    size_t unused_bits = (BITMAP_WORD_COUNT * BITS_PER_WORD) - PAGE_COUNT;
    if (unused_bits > 0) {
        uint64_t mask = (1ULL << (BITS_PER_WORD - unused_bits)) - 1;
        buf->page_bitmap[BITMAP_WORD_COUNT - 1] &= mask;
    }

    // command buffer
    glCreateBuffers(1, &buf->cmd_buffer);
    glNamedBufferStorage(buf->cmd_buffer, sizeof(struct draw_command) * max_commands, NULL, storage_flags);
    buf->cmds = glMapNamedBufferRange(buf->cmd_buffer, 0, sizeof(struct draw_command) * max_commands, mapping_flags);
    buf->cmds_cap = max_commands;
    buf->cmds_count = 0;

    // per chunk data
    glCreateBuffers(1, &buf->chunk_data_buffer);
    glNamedBufferStorage(buf->chunk_data_buffer, sizeof(struct per_chunk_data) * max_commands, NULL, storage_flags);
    buf->chunk_data = glMapNamedBufferRange(buf->chunk_data_buffer, 0, sizeof(struct per_chunk_data) * max_commands, mapping_flags);

    return buf;
}

static size_t find_free_page(const struct drawbuf *mdib) {
#if 1
    // start search after the hint index
    for (size_t word = mdib->last_alloc_word; word < BITMAP_WORD_COUNT; word++) {
        if (mdib->page_bitmap[word] != 0) {
            int bit = __builtin_ctzll(mdib->page_bitmap[word]);
            if (bit >= 0) {
                size_t page_idx = word * BITS_PER_WORD + bit;
                if (page_idx < PAGE_COUNT) {
                    return page_idx;
                }
            }
        }
    }
#endif
    // search from beginning to hint
    for (size_t word = 0; word < mdib->last_alloc_word && word < BITMAP_WORD_COUNT; word++) {
        if (mdib->page_bitmap[word] != 0) {
            int bit = __builtin_ctzll(mdib->page_bitmap[word]);
            if (bit >= 0) {
                size_t page_idx = word * BITS_PER_WORD + bit;
                if (page_idx < PAGE_COUNT) {
                    return page_idx;
                }
            }
        }
    }

    return PAGE_COUNT;
}

static size_t find_contiguous_pages(struct drawbuf *buf, size_t page_count) {
    if (page_count == 0 || page_count > PAGE_COUNT) {
        printf("find_contiguous_pages: invalid request for %zu pages\n", page_count);
        return PAGE_COUNT;
    }

    if (page_count == 1)
        return find_free_page(buf);

    size_t start = 0;
    while (start <= PAGE_COUNT - page_count) {
        size_t count = 0;
        size_t current = start;
        while (current < PAGE_COUNT && count < page_count) {
            if (test_bit(buf->page_bitmap, current)) {
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
    return PAGE_COUNT;
}

static bool find_space(struct worldrnd *rnd, size_t page_count, struct drawbuf **buf_out, size_t *pos_out) {

    const int start_index = 0;
    for (int i = 0; i < rnd->drawbuf_count; i++) {

        // will be useful later if we implement different buffers for different parts
        struct drawbuf *buf = rnd->drawbuf[(start_index + i) % rnd->drawbuf_count];
        if (page_count > buf->free_pages) {
            fprintf(stderr, "find_space: not enough free pages in buffer %d [%d/%d]\n", i, buf->free_pages, PAGE_COUNT);
            continue;
        }

        size_t start_page = find_contiguous_pages(buf, page_count);
        if (start_page < PAGE_COUNT) {
            *buf_out = buf;
            *pos_out = start_page;
            return true;
        } else {
            break;
        }
    }

    return false;
}

void worldrnd_init(struct worldrnd *rnd, int max_commands) {
    rnd->max_commands = 2;
    while (rnd->max_commands < max_commands)
        rnd->max_commands <<= 1;
    printf("worldrnd_init: max_commands=%d [%d]\n", rnd->max_commands, max_commands);

    rnd->drawbuf_cap = 1;
    rnd->drawbuf_count = 0;
    rnd->drawbuf = malloc(sizeof(struct drawbuf *) * rnd->drawbuf_cap);
    if (!rnd->drawbuf) {
        fprintf(stderr, "worldrnd_init: failed to allocate draw buffers\n");
        return;
    }

    rnd->drawbuf[rnd->drawbuf_count++] = alloc_drawbuf(rnd->max_commands);
    mtx_init(&rnd->upload_lock);
    mtx_init(&rnd->cmd_lock);
}

bool worldrnd_upload(struct worldrnd *rnd, struct chunk *c, const void *data, uint32_t size) {
    if (size == 0 || !rnd || !c || !data || c->fill_count == 0)
        return false;

    size_t page_count = size_to_pages(size);
    struct drawbuf *buf = NULL;
    mtx_lock(&rnd->upload_lock);

    size_t pos = 0;
    if (!find_space(rnd, page_count, &buf, &pos)) {
        fprintf(stderr, "worldrnd_upload: failed to find space\n");
        mtx_unlock(&rnd->upload_lock);
        return false;
    }

    if (pos >= PAGE_COUNT) {
        fprintf(stderr, "worldrnd_upload: position is out of bounds\n");
        mtx_unlock(&rnd->upload_lock);
        return false;
    }

    if (buf == NULL) {
        fprintf(stderr, "worldrnd_upload: draw buffer is NULL\n");
        mtx_unlock(&rnd->upload_lock);
        return false;
    }

    // mark pages allocated
    for (size_t i = 0; i < page_count; i++) {
        clear_bit(buf->page_bitmap, pos + i);
    }

    buf->free_pages -= page_count;
    buf->last_alloc_word = pos / BITS_PER_WORD;

    void *ptr = (void *)((char *)buf->mapped + (pos * PAGE_SIZE));
    memmove(ptr, data, size);

    // add the dirty range
    uint64_t *dirty_range = &buf->flush_ranges[buf->flush_range_count++];
    *dirty_range = ((uint64_t)pos << 32) | (uint32_t)size;
    mtx_unlock(&rnd->upload_lock);

    c->mdi_page = pos;
    c->mdi_size = size;
    c->mdiptr = buf;
    c->mdi_valid = true;
    return true;
}

bool worldrnd_unload(struct worldrnd *rnd, struct chunk *c) {
    if (!c->mdi_valid)
        return false;

    struct drawbuf *buf = c->mdiptr;
    size_t page_count = size_to_pages(c->mdi_size);
    for (size_t i = 0; i < page_count; i++) {
        size_t pos = c->mdi_page + i;
        buf->page_bitmap[pos / BITS_PER_WORD] |= (1ULL << (pos % BITS_PER_WORD));
    }

    buf->free_pages += page_count;
    c->mdi_valid = false;
    return true;
}

void worldrnd_clear(struct worldrnd *rnd) {
    mtx_lock(&rnd->cmd_lock);
    for (int i = 0; i < rnd->drawbuf_count; i++) {
        struct drawbuf *buf = rnd->drawbuf[i];
        buf->cmds_count = 0;
        buf->flush_range_count = 0;
    }

    rnd->draw_count = 0;
    rnd->added_cmds_count = 0;
    mtx_unlock(&rnd->cmd_lock);
}

void worldrnd_queue_chunk(struct worldrnd *rnd, struct chunk *c) {
    struct drawbuf *buf = c->mdiptr;
    if (c->fill_count == 0 || !c->mdi_valid || !buf)
        return;

    struct draw_command cmd = {0};
    cmd.baseInstance = 0;
    cmd.instanceCount = 1;
    cmd.first = (c->mdi_page * PAGE_SIZE / sizeof(struct mesh_quad)) * 6;
    cmd.count = c->quad_count * 6;

    if (buf->cmds_count >= buf->cmds_cap) {
        fprintf(stderr, "worldrnd_add_chunk_drawcmd: command buffer full (%d)\n", buf->cmds_cap);
        return;
    }

    mtx_lock(&rnd->cmd_lock);

    int index = buf->cmds_count;
    buf->cmds[index] = cmd;
    const ivec3 wpos = chunk_world_pos(c);
    buf->chunk_data[index] = (struct per_chunk_data){.x = wpos.x, .y = wpos.y, .z = wpos.z, .tint = c->tint};
    buf->cmds_count++;
    rnd->added_cmds_count++;
    mtx_unlock(&rnd->cmd_lock);
}

void worldrnd_draw(struct worldrnd *rnd, struct cam *cam, GLuint program, GLuint atlas, int atlas_count) {

    static GLuint empty_vao = GL_NONE;
    if (empty_vao == GL_NONE)
        glGenVertexArrays(1, &empty_vao);

    rnd->draw_count = 0;
    glUseProgram(program);
    glBindTextureUnit(0, atlas);
    glUniformMatrix4fv(glGetUniformLocation(program, "vp"), 1, GL_FALSE, cam_view_proj_ptr(cam));
    glUniform1ui(glGetUniformLocation(program, "atlas_depth"), (u32)atlas_count);
    glBindVertexArray(empty_vao);

    mtx_lock(&rnd->cmd_lock);

    for (int i = 0; i < rnd->drawbuf_count; i++) {
        struct drawbuf *buf = rnd->drawbuf[i];
        if (buf->cmds_count == 0)
            continue;

        glFlushMappedNamedBufferRange(buf->chunk_data_buffer, 0, sizeof(struct per_chunk_data) * buf->cmds_count);
        glFlushMappedNamedBufferRange(buf->cmd_buffer, 0, sizeof(struct draw_command) * buf->cmds_count);

        for (int i = 0; i < buf->flush_range_count; i++) {
            uint64_t *range = &buf->flush_ranges[i];
            uint32_t size = *range & 0xFFFFFFFF;
            uint32_t offset = (*range >> 32) & 0xFFFFFFFF;
            glFlushMappedNamedBufferRange(buf->buffer, offset, size);
        }

        buf->flush_range_count = 0;
        glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf->buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buf->chunk_data_buffer);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buf->cmd_buffer);
        glMultiDrawArraysIndirect(GL_TRIANGLES, 0, buf->cmds_count, 0);
        rnd->draw_count++;
    }

    mtx_unlock(&rnd->cmd_lock);
}
