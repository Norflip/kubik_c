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

block_t world_get_block(struct world *w, int x, int y, int z) {
    // world coordinates to chunk coordinates
    int cx = u_idiv(x, C_SIZE);
    int cy = u_idiv(y, C_SIZE);
    int cz = u_idiv(z, C_SIZE);
    struct chunk *c = world_get_chunk(w, cx, cy, cz);
    if (!c) {
        return BLK_AIR;
    }

    int lx = u_wrap(x, C_SIZE);
    int ly = u_wrap(y, C_SIZE);
    int lz = u_wrap(z, C_SIZE);
    return chunk_get_block(c, lx, ly, lz);
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

bool world_trace(struct world *w, vec3 ro, vec3 rd, float max_distance, struct world_hit *hit) {

    ivec3 pos = iv3_floor(ro);
    rd = v3_normalize(rd);
    ivec3 step = (ivec3){(rd.x > 0) ? 1 : ((rd.x < 0) ? -1 : 0), (rd.y > 0) ? 1 : ((rd.y < 0) ? -1 : 0), (rd.z > 0) ? 1 : ((rd.z < 0) ? -1 : 0)};
    vec3 fr = (vec3){.x = ro.x - floorf(ro.x), .y = ro.y - floorf(ro.y), .z = ro.z - floorf(ro.z)};

    float tx = step.x / rd.x;
    float ty = step.y / rd.y;
    float tz = step.z / rd.z;

    float tMaxX, tMaxY, tMaxZ;
    tMaxX = tx * ((rd.x > 0.0f) ? (1.0f - fr.x) : fr.x);
    tMaxY = ty * ((rd.y > 0.0f) ? (1.0f - fr.y) : fr.y);
    tMaxZ = tz * ((rd.z > 0.0f) ? (1.0f - fr.z) : fr.z);
    int maxsteps = (int)ceilf(max_distance * 1.7f);
    const float max_distance_sqr = max_distance * max_distance;

    do {
        // check if out of bounds
        if (pos.y < 0 || pos.y >= (H_SIZE * C_SIZE))
            break;

        float dstsqr = v3_lensqr(v3_sub(v3_cast(pos), ro));
        if (dstsqr > max_distance_sqr)
            break;

        hit->block = world_get_block(w, pos.x, pos.y, pos.z);
        if (hit->block != BLK_AIR && block_is_solid(hit->block)) {
            hit->pos = pos;
            hit->did_hit = true;
            return true;
        }

        if (tMaxX < tMaxY) {
            if (tMaxZ < tMaxX) {
                tMaxZ += tz;
                pos.z += step.z;
                hit->normal = (ivec3){0, 0, -step.z};
            } else {
                tMaxX += tx;
                pos.x += step.x;
                hit->normal = (ivec3){-step.x, 0, 0};
            }
        } else {
            if (tMaxZ < tMaxY) {
                tMaxZ += tz;
                pos.z += step.z;
                hit->normal = (ivec3){0, 0, -step.z};
            } else {
                tMaxY += ty;
                pos.y += step.y;
                hit->normal = (ivec3){0, -step.y, 0};
            }
        }
    } while (maxsteps--);

    hit->did_hit = false;
    return false;
}
