#include "world.h"
#include "gfx.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FNL_IMPL
#include <FastNoiseLite.h>

#define NUM_JUMPS 4
#define AXIS_STEPS (C_SIZE / NUM_JUMPS + 1)
#define SAMPLE_VOLUME (AXIS_STEPS * AXIS_STEPS * AXIS_STEPS)
#define SAMPLE_VOLUME_SIZE (AXIS_STEPS * AXIS_STEPS * AXIS_STEPS * sizeof(float))

static inline int zxy_index_l(int x, int y, int z, int l) {
    return z + (x * l) + (y * l * l);
}

static inline void relative_pos(int x, int y, int z, int size, int *hx, int *hy, int *hz, int *lx, int *ly, int *lz) {
    *hx = u_idiv(x, size);
    *hy = u_idiv(y, size);
    *hz = u_idiv(z, size);
    *lx = u_wrap(x, size);
    *ly = u_wrap(y, size);
    *lz = u_wrap(z, size);
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
    h->chunk_count = 0;
    w->hunks[w->hunk_count++] = h;
    return h;
}

static struct chunk *add_chunk(struct world *w, int x, int y, int z, struct hunk *hunk, int index) {
    if (w->chunk_count >= w->chunk_cap)
        return NULL;

    if (!hunk) {
        int hx, _hy, hz;
        int lx, ly, lz;
        relative_pos(x, y, z, H_SIZE, &hx, &_hy, &hz, &lx, &ly, &lz);

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

static void *m_test_worker(void *arg) {
    struct world *w = (struct world *)arg;
    while (!w->initialized)
        thrd_sleep(100);

    // generate data after neighbours
    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        gen_chunk_base(w, c);
    }

    // generate mesh after the data is done
    void *quads = malloc(C_SIZE3 * sizeof(struct mesh_quad));
    void *mesh_data = NULL;
    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        size_t quad_count = chunk_generate_mesh_data(c, quads);
        worldrnd_upload(&w->rnd, c, quads, quad_count * sizeof(struct mesh_quad));
    }

    free(quads);
    return NULL;
}

void world_init(struct world *w, int seed) {
    memset(w, 0, sizeof(struct world));

    const int SIZE = 32;
    w->initialized = true;
    w->seed = seed;

    w->chunk_count = 0;
    w->chunk_cap = SIZE * SIZE * SIZE;
    w->chunks = malloc(sizeof(struct chunk) * w->chunk_cap);

    w->hunk_count = 0;
    w->hunk_cap = 9;
    w->hunks = malloc(sizeof(struct hunk *) * w->hunk_cap);

    worldrnd_init(&w->rnd, H_SIZE3);
}

void world_generate_initiale(struct world *w) {
    assert(w->initialized);
    const int SIZE = 8;
    for (int z = 0; z < SIZE; z++) {
        for (int y = 0; y < SIZE; y++) {
            for (int x = 0; x < SIZE; x++) {
                int hx, _hy, hz;
                int lx, ly, lz;
                relative_pos(x, y, z, H_SIZE, &hx, &_hy, &hz, &lx, &ly, &lz);

                int index = zxy_index_l(lx, ly, lz, H_SIZE);
                struct hunk *h = add_hunk(w, hx, hz);
                add_chunk(w, x, y, z, h, index);
            }
        }
    }

    printf("World initialized with %d chunks\n", w->chunk_count);
    printf("Sampling %i instead of %i [%i jumps]\n", AXIS_STEPS * AXIS_STEPS * AXIS_STEPS, C_SIZE * C_SIZE * C_SIZE, NUM_JUMPS);

    // collect the neighbours
    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        for (int face = 0; face < 6; face++) {
            int dx = c->x + FACE_NORMALS[face].x;
            int dy = c->y + FACE_NORMALS[face].y;
            int dz = c->z + FACE_NORMALS[face].z;
            struct chunk *neighbour = world_get_chunk(w, dx, dy, dz);
            chunk_set_neighbour(c, neighbour, face);
        }
    }

    thrd_init(&w->test_worker_thrd, m_test_worker, w);
}

struct chunk *world_get_chunk(struct world *w, int x, int y, int z) {
    int hx, _hy, hz;
    int lx, ly, lz;
    relative_pos(x, y, z, H_SIZE, &hx, &_hy, &hz, &lx, &ly, &lz);
    struct hunk *hunk = get_hunk(w, hx, hz);
    if (!hunk)
        return NULL;
    int index = zxy_index_l(lx, ly, lz, H_SIZE);
    return hunk->chunks[index];
}

static void set_block_internal(struct world *w, struct chunk *c, int lx, int ly, int lz, block_t block) {
    if (!c)
        return;

    assert(lx >= 0 && lx < C_SIZE);
    assert(ly >= 0 && ly < C_SIZE);
    assert(lz >= 0 && lz < C_SIZE);

    if (!chunk_set_block(c, lx, ly, lz, block)) {
        //   fprintf(stderr, "set_block_internal: failed to set block in chunk (%d, %d, %d)\n", c->x, c->y, c->z);
        return;
    }

    int wx = (c->x * C_SIZE) + lx;
    int wy = (c->y * C_SIZE) + ly;
    int wz = (c->z * C_SIZE) + lz;
    struct hunk *hunk = get_hunk(w, u_idiv(c->x, H_SIZE), u_idiv(c->z, H_SIZE));
    if (!hunk) {
        fprintf(stderr, "set_block_internal: hunk for chunk (%d, %d) not found\n", c->x, c->z);
        return;
    }

    int hunkx = u_wrap(wx, HW_SIZE);
    int hunkz = u_wrap(wz, HW_SIZE);
    int hindex = hunkz * HW_SIZE + hunkx;
    uint16_t *heightref = &(hunk->height_map[hindex]);

    if (block == BLK_AIR && wy == *heightref) {
        int old_height = *heightref;
        // start traversing downwards to find the next non-air block
        // TODO: could we speed up world_get_block by crawling downwards in the same chunk/hunk?
        block_t b;
        while (old_height > 0) {
            old_height--;
            b = world_get_block(w, (ivec3){wx, old_height, wz}.x, old_height, (ivec3){wx, old_height, wz}.z);
            if (b != BLK_AIR) {
                *heightref = old_height;
                break;
            }
        }
    } else if (block != BLK_AIR && wy > *heightref) {
        *heightref = wy;
    }
}

void world_set_block(struct world *w, int x, int y, int z, block_t block) {
    int cx, cy, cz;
    int lx, ly, lz;
    relative_pos(x, y, z, C_SIZE, &cx, &cy, &cz, &lx, &ly, &lz);
    struct chunk *c = world_get_chunk(w, cx, cy, cz);
    if (!c)
        return;
    set_block_internal(w, c, lx, ly, lz, block);
}

block_t world_get_block(struct world *w, int x, int y, int z) {
    int cx, cy, cz;
    int lx, ly, lz;
    relative_pos(x, y, z, C_SIZE, &cx, &cy, &cz, &lx, &ly, &lz);
    struct chunk *c = world_get_chunk(w, cx, cy, cz);
    if (!c)
        return BLK_AIR;
    return chunk_get_block(c, lx, ly, lz);
}

int world_sample_height(struct world *w, int x, int z) {
    int hx = u_idiv(x, HW_SIZE);
    int hz = u_idiv(z, HW_SIZE);
    struct hunk *hunk = get_hunk(w, hx, hz);
    if (!hunk)
        return -1;

    int lx = u_wrap(x, HW_SIZE);
    int lz = u_wrap(z, HW_SIZE);
    int hindex = lz * HW_SIZE + lx;
    assert(hindex >= 0 && hindex < HW_SIZE2);
    return hunk->height_map[hindex];
}

void world_draw(struct world *w, struct cam *cam, GLuint program, GLuint atlas, int atlas_depth) {
    worldrnd_clear(&w->rnd);

    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        if (cam_aabb_in_view(cam, c->min, c->max)) {
            worldrnd_queue_chunk(&w->rnd, c);
            dbg_aabb(c->min, c->max, C32_CYAN);
        }
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
            hit->distance = sqrtf(dstsqr);
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

float interp_samples(const float *buf, int x, int y, int z) {
    assert(buf);
    assert(x >= 0 && x < C_SIZE && y >= 0 && y < C_SIZE && z >= 0 && z < C_SIZE);
    int x0 = x / NUM_JUMPS;
    int y0 = y / NUM_JUMPS;
    int z0 = z / NUM_JUMPS;

    // if we are on a direct spot than we don't need to interpolate
    if (x % NUM_JUMPS == 0 && y % NUM_JUMPS == 0 && z % NUM_JUMPS == 0)
        return buf[zxy_index_l(x0, y0, z0, AXIS_STEPS)];

    // printf("Sampling at (%d, %d, %d) low (%d, %d, %d), high (%d, %d, %d)\n", x, y, z, x0, y0, z0, x1, y1, z1);
    // printf("low index (%d, %d, %d), high index (%d, %d, %d)\n", x0 * SAMPLE_GRID_JUMPS, y0 * SAMPLE_GRID_JUMPS, z0 * SAMPLE_GRID_JUMPS, x1 * SAMPLE_GRID_JUMPS, y1 * SAMPLE_GRID_JUMPS, z1 * SAMPLE_GRID_JUMPS);

    float sx = (float)(x % NUM_JUMPS) / NUM_JUMPS;
    float sy = (float)(y % NUM_JUMPS) / NUM_JUMPS;
    float sz = (float)(z % NUM_JUMPS) / NUM_JUMPS;

    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const int z1 = z0 + 1;
    float c000 = buf[zxy_index_l(x0, y0, z0, AXIS_STEPS)];
    float c100 = buf[zxy_index_l(x1, y0, z0, AXIS_STEPS)];
    float v00 = u_lerp(c000, c100, sx);

    float c010 = buf[zxy_index_l(x0, y1, z0, AXIS_STEPS)];
    float c110 = buf[zxy_index_l(x1, y1, z0, AXIS_STEPS)];
    float v10 = u_lerp(c010, c110, sx);

    float c001 = buf[zxy_index_l(x0, y0, z1, AXIS_STEPS)];
    float c101 = buf[zxy_index_l(x1, y0, z1, AXIS_STEPS)];
    float v01 = u_lerp(c001, c101, sx);

    float c011 = buf[zxy_index_l(x0, y1, z1, AXIS_STEPS)];
    float c111 = buf[zxy_index_l(x1, y1, z1, AXIS_STEPS)];
    float v11 = u_lerp(c011, c111, sx);

    float v0 = u_lerp(v00, v10, sy);
    float v1 = u_lerp(v01, v11, sy);
    return u_lerp(v0, v1, sz);
}

static void fill_samples(float *samples, struct chunk *c, fnl_state *state, float scale) {

    for (int z = 0; z < AXIS_STEPS; z++) {
        for (int x = 0; x < AXIS_STEPS; x++) {
            for (int y = 0; y < AXIS_STEPS; y++) {
                int dx = (c->x * C_SIZE + (x * NUM_JUMPS)) * scale;
                int dy = (c->y * C_SIZE + (y * NUM_JUMPS)) * scale;
                int dz = (c->z * C_SIZE + (z * NUM_JUMPS)) * scale;
                int index = zxy_index_l(x, y, z, AXIS_STEPS);
                samples[index] = fnlGetNoise3D(state, dx, dy, dz);
            }
        }
    }
}

static float simplex2d(int x, int y, float scale, int seed) {
    fnl_state state = fnlCreateState(seed);
    state.noise_type = FNL_NOISE_OPENSIMPLEX2;
    // state.frequency = 1.0f / scale;
    return fnlGetNoise2D(&state, x / scale, y / scale);
}

static void gen_chunk_base2(struct world *w, struct chunk *c) {

    fnl_state state = fnlCreateState(w->seed);
    state.noise_type = FNL_NOISE_OPENSIMPLEX2;
    state.octaves = 4;
    state.frequency = 0.008f;
    state.fractal_type = FNL_FRACTAL_FBM;
    state.lacunarity = 2.0f;
    state.gain = 0.38f;

    fnl_state state2 = state;
    state2.seed += 1;
    state2.frequency = 0.002f;

    const int world_height = 80;
    const float noise_height_scale = 8.0f;

    for (int z = 0; z < C_SIZE; z++) {
        for (int x = 0; x < C_SIZE; x++) {

            int wx = x + c->x * C_SIZE;
            int wz = z + c->z * C_SIZE;
            float h = world_height + fnlGetNoise2D(&state, wx, wz) * noise_height_scale;
            // h += fnlGetNoise2D(&state2, wx, wz) * (noise_height_scale * 0.5f);

            for (int y = 0; y < C_SIZE; y++) {
                int wy = y + c->y * C_SIZE;

                block_t block = BLK_AIR;
                if (wy < h) {
                    block = BLK_STONE;
                }

                set_block_internal(w, c, x, y, z, block);
            }
        }
    }
}

void gen_chunk_base(struct world *w, struct chunk *c) {

    gen_chunk_base2(w, c);

    return;

    float samples[SAMPLE_VOLUME];
    fnl_state state = fnlCreateState(w->seed);
    state.noise_type = FNL_NOISE_OPENSIMPLEX2;
    state.octaves = 5;

    const int world_height = 80;
    const float scale = 3.001f;
    const float squish_factor = 0.15f;

    fnl_state height = fnlCreateState(w->seed + 1);
    height.noise_type = FNL_NOISE_OPENSIMPLEX2;
    height.fractal_type = FNL_FRACTAL_FBM;

    float height_noise_scale = 0.5f;
    float height_world_scale = 8.0f;

    fill_samples(samples, c, &state, scale);

    for (int z = 0; z < C_SIZE; z++) {
        for (int x = 0; x < C_SIZE; x++) {

            int wx = x + c->x * C_SIZE;
            int wz = z + c->z * C_SIZE;

            float h = world_height + fnlGetNoise2D(&height, wx * height_noise_scale, wz * height_noise_scale) * 8.0f;
            h += fnlGetNoise2D(&height, wx * height_noise_scale * 2, wz * height_noise_scale * 0.5f) * 4.0f;
            h += fnlGetNoise2D(&height, wx * height_noise_scale * 4, wz * height_noise_scale * 0.025f) * 4.0f;

            /*
                double h = simplex.noise2d(x / 100d, y / 100d) * 8d;
        h += simplex.noise2d(x / 30d, y / 30d) * 4d;
        h += simplex.noise2d(x / 15d, y / 15d) * 2d;
        h += simplex.noise2d(x / 7d, y / 7d);
        */
            // float h = simplex2d(wx, wz, 100.0f / 100.0f, w->seed) * 8.0f;

            for (int y = 0; y < C_SIZE; y++) {
                int wy = y + c->y * C_SIZE;

                float n = interp_samples(samples, x, y, z);

#if 0
                float density_mod = (world_height - wy) * squish_factor;
                block_t block = BLK_AIR;
                if (n + density_mod > 0.0f)
                    block = BLK_STONE;
                set_block_internal(w, c, x, y, z, block);
#else

                float density_mod = (world_height - wy) * squish_factor;

                block_t block = BLK_AIR;
                if (wy < h && n + density_mod > 0.0f) {
                    block = BLK_STONE;
                }

                set_block_internal(w, c, x, y, z, block);
#endif
            }
        }
    }
}
