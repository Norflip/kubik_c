#include "world.h"
#include "gfx.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FNL_IMPL
#include <FastNoiseLite.h>

// work related stuff
#define WORK_MESH_PRIORITY_BASE 100

struct work {
    void (*func)(struct chunk *c, struct world *w, struct work *self);
    int priority;
    bool is_column;
    bool in_meshing_dist;
    union {
        struct {
            int x, z;
            int min_y, max_y;
        } column;
        struct chunk *chunk;
    };
};

static int work_cmp(const void *a, const void *b) {
    const struct work *wa = (const struct work *)a;
    const struct work *wb = (const struct work *)b;
    return (wa->priority > wb->priority) - (wa->priority < wb->priority);
}

static void push_work(struct world *world, struct work work) {
    mutex_lock(&world->work_lock);
    pqueue_enqueue(&world->work_que, &work);
    cond_signal(&world->work_cond);
    mutex_unlock(&world->work_lock);
}

static void pop_work(struct world *world, struct work *out) {
    mutex_lock(&world->work_lock);
    while (pqueue_is_empty(&world->work_que))
        cond_wait(&world->work_cond, &world->work_lock);
    pqueue_dequeue(&world->work_que, out);
    mutex_unlock(&world->work_lock);
}

static void gen_chunk_base(struct world *w, struct chunk *c);
static void gen_chunk_base_flatgrass(struct world *w, struct chunk *c);
static void gen_chunk_surface(struct world *w, struct chunk *c);
static void gen_chunk_caves(struct world *w, struct chunk *c);

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

static struct hunk *get_hunk_for_chunk(struct world *w, struct chunk *c) {
    int hx = u_idiv(c->pos.x, H_SIZE);
    int hz = u_idiv(c->pos.z, H_SIZE);
    return get_hunk(w, hx, hz);
}

static void generate_hunk_noise_maps(struct world *w, struct hunk *hunk) {
    fnl_state temp_state = fnlCreateState(w->seed + 4);
    temp_state.fractal_type = FNL_FRACTAL_FBM;
    temp_state.frequency = 0.003f;

    fnl_state rain_state = fnlCreateState(w->seed + 8);
    rain_state.fractal_type = FNL_FRACTAL_FBM;
    rain_state.frequency = 0.004f;

    const int hunk_x = hunk->x * HW_SIZE;
    const int hunk_z = hunk->z * HW_SIZE;

    for (int z = 0; z < HW_SIZE; z++) {
        for (int x = 0; x < HW_SIZE; x++) {
            float n = fnlGetNoise2D(&temp_state, x + hunk_x, z + hunk_z);
            n = (n + 1.0f) * 0.5f; // map from [-1, 1] to [0, 1]
            hunk->temp_map[z * HW_SIZE + x] = (uint8_t)(n * 255);
        }
    }

    for (int z = 0; z < HW_SIZE; z++) {
        for (int x = 0; x < HW_SIZE; x++) {
            float n = fnlGetNoise2D(&rain_state, x + hunk_x, z + hunk_z);
            n = (n + 1.0f) * 0.5f; // map from [-1, 1] to [0, 1]
            hunk->rain_map[z * HW_SIZE + x] = (uint8_t)(n * 255);
        }
    }
}

static float hunk_get_rainfall(struct hunk *hunk, int x, int z) {
    x = u_wrap(x, HW_SIZE);
    z = u_wrap(z, HW_SIZE);
    return hunk->rain_map[z * HW_SIZE + x] / 255.0f;
}

static float hunk_get_temperature(struct hunk *hunk, int x, int z) {
    x = u_wrap(x, HW_SIZE);
    z = u_wrap(z, HW_SIZE);
    return hunk->temp_map[z * HW_SIZE + x] / 255.0f;
}

static struct hunk *alloc_hunk(struct world *w, int x, int z) {
    struct hunk *h = get_hunk(w, x, z);
    if (h != NULL)
        return h;

    h = malloc(sizeof(struct hunk));
    memset(h, 0, sizeof(struct hunk));
    h->x = x;
    h->z = z;
    h->chunk_count = 0;
    w->hunks[w->hunk_count++] = h;
    generate_hunk_noise_maps(w, h);
    return h;
}

static struct chunk *alloc_chunk(struct world *w, int x, int y, int z, struct hunk *hunk, int index) {
    assert(w);
    assert(hunk);

    if (w->chunk_count >= w->chunk_cap) {
        fprintf(stderr, "add_chunk: cannot add more chunks, capacity reached (%d)\n", w->chunk_cap);
        return NULL;
    }

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
    chunk_initialize(c, (ivec3){x, y, z}, 0);

    w->chunks[w->chunk_count++] = c;
    hunk->chunks[index] = c;
    hunk->chunk_count++;
    return c;
}

static void *worker_func(void *arg) {
    struct world *w = (struct world *)arg;
    while (!w->initialized) {
        printf("waiting for world to initialize...\n");
        thread_sleep(100);
    }

    struct work work;
    while (true) {
        pop_work(w, &work);
        if (work.func)
            work.func(work.chunk, w, &work);
    }

    return NULL;
}

static void gen_chunk_mesh_fn(struct chunk *c, struct world *w, struct work *self) {
    assert(c != NULL);

    // check the neighbours
    for (int face = 0; face < 6; face++) {
        struct chunk *n = c->neighbours[face];
        if (n && n->state < C_STATE_GENERATED) {
            // printf("deferring meshing of chunk (%d, %d, %d) because neighbour (%d, %d, %d) is not ready\n", c->x, c->y, c->z, n->x, n->y, n->z);
            //  requeue this work item
            self->priority++;
            push_work(w, *self);
            return;
        }
    }

    c->state = C_STATE_MESHING;
    void *quads = malloc(C_SIZE3 * 6 * sizeof(struct mesh_quad));
    size_t quad_count = chunk_generate_mesh_data(c, quads);

    if (quad_count > 0) {
        // worldrnd_upload(&w->rnd, c, quads, quad_count * sizeof(struct mesh_quad));
        if (!mesh_batch_alloc(w->batch, quads, quad_count * sizeof(struct mesh_quad), &c->mesh_handle)) {
            fprintf(stderr, "gen_chunk_mesh_fn: failed to allocate mesh batch for chunk (%d, %d, %d)\n", c->pos.x, c->pos.y, c->pos.z);
        }
    }

    c->state = C_STATE_MESHED;
    free(quads);
}

static void gen_chunk_data_fn(struct chunk *c, struct world *w, struct work *self) {
    assert(self->is_column);

    struct chunk *top_chunk = world_get_chunk(w, self->column.x, self->column.max_y, self->column.z);
    struct chunk *chunk = top_chunk;

    for (int y = self->column.max_y; y >= self->column.min_y && chunk; y--) {
        if (w->flatgrass)
            gen_chunk_base_flatgrass(w, chunk);
        else
            gen_chunk_base(w, chunk);

        chunk = chunk->neighbours[DIR_DOWN];
    }

    chunk = top_chunk;
    for (int y = self->column.max_y; y >= self->column.min_y && chunk; y--) {
        gen_chunk_surface(w, chunk);
        gen_chunk_caves(w, chunk);

        chunk->state = C_STATE_GENERATED;

        if (self->in_meshing_dist && chunk->fill_count > 0) {
            push_work(w, (struct work){.chunk = chunk, .is_column = false, .func = gen_chunk_mesh_fn, .priority = WORK_MESH_PRIORITY_BASE});
        }

        chunk = chunk->neighbours[DIR_DOWN];
    }
}

struct chunk *world_get_chunk(struct world *w, int x, int y, int z) {
    int hx, _hy, hz;
    int lx, ly, lz;
    relative_pos(x, y, z, H_SIZE, &hx, &_hy, &hz, &lx, &ly, &lz);
    struct hunk *hunk = get_hunk(w, hx, hz);
    if (!hunk)
        return NULL;
    int index = u_index_zxy(lx, ly, lz, H_SIZE);
    return hunk->chunks[index];
}

static void set_block_internal(struct world *w, struct chunk *c, int lx, int ly, int lz, block_t block) {
    if (!c)
        return;

    assert(lx >= 0 && lx < C_SIZE);
    assert(ly >= 0 && ly < C_SIZE);
    assert(lz >= 0 && lz < C_SIZE);

    if (!chunk_set_block(c, lx, ly, lz, block)) {
        return;
    }

    int wx = (c->pos.x * C_SIZE) + lx;
    int wy = (c->pos.y * C_SIZE) + ly;
    int wz = (c->pos.z * C_SIZE) + lz;
    struct hunk *hunk = get_hunk(w, u_idiv(c->pos.x, H_SIZE), u_idiv(c->pos.z, H_SIZE));
    if (!hunk) {
        fprintf(stderr, "set_block_internal: hunk for chunk (%d, %d) not found\n", c->pos.x, c->pos.z);
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

void world_init(struct world *w, int seed, int num_worker_threads, bool flatgrass) {
    memset(w, 0, sizeof(struct world));

    const int SIZE = 256;
    w->seed = seed;
    w->flatgrass = flatgrass;

    w->chunk_count = 0;
    w->chunk_cap = SIZE * SIZE * H_SIZE;
    w->chunks = malloc(sizeof(struct chunk) * w->chunk_cap);
    if (!w->chunks) {
        fprintf(stderr, "world_init: failed to allocate chunk array\n");
        exit(EXIT_FAILURE);
    }

    w->hunk_count = 0;
    w->hunk_cap = 8;
    w->hunks = malloc(sizeof(struct hunk *) * w->hunk_cap);
    if (!w->hunks) {
        fprintf(stderr, "world_init: failed to allocate hunk array\n");
        exit(EXIT_FAILURE);
    }

    w->batch = mesh_batch_init("chunk_mesh_batch", MB(32), KB(2), H_SIZE3, sizeof(struct instance_data));

    // work queue
    pqueue_init(&(w->work_que), 512, sizeof(struct work), work_cmp);
    mutex_init(&w->work_lock);
    cond_init(&w->work_cond);
    printf("World initialized with seed %d\n", seed);

    w->num_worker_threads = num_worker_threads;
    thread_t thread;
    for (int i = 0; i < w->num_worker_threads; i++) {
        thread_init(&thread, worker_func, w);
        thread_detach(thread);
    }

    w->initialized = true;
}

// max_y is inclusive
static void push_column_work(struct world *w, int x, int z, int min_y, int max_y, int priority, bool in_meshing_dist) {
    struct work work = {0};
    work.is_column = true;
    work.column.x = x;
    work.column.z = z;
    work.column.min_y = min_y;
    work.column.max_y = max_y;
    work.func = gen_chunk_data_fn;
    work.priority = priority;
    work.in_meshing_dist = in_meshing_dist;
    push_work(w, work);
}

static int get_chunk_column_priority(int x, int z, int center_x, int center_z) {
    int dx = x - center_x;
    int dz = z - center_z;
    return (dx * dx) + (dz * dz);
}

void world_generate_initial(struct world *w, int load_radius) {
    assert(w->initialized);

    if (load_radius <= 0)
        load_radius = 1;

    const int size = (1 + (load_radius + 1) * 2);
    int t = size;
    int max_iter = t * t;

    if (max_iter >= w->chunk_cap) {
        fprintf(stderr, "world_generate_initial: too many chunks (%d) for current capacity (%d)\n", max_iter, w->chunk_cap);
        return;
    }

    // we should these coordinates for future use
    ivec2 *columns = malloc(sizeof(ivec2) * max_iter);
    int cind = 0;
    int x, z, dx, dz;
    x = z = dx = 0;
    dz = -1;

    for (int i = 0; i < max_iter; i++) {
        if ((-size / 2 <= x) && (x <= size / 2) && (-size / 2 <= z) && (z <= size / 2))
            columns[cind++] = (ivec2){x, z};
        if ((x == z) || ((x < 0) && (x == -z)) || ((x > 0) && (x == 1 - z))) {
            t = dx;
            dx = -dz;
            dz = t;
        }
        x += dx;
        z += dz;
    }

    for (int i = 0; i < cind; i++) {
        ivec2 col = columns[i];
        for (int y = 0; y < H_SIZE; y++) {
            int hx, _hy, hz;
            int lx, ly, lz;
            relative_pos(col.x, y, col.y, H_SIZE, &hx, &_hy, &hz, &lx, &ly, &lz);

            int index = u_index_zxy(lx, ly, lz, H_SIZE);
            struct hunk *h = alloc_hunk(w, hx, hz);
            alloc_chunk(w, col.x, y, col.y, h, index);
        }
    }

    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        for (int face = 0; face < 6; face++) {
            int dx = c->pos.x + FACE_NORMALS[face].x;
            int dy = c->pos.y + FACE_NORMALS[face].y;
            int dz = c->pos.z + FACE_NORMALS[face].z;
            struct chunk *neighbour = world_get_chunk(w, dx, dy, dz);
            chunk_set_neighbour(c, neighbour, face);
        }
    }

    printf("generating %d chunks\n", w->chunk_count);

    for (int i = 0; i < cind; i++) {
        ivec2 col = columns[i];
        const int ax = abs(col.x);
        const int az = abs(col.y);
        bool in_meshing_dist = (ax > az ? ax : az) <= load_radius;
        push_column_work(w, col.x, col.y, 0, H_SIZE - 1, get_chunk_column_priority(col.x, col.y, 0, 0), in_meshing_dist);
    }

    free(columns);
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

bool world_is_working(struct world *w) {
    return !pqueue_is_empty(&w->work_que);
}

void world_draw(struct world *w, struct cam *cam, GLuint program, GLuint atlas, int atlas_depth) {
    // worldrnd_clear(&w->rnd);
    mesh_batch_clear_commands(w->batch);

    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        if (c->fill_count <= 0 || !c->mesh_handle.valid)
            continue;

        if (cam_aabb_in_view(cam, c->min, c->max)) {
            struct mesh_quad *ptr = mesh_batch_handle_ptr(&c->mesh_handle);
            struct draw_command cmd = {
                .baseInstance = 0,
                .instanceCount = 1,
                .first = mesh_batch_calculate_offset(w->batch, &c->mesh_handle, sizeof(struct mesh_quad), 6),
                .count = c->quad_count * 6,
            };

            struct instance_data data = {
                .pos = iv3_scale(c->pos, C_SIZE),
                .tint = C32_CLEAR,
            };

            mesh_batch_add_command(w->batch, &cmd, &data);
            dbg_aabb(c->min, c->max, C32_WHITE);
        }
    }

    dbg_fade(false);
    for (int i = 0; i < w->hunk_count; i++) {
        struct hunk *h = w->hunks[i];
        vec3 hmin = {h->x * HW_SIZE, 0, h->z * HW_SIZE};
        vec3 hmax = {hmin.x + HW_SIZE, H_SIZE * C_SIZE, hmin.z + HW_SIZE};
        dbg_aabb(hmin, hmax, C32_GREEN);
    }
    dbg_fade(true);

    glUseProgram(program);
    glBindTextureUnit(0, atlas);
    glUniformMatrix4fv(glGetUniformLocation(program, "vp"), 1, GL_FALSE, cam_view_proj_ptr(cam));
    glUniform1ui(glGetUniformLocation(program, "atlas_depth"), (u32)atlas_depth);

    static GLuint empty_vao = GL_NONE;
    if (empty_vao == GL_NONE)
        glGenVertexArrays(1, &empty_vao);
    glBindVertexArray(empty_vao);

    mesh_batch_multidraw(w->batch, 0, 1);
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

static void validate_chunk_neighbour_sync(struct chunk *c) {
    for (int face = 0; face < 6; face++) {
        struct chunk *n = c->neighbours[face];
        if (!n)
            continue;
        int opposite_face = (face % 2 == 0) ? face + 1 : face - 1;
        if (n->neighbours[opposite_face] != c) {
            printf("Chunk (%d, %d, %d) neighbour %d does not point back to it\n", c->pos.x, c->pos.y, c->pos.z, face);
        }
    }
}

static void validate_chunk_padded_block_count(struct chunk *c) {
    int count = 0;
    for (int z = 0; z < C_SIZE; z++) {
        for (int x = 0; x < C_SIZE; x++) {
            for (int y = 0; y < C_SIZE; y++) {
                block_t b = chunk_get_block(c, x, y, z);
                if (b != BLK_AIR)
                    count++;
            }
        }
    }

    if (count != c->fill_count) {
        printf("Chunk (%d, %d, %d) fill count mismatch: expected %d, got %d\n", c->pos.x, c->pos.y, c->pos.z, c->fill_count, count);
    }
}

static void validate_right_pos_in_hunk(struct world *w, struct chunk *c) {
    int hx, _hy, hz;
    int lx, ly, lz;
    relative_pos(c->pos.x, c->pos.y, c->pos.z, H_SIZE, &hx, &_hy, &hz, &lx, &ly, &lz);
    struct hunk *hunk = get_hunk(w, hx, hz);
    if (!hunk) {
        printf("Chunk (%d, %d, %d) hunk (%d, %d) not found\n", c->pos.x, c->pos.y, c->pos.z, hx, hz);
        return;
    }
    int index = u_index_zxy(lx, ly, lz, H_SIZE);
    if (hunk->chunks[index] != c) {
        printf("Chunk (%d, %d, %d) not found in its hunk (%d, %d) at index (%d)\n", c->pos.x, c->pos.y, c->pos.z, hx, hz, index);
    }
}

static void validate_hunk_chunk_counts(struct world *w) {
    for (int i = 0; i < w->hunk_count; i++) {
        struct hunk *h = w->hunks[i];
        int actual_chunk_count = 0;
        for (int j = 0; j < H_SIZE3; j++) {
            if (h->chunks[j])
                actual_chunk_count++;
        }
        if (actual_chunk_count != h->chunk_count) {
            printf("Hunk (%d, %d) chunk count mismatch: expected %d, got %d\n", h->x, h->z, h->chunk_count, actual_chunk_count);
        }
    }
}

void validate_world(struct world *w) {

    assert(w->initialized);
    while (world_is_working(w))
        thread_sleep(100);

    printf("=== Validating world with %d chunks and %d hunks ===\n", w->chunk_count, w->hunk_count);
    for (int i = 0; i < w->chunk_count; i++) {
        validate_chunk_neighbour_sync(w->chunks[i]);
        validate_chunk_padded_block_count(w->chunks[i]);
        validate_right_pos_in_hunk(w, w->chunks[i]);
    }

    validate_hunk_chunk_counts(w);
    printf("=== World validation complete ===\n");
}

#define NUM_JUMPS_XZ 8
#define NUM_JUMPS_Y 4
#define AXIS_STEPS_XZ (C_SIZE / NUM_JUMPS_XZ + 1)
#define AXIS_STEPS_Y (C_SIZE / NUM_JUMPS_Y + 1)
#define SAMPLE_VOLUME (AXIS_STEPS_XZ * AXIS_STEPS_Y * AXIS_STEPS_XZ)
#define axis_step_index(xi, yi, zi) ((zi) * AXIS_STEPS_XZ * AXIS_STEPS_Y + (xi) * AXIS_STEPS_Y + (yi))

static inline float sample_noise(const float *buf, int x, int y, int z) {

    assert(buf);
    assert(x >= 0 && x < C_SIZE && y >= 0 && y < C_SIZE && z >= 0 && z < C_SIZE);

    const int x0 = x / NUM_JUMPS_XZ;
    const int y0 = y / NUM_JUMPS_Y;
    const int z0 = z / NUM_JUMPS_XZ;

    // if we are on a direct spot than we don't need to interpolate
    if (x % NUM_JUMPS_XZ == 0 && y % NUM_JUMPS_Y == 0 && z % NUM_JUMPS_XZ == 0)
        return buf[z0 * AXIS_STEPS_XZ * AXIS_STEPS_Y + x0 * AXIS_STEPS_Y + y0];

    const float sx = (float)(x % NUM_JUMPS_XZ) / NUM_JUMPS_XZ;
    const float sy = (float)(y % NUM_JUMPS_Y) / NUM_JUMPS_Y;
    const float sz = (float)(z % NUM_JUMPS_XZ) / NUM_JUMPS_XZ;

    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const int z1 = z0 + 1;
    const float c000 = buf[axis_step_index(x0, y0, z0)];
    const float c100 = buf[axis_step_index(x1, y0, z0)];
    const float v00 = u_lerp(c000, c100, sx);

    const float c010 = buf[axis_step_index(x0, y1, z0)];
    const float c110 = buf[axis_step_index(x1, y1, z0)];
    const float v10 = u_lerp(c010, c110, sx);

    const float c001 = buf[axis_step_index(x0, y0, z1)];
    const float c101 = buf[axis_step_index(x1, y0, z1)];
    const float v01 = u_lerp(c001, c101, sx);

    const float c011 = buf[axis_step_index(x0, y1, z1)];
    const float c111 = buf[axis_step_index(x1, y1, z1)];
    const float v11 = u_lerp(c011, c111, sx);

    const float v0 = u_lerp(v00, v10, sy);
    const float v1 = u_lerp(v01, v11, sy);
    return u_lerp(v0, v1, sz);
}

static void sample_initial_noise(float *samples, struct chunk *c, fnl_state *state) {
    for (int z = 0; z < AXIS_STEPS_XZ; z++) {
        for (int x = 0; x < AXIS_STEPS_XZ; x++) {
            for (int y = 0; y < AXIS_STEPS_Y; y++) {
                int dx = (c->pos.x * C_SIZE + (x * NUM_JUMPS_XZ));
                int dy = (c->pos.y * C_SIZE + (y * NUM_JUMPS_Y));
                int dz = (c->pos.z * C_SIZE + (z * NUM_JUMPS_XZ));
                int index = axis_step_index(x, y, z);
                samples[index] = fnlGetNoise3D(state, dx, dy, dz);
            }
        }
    }
}

static void gen_chunk_base(struct world *w, struct chunk *c) {

    const int ocean_level = 105;

    fnl_state density_state = fnlCreateState(w->seed);
    density_state.noise_type = FNL_NOISE_OPENSIMPLEX2;
    density_state.fractal_type = FNL_FRACTAL_FBM;
    density_state.octaves = 3;
    density_state.lacunarity = 2.0f;
    density_state.gain = 0.5f;
    density_state.frequency = 0.01f;

    float density_samples[SAMPLE_VOLUME];
    sample_initial_noise(density_samples, c, &density_state);

    fnl_state squash_state = fnlCreateState(w->seed + 1);
    squash_state.noise_type = FNL_NOISE_OPENSIMPLEX2;
    squash_state.frequency = 0.002f;

    fnl_state height_state = fnlCreateState(w->seed + 2);
    height_state.noise_type = FNL_NOISE_OPENSIMPLEX2;
    height_state.fractal_type = FNL_FRACTAL_RIDGED;
    height_state.octaves = 4;
    height_state.lacunarity = 2.0f;
    height_state.gain = 0.5f;
    height_state.weighted_strength = 0.5f;
    height_state.frequency = 0.002f;

    const int base_world_height = 128;
    const float min_squash = 0.5f;
    const float max_squash = 8.0f;
    const float height_scale = 64.0f;

    const float min_height = base_world_height - height_scale;
    const float max_height = base_world_height + height_scale;
    const float height_range = max_height - min_height;
    uint32_t seed = chunk_seed(c);

    struct hunk *hunk = get_hunk_for_chunk(w, c);

    for (int z = 0; z < C_SIZE; z++) {
        for (int x = 0; x < C_SIZE; x++) {
            int wx = x + c->pos.x * C_SIZE;
            int wz = z + c->pos.z * C_SIZE;

            float temp = hunk_get_temperature(hunk, wx, wz);
            float rain = hunk_get_rainfall(hunk, wx, wz);

            //  temp = 0.0f;
            //   rain = 0.0f;

            float climate_height_mod = (1.0f - temp) * rain; // 0 to 1
            float adjusted_height_scale = height_scale * (0.5f + climate_height_mod);

            float height_noise = fnlGetNoise2D(&height_state, wx, wz);
            height_noise = (height_noise + 1.0f) * 0.5f;
            float terrain_height = base_world_height + (height_noise - 0.5f) * adjusted_height_scale * 2.0f;

            float squash_noise = fnlGetNoise2D(&squash_state, wx, wz);
            squash_noise = (squash_noise + 1.0f) * 0.5f;

            float climate_squash_mod = 1.0f + rain * 2.0f; // 1 to 3
            float squash_factor = (min_squash + (max_squash - min_squash) * squash_noise) * climate_squash_mod;

            float climate_factor = temp * (1.0f - rain);
            float terrain_amplification = 1.0f + climate_factor * 0.5f;

            for (int y = 0; y < C_SIZE; y++) {
                int wy = y + c->pos.y * C_SIZE;
                float height_normalized = (terrain_height - wy) / height_range;
                float density_bias = height_normalized * squash_factor * terrain_amplification;
                float density = sample_noise(density_samples, x, y, z);

                block_t block = BLK_AIR;
                if (wy == 0 || wy < rng_next(&seed) % 5) {
                    block = BLK_BASE;
                } else if (density + density_bias > 0.0f) {
                    block = BLK_STONE;
                } else if (wy < ocean_level) {
                    block = BLK_OCEAN;
                }

                set_block_internal(w, c, x, y, z, block);
            }
        }
    }
}

static void gen_chunk_base_flatgrass(struct world *w, struct chunk *c) {
    const int flatgrass_height = 30;
    for (int z = 0; z < C_SIZE; z++) {
        for (int x = 0; x < C_SIZE; x++) {
            for (int y = 0; y < C_SIZE; y++) {
                block_t block = BLK_AIR;
                int wy = y + c->pos.y * C_SIZE;
                if (wy == 0) {
                    block = BLK_BASE;
                } else if (wy < flatgrass_height) {
                    block = BLK_STONE;
                }
                set_block_internal(w, c, x, y, z, block);
            }
        }
    }
}

struct local_pos {
    int x, y, z;
    struct chunk *c;
};

void local_pos_up(struct local_pos *it) {
    it->y++;
    if (it->y >= C_SIZE) {
        it->y = 0;
        it->c = it->c->neighbours[DIR_UP];
    }
}

void local_pos_down(struct local_pos *it) {
    it->y--;
    if (it->y < 0) {
        it->y = C_SIZE - 1;
        it->c = it->c->neighbours[DIR_DOWN];
    }
}

bool check_air_above(struct local_pos *it, int n) {
    for (int i = 0; i < n; i++) {
        if (it->c == NULL)
            return true;

        if (chunk_get_block(it->c, it->x, it->y, it->z) != BLK_AIR)
            return false;

        local_pos_up(it);
    }
    return true;
}

static void gen_chunk_surface(struct world *w, struct chunk *c) {

    const int ocean_level = 105;

    const int required_air_above = 12;
    const int min_dirt_depth = 2;
    const int max_dirt_depth = 5;

    fnl_state dirt_state = fnlCreateState(w->seed + 10);
    dirt_state.noise_type = FNL_NOISE_PERLIN;
    dirt_state.frequency = 0.15f;

    const block_t surface_block = BLK_GRASS;
    const block_t subsurface_block = BLK_DIRT;

    // check 8 blocks above and see if they are air, if so, replace with grass and dirt
    for (int z = 0; z < C_SIZE; z++) {
        for (int x = 0; x < C_SIZE; x++) {

            int steps_down = 0;
            for (int y = C_SIZE - 1; y >= 0; y--) {
                block_t block = chunk_get_block(c, x, y, z);
                if (block != BLK_STONE) {
                    steps_down++;
                    continue;
                }

                if ((c->pos.y * C_SIZE + y) < ocean_level) {
                    // underwater stone should not be replaced with grass
                    break;
                }

                // would be the first stone in the column
                // check if we have 8 blocks of air above
                // if so it's a surface block and we can replace it with grass and move down with some dirt
                struct local_pos pos_a = {.x = x, .y = y, .z = z, .c = c};

                if (steps_down < required_air_above) {
                    // we already know there is enough air above
                    local_pos_up(&pos_a);
                    if (!check_air_above(&pos_a, required_air_above))
                        break;
                }

                set_block_internal(w, c, x, y, z, surface_block);
                float dirt_t = fnlGetNoise2D(&dirt_state, (c->pos.x * C_SIZE + x) * 0.5f, (c->pos.z * C_SIZE + z) * 0.5f);
                dirt_t = (dirt_t + 1.0f) * 0.5f;
                int dirt_depth = min_dirt_depth + (int)((max_dirt_depth - min_dirt_depth) * u_clamp(dirt_t, 0.0f, 1.0f));

                struct local_pos pos_b = {.x = x, .y = y, .z = z, .c = c};
                local_pos_down(&pos_b);

                for (int d = 0; d < dirt_depth; d++) {
                    if (pos_a.c == NULL)
                        break;
                    block_t b = chunk_get_block(pos_b.c, pos_b.x, pos_b.y, pos_b.z);
                    if (b != BLK_STONE)
                        break;
                    set_block_internal(w, pos_b.c, pos_b.x, pos_b.y, pos_b.z, subsurface_block);
                    local_pos_down(&pos_b);
                }
                break;
            }
        }
    }
}

static void gen_chunk_caves(struct world *w, struct chunk *c) {
    (void)w;
    (void)c;
}
