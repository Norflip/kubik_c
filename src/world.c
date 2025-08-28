#include "world.h"
#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int zxy_index_l(int x, int y, int z, int l) {
    return z + (x * l) + (y * l * l);
}

// TODO: too bulky, consider refactoring
static inline void chunk_to_hunk(int x, int y, int z, int *hx, int *hz, int *lx, int *ly, int *lz, int *index) {
    *hx = u_idiv(x, H_SIZE);
    *hz = u_idiv(z, H_SIZE);
    *lx = u_wrap(x, H_SIZE);
    *ly = u_wrap(y, H_SIZE);
    *lz = u_wrap(z, H_SIZE);
    *index = zxy_index_l(u_wrap(*lx, H_SIZE), u_wrap(*ly, H_SIZE), u_wrap(*lz, H_SIZE), H_SIZE);
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
    if (w->chunk_count >= w->chunk_cap)
        return NULL;

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

    const int SIZE = 8;
    w->chunk_count = 0;
    w->chunk_cap = SIZE * SIZE * SIZE;
    w->chunks = malloc(sizeof(struct chunk) * w->chunk_cap);

    w->hunk_count = 0;
    w->hunk_cap = 9;
    w->hunks = malloc(sizeof(struct hunk *) * w->hunk_cap);

    worldrnd_init(&w->rnd, 512);

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
        worldrnd_upload(&w->rnd, c, mesh_data, c->quad_count * sizeof(struct mesh_quad));
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

void world_draw(struct world *w, struct cam *cam, GLuint program, GLuint atlas, int atlas_depth) {
    worldrnd_clear(&w->rnd);

    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        if (!cam_aabb_in_view(cam, c->min, c->max))
            continue;
        worldrnd_queue_chunk(&w->rnd, c);
        dbg_aabb(c->min, c->max, C32_CYAN);
    }

    worldrnd_draw(&w->rnd, cam, program, atlas, atlas_depth);
}
