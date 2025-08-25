#include "world.h"
#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define idiv(x, d) ((x > 0) ? (x) / (d) : ((x) - (d) + 1) / (d))
#define iwrap(x, l) ((((x) % (l)) + (l)) % (l))

static inline int zxy_index_l(int x, int y, int z, int l) {
    return z + (x * l) + (y * l * l);
}

static inline void chunk_to_hunk(int x, int y, int z, int *hx, int *hz, int *lx, int *ly, int *lz, int *index) {
    *hx = idiv(x, H_SIZE);
    *hz = idiv(z, H_SIZE);

    *lx = iwrap(x, H_SIZE);
    *ly = iwrap(y, H_SIZE);
    *lz = iwrap(z, H_SIZE);
    *index = zxy_index_l(iwrap(*lx, H_SIZE), iwrap(*ly, H_SIZE), iwrap(*lz, H_SIZE), H_SIZE);
}

static struct hunk *get_hunk(struct world *w, int x, int z) {
    for (int i = 0; i < w->hunk_count; i++) {
        struct hunk *h = w->hunks[i];
        if (h->x == x && h->z == z) {
            return h;
        }
    }
    return NULL;
}

static struct hunk *add_hunk(struct world *w, int x, int z) {
    struct hunk *h = get_hunk(w, x, z);
    if (h != NULL)
        return h;

    h = malloc(sizeof(struct hunk));
    memset(h, 0, sizeof(struct hunk));
    h->x = x;
    h->z = z;
    w->hunks[w->hunk_count++] = h;
    return h;
}

static struct chunk *add_chunk(struct world *w, int x, int y, int z, struct hunk *hunk, int index) {
    if (w->chunk_count >= w->chunk_cap) {
        return NULL;
    }

    if (!hunk) {
        int hx, hz, index;
        int lx, ly, lz;
        chunk_to_hunk(x, y, z, &hx, &hz, &lx, &ly, &lz, &index);
        hunk = get_hunk(w, hx, hz);

        if (hunk == NULL) {
            fprintf(stderr, "add_chunk: hunk (%d, %d) not found for chunk (%d, %d, %d)\n", hx, hz, x, y, z);
            return NULL;
        }
    }

    if (hunk->chunks[index]) {
        fprintf(stderr, "add_chunk: chunk (%d, %d, %d) already exists in hunk (%d, %d) at index (%i)\n", x, y, z, hunk->x, hunk->z, index);
        return hunk->chunks[index];
    }

    struct chunk *c = malloc(sizeof(struct chunk));
    chunk_initialize(c, x, y, z);
    w->chunks[w->chunk_count++] = c;
    hunk->chunks[index] = c;
    hunk->chunk_count++;
    return c;
}

void world_init_demo(struct world *w) {

    const int SIZE = 5;

    w->chunk_count = 0;
    w->chunk_cap = SIZE * SIZE * SIZE;
    w->chunks = malloc(sizeof(struct chunk) * w->chunk_cap);

    w->hunk_count = 0;
    w->hunk_cap = 9;
    w->hunks = malloc(sizeof(struct hunk *) * w->hunk_cap);

    glGenVertexArrays(1, &w->empty_vao);

    for (int z = 0; z < SIZE; z++) {
        for (int y = 0; y < SIZE; y++) {
            for (int x = 0; x < SIZE; x++) {

                int lcx = x - (SIZE / 2);
                int lcy = y - (SIZE / 2);
                int lcz = z - (SIZE / 2);

                int hx, hz, index;
                int lx, ly, lz;
                chunk_to_hunk(lcx, lcy, lcz, &hx, &hz, &lx, &ly, &lz, &index);

                struct hunk *h = add_hunk(w, hx, hz);
                add_chunk(w, lcx, lcy, lcz, h, index);
            }
        }
    }

    printf("World initialized with %d chunks\n", w->chunk_count);

    // collect the neighbours
    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        for (int face = 0; face < 6; face++) {
            int dx = c->x + FACE_NORMALS[face].x;
            int dy = c->y + FACE_NORMALS[face].y;
            int dz = c->z + FACE_NORMALS[face].z;
            struct chunk *neighbour = world_get_chunk(w, dx, dy, dz);
            chunk_link(c, neighbour, face);
        }
    }

    // generate data after neighbours
    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        tmp_fill_chunk_data(c, false);
    }

    // generate mesh after the data is done
    void *mesh_data = NULL;
    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        chunk_generate_mesh_data(c, &mesh_data);
        chunk_upload_mesh_data(c, mesh_data);
    }
}

struct chunk *world_get_chunk(struct world *w, int x, int y, int z) {

    int hx, hz, index;
    int lx, ly, lz;
    chunk_to_hunk(x, y, z, &hx, &hz, &lx, &ly, &lz, &index);

    struct hunk *hunk = get_hunk(w, hx, hz);
    if (!hunk) {
        fprintf(stderr, "world_get_chunk: hunk (%d, %d) not found\n", hx, hz);
        return NULL;
    }

    return hunk->chunks[index];
}

void tmp_world_draw(struct world *w, struct cam *cam, GLuint program, GLuint atlas, int atlas_depth) {

    if (w->chunk_count == 0)
        return;

    w->draw_count = 0;
    cam_update(cam);

    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "vp"), 1, GL_FALSE, cam_view_proj_ptr(cam));
    glUniform1ui(glGetUniformLocation(program, "atlas_depth"), (u32)atlas_depth);
    glBindTextureUnit(0, atlas);

    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        if (c->quad_count == 0 || c->quad_buffer == GL_NONE)
            continue;
        vec3 min = {(float)c->x * C_SIZE, (float)c->y * C_SIZE, (float)c->z * C_SIZE};
        vec3 max = {(float)(c->x + 1) * C_SIZE, (float)(c->y + 1) * C_SIZE, (float)(c->z + 1) * C_SIZE};

        if (!cam_aabb_in_view(cam, min, max))
            continue;

        glUniform3i(glGetUniformLocation(program, "chunk_pos"), (u32)c->x * C_SIZE, (u32)c->y * C_SIZE, (u32)c->z * C_SIZE);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, c->quad_buffer);
        glBindVertexArray(w->empty_vao);
        glDrawArrays(GL_TRIANGLES, 0, c->quad_count * 6);

        dbg_aabb(min, max, C32_CYAN);
        w->draw_count++;
    }
}
