#include "chunk.h"
#include "queue.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define C_CANARY 0xBABEDAD

#define ASSERT_CHUNK(c)                                          \
    assert(c != NULL);                                           \
    assert(c->_canary0 == C_CANARY && "Chunk canary0 mismatch"); \
    assert(c->_canary1 == C_CANARY && "Chunk canary1 mismatch");

static inline int get_index(int x, int y, int z) {
    return u_index_zxy(x + 1, y + 1, z + 1, C_SIZEP);
}

void chunk_initialize(struct chunk *c, ivec3 pos, enum chunk_flag flags) {
    memset(c, 0, sizeof(struct chunk));
    c->pos = pos;
    c->_canary0 = C_CANARY;
    c->_canary1 = C_CANARY;
    c->tint = C32_CLEAR;

    c->min = (vec3){c->pos.x * C_SIZE, c->pos.y * C_SIZE, c->pos.z * C_SIZE};
    c->max = v3_addf(c->min, C_SIZE);

    c->blocks = NULL;
    c->state = C_STATE_EMPTY;
    c->flags = flags;
    mutex_init(&c->lock);
}

static struct chunk *get_neighbour(struct chunk *c, int face) {
    if (c == NULL || face < 0 || face >= DIR_COUNT)
        return NULL;
    return c->neighbours[face];
}

static inline struct chunk *find_neighbour(struct chunk *chunk, int dx, int dy, int dz) {
    struct chunk *current = chunk;

    if (dx > 0)
        current = current->neighbours[DIR_RIGHT];
    else if (dx < 0)
        current = current->neighbours[DIR_LEFT];

    if (!current)
        return NULL;

    if (dy > 0)
        current = current->neighbours[DIR_UP];
    else if (dy < 0)
        current = current->neighbours[DIR_DOWN];

    if (!current)
        return NULL;

    if (dz > 0)
        current = current->neighbours[DIR_FRONT];
    else if (dz < 0)
        current = current->neighbours[DIR_BACK];

    return current;
}

static bool try_alloc_block_data(struct chunk *c) {
    ASSERT_CHUNK(c);
    mutex_lock(&c->lock);

    if (c->blocks != NULL) {
        mutex_unlock(&c->lock);
        return true;
    }

    c->blocks = calloc(C_SIZEP3, sizeof(struct block_state));
    if (c->blocks == NULL) {
        fprintf(stderr, "chunk_set_block: failed to allocate memory for chunk blocks\n");
        mutex_unlock(&c->lock);
        return false;
    }

    mutex_unlock(&c->lock);
    return true;
}

static inline uint8_t get_boundary_mask(int x, int y, int z, int *count) {
    uint8_t mask = 0;
    *count = 0;
    if (x == C_SIZE - 1) {
        mask |= (1 << DIR_RIGHT); // +X boundary
        (*count)++;
    } else if (x == 0) {
        mask |= (1 << DIR_LEFT); // -X boundary
        (*count)++;
    }

    if (y == C_SIZE - 1) {
        mask |= (1 << DIR_UP); // +Y boundary
        (*count)++;
    } else if (y == 0) {
        mask |= (1 << DIR_DOWN); // -Y boundary
        (*count)++;
    }

    if (z == C_SIZE - 1) {
        mask |= (1 << DIR_FRONT); // +Z boundary
        (*count)++;
    } else if (z == 0) {
        mask |= (1 << DIR_BACK); // -Z boundary
        (*count)++;
    }

    return mask;
}

static void set_padded_blockstate(struct chunk *c, int x, int y, int z, struct block_state state) {
    if (!pos_on_border(x, y, z))
        return;

    ASSERT_CHUNK(c);
    int boundary_count = 0;
    uint8_t mask = get_boundary_mask(x, y, z, &boundary_count);
    if (mask == 0 || boundary_count == 0)
        return;

    struct chunk *n;
    for (int face = 0; face < DIR_COUNT; face++) {
        n = c->neighbours[face];
        if ((mask & (1 << face)) && n) {
            // chunk_iterator would kinda do the same thing here?
            int tx = x;
            int ty = y;
            int tz = z;

            switch (face) {
            case DIR_RIGHT:
                tx = -1;
                break;
            case DIR_LEFT:
                tx = C_SIZE;
                break;
            case DIR_UP:
                ty = -1;
                break;
            case DIR_DOWN:
                ty = C_SIZE;
                break;
            case DIR_FRONT:
                tz = -1;
                break;
            case DIR_BACK:
                tz = C_SIZE;
                break;
            }

            // if (c->blocks != NULL) {
            if (try_alloc_block_data(n)) {
                // memcpy(&n->blocks[get_index(tx, ty, tz)], value, sizeof(struct block_state));
                n->blocks[get_index(tx, ty, tz)] = state;

                n->dirty = true;
            }
            // }
        }
    }

    /*
    static const int edge_combinations[12][2] = {
        {DIR_RIGHT, DIR_UP},    {DIR_RIGHT, DIR_DOWN}, // +X edges
        {DIR_LEFT, DIR_UP},     {DIR_LEFT, DIR_DOWN},  // -X edges
        {DIR_UP, DIR_FRONT},    {DIR_UP, DIR_BACK},    // +Y edges
        {DIR_DOWN, DIR_FRONT},  {DIR_DOWN, DIR_BACK},  // -Y edges
        {DIR_RIGHT, DIR_FRONT}, {DIR_RIGHT, DIR_BACK}, // +X Z edges
        {DIR_LEFT, DIR_FRONT},  {DIR_LEFT, DIR_BACK}   // -X Z edges
    };

    static const int corner_combinations[8][3] = {
        {DIR_RIGHT, DIR_UP, DIR_FRONT},   // +X,+Y,+Z
        {DIR_RIGHT, DIR_UP, DIR_BACK},    // +X,+Y,-Z
        {DIR_RIGHT, DIR_DOWN, DIR_FRONT}, // +X,-Y,+Z
        {DIR_RIGHT, DIR_DOWN, DIR_BACK},  // +X,-Y,-Z
        {DIR_LEFT, DIR_UP, DIR_FRONT},    // -X,+Y,+Z
        {DIR_LEFT, DIR_UP, DIR_BACK},     // -X,+Y,-Z
        {DIR_LEFT, DIR_DOWN, DIR_FRONT},  // -X,-Y,+Z
        {DIR_LEFT, DIR_DOWN, DIR_BACK}    // -X,-Y,-Z
    };

    // update edges
    if (boundary_count >= 2) {
        for (int i = 0; i < 12; i++) {
            int face1 = edge_combinations[i][0];
            int face2 = edge_combinations[i][1];

            if ((mask & (1 << face1)) && (mask & (1 << face2))) {
                // Calculate neighbour direction
                int dx = (face1 == DIR_RIGHT) ? 1 : (face1 == DIR_LEFT) ? -1 : 0;
                int dy = (face1 == DIR_UP || face2 == DIR_UP) ? 1 : (face1 == DIR_DOWN || face2 == DIR_DOWN) ? -1 : 0;
                int dz = (face1 == DIR_FRONT || face2 == DIR_FRONT) ? 1 : (face1 == DIR_BACK || face2 == DIR_BACK) ? -1 : 0;

                struct chunk *n = find_neighbour(c, dx, dy, dz);
                if (n) {
                    int tx = (dx == 1) ? -1 : (dx == -1) ? C_SIZE : x;
                    int ty = (dy == 1) ? -1 : (dy == -1) ? C_SIZE : y;
                    int tz = (dz == 1) ? -1 : (dz == -1) ? C_SIZE : z;

                    // if (n->blocks != NULL) {
                    if (try_alloc_block_data(n)) {
                        // memcpy(&n->blocks[get_index(tx, ty, tz)], value, sizeof(struct block_state));
                        n->blocks[get_index(tx, ty, tz)] = state;

                        n->dirty = true;
                    }
                    // }
                }
            }
        }
    }

    // update_corners
    if (boundary_count >= 3) {

        for (int i = 0; i < 8; i++) {
            int face1 = corner_combinations[i][0];
            int face2 = corner_combinations[i][1];
            int face3 = corner_combinations[i][2];

            if ((mask & (1 << face1)) && (mask & (1 << face2)) && (mask & (1 << face3))) {
                int dx = (face1 == DIR_RIGHT) ? 1 : -1;
                int dy = (face2 == DIR_UP) ? 1 : -1;
                int dz = (face3 == DIR_FRONT) ? 1 : -1;

                struct chunk *n = find_neighbour(c, dx, dy, dz);
                if (n) {
                    int tx = (dx == 1) ? -1 : C_SIZE;
                    int ty = (dy == 1) ? -1 : C_SIZE;
                    int tz = (dz == 1) ? -1 : C_SIZE;

                    // if (n->blocks != NULL) {
                    if (try_alloc_block_data(n)) {
                        // memcpy(&n->blocks[get_index(tx, ty, tz)], value, sizeof(struct block_state));
                        n->blocks[get_index(tx, ty, tz)] = state;
                        n->dirty = true;
                    }
                    // }
                }
            }
        }
    }
        */
}

bool chunk_set_block(struct chunk *c, int x, int y, int z, block_t block, bool player_triggered) {
    ASSERT_CHUNK(c);
    if (!pos_inside(x, y, z)) {
        fprintf(stderr, "chunk_set_block: position (%d, %d, %d) is out of bounds in chunk (%d, %d, %d)\n", x, y, z, c->pos.x, c->pos.y, c->pos.z);
        return false;
    }

    if (block == BLK_AIR && c->blocks == NULL)
        return true;

    if (!try_alloc_block_data(c))
        return false;

    const int index = get_index(x, y, z);
    struct block_state old_state = c->blocks[index];
    if (old_state.block != 0 && block == 0) {
        c->fill_count--;
    } else if (old_state.block == 0 && block != 0) {
        c->fill_count++;
    }

    const light_t light_level = block_default_light(block);
    struct block_state new_state = {.block = block, .light = light_level};
    c->blocks[index] = new_state;
    set_padded_blockstate(c, x, y, z, new_state);

    if (player_triggered && old_state.light != light_level) {
        if (old_state.light > 0)
            rem_light_tmp(c, x, y, z, old_state.light);
        if (light_level > 0)
            add_light_tmp(c, x, y, z);
    }

    c->dirty = true;
    return true;
}

block_t chunk_get_block(struct chunk *c, int x, int y, int z) {
    ASSERT_CHUNK(c);
    if (!pos_inside_padded(x, y, z))
        return BLK_AIR;
    if (c->blocks == NULL)
        return BLK_AIR;
    return c->blocks[get_index(x, y, z)].block;
}

bool chunk_block_is_solid(struct chunk *c, int x, int y, int z) {
    return block_is_solid(chunk_get_block(c, x, y, z));
}

ivec3 chunk_world_pos(struct chunk *c) {
    ASSERT_CHUNK(c);
    return (ivec3){c->pos.x * C_SIZE, c->pos.y * C_SIZE, c->pos.z * C_SIZE};
}

// FNV-1a hash https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
uint32_t chunk_seed(struct chunk *c) {
    ASSERT_CHUNK(c);
    const uint32_t prime = 16777619u;
    const uint32_t basis = 2166136261u;
    uint32_t hash = basis;
    hash ^= (uint32_t)c->pos.x;
    hash *= prime;
    hash ^= (uint32_t)c->pos.y;
    hash *= prime;
    hash ^= (uint32_t)c->pos.z;
    hash *= prime;
    return hash;
}

void chunk_set_neighbour(struct chunk *c, struct chunk *neighbour, int face) {
    ASSERT_CHUNK(c);
    assert(face >= 0 && face < DIR_COUNT);
    if (c == neighbour) {
        fprintf(stderr, "chunk_link: chunk is linking to itself\n");
        return;
    }

    c->neighbours[face] = neighbour;
    if (neighbour) {
        neighbour->neighbours[dir_opposite(face)] = c;
    }
}

static struct mesh_quad make_quad(int x, int y, int z, int face, block_t block, light_t light, u8 ao[4], bool flipped) {
    struct mesh_quad quad = {0};
    quad.pos_face = (x & 0x3F) | ((y & 0x3F) << 6) | ((z & 0x3F) << 12);
    quad.pos_face |= (face & 0x7) << 18;
    quad.pos_face |= ((ao[0] & 0x3) << 21) | ((ao[1] & 0x3) << 23) | ((ao[2] & 0x3) << 25) | ((ao[3] & 0x3) << 27);
    quad.pos_face |= (flipped & 0x1) << 29;
    quad.light = light;

    int texture_index = block_texture_index(block, face);
    assert(texture_index != NO_TEXTURE_INDEX);
    quad.texid = texture_index;
    return quad;
}

#define ao_value(s1, c, s2) ((solids[s1] && solids[s2]) ? 1 : (3 - (solids[s1] + solids[s2] + solids[c])))

static void get_face_ao(struct chunk *c, int x, int y, int z, int face, uint8_t *ao_out, bool *flip) {
    assert(face >= 0 && face < DIR_COUNT);
    // counter clockwise
    // took fucking ages
    const ivec3 ao_offsets[DIR_COUNT][8] = {{{1, -1, -1}, {1, -1, 0}, {1, -1, 1}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}, {1, 1, -1}, {1, 0, -1}}, {{-1, -1, 1}, {-1, -1, 0}, {-1, -1, -1}, {-1, 0, -1}, {-1, 1, -1}, {-1, 1, 0}, {-1, 1, 1}, {-1, 0, 1}},
                                            {{-1, 1, -1}, {0, 1, -1}, {1, 1, -1}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}, {-1, 1, 1}, {-1, 1, 0}}, {{-1, -1, -1}, {-1, -1, 0}, {-1, -1, 1}, {0, -1, 1}, {1, -1, 1}, {1, -1, 0}, {1, -1, -1}, {0, -1, -1}},
                                            {{1, -1, 1}, {0, -1, 1}, {-1, -1, 1}, {-1, 0, 1}, {-1, 1, 1}, {0, 1, 1}, {1, 1, 1}, {1, 0, 1}}, {{-1, -1, -1}, {0, -1, -1}, {1, -1, -1}, {1, 0, -1}, {1, 1, -1}, {0, 1, -1}, {-1, 1, -1}, {-1, 0, -1}}};
    bool solids[8];
    for (int i = 0; i < 8; i++) {
        int ox = x + ao_offsets[face][i].x;
        int oy = y + ao_offsets[face][i].y;
        int oz = z + ao_offsets[face][i].z;
        solids[i] = chunk_block_is_solid(c, ox, oy, oz);
    }

    ao_out[0] = ao_value(7, 0, 1); // BOT LEFT
    ao_out[1] = ao_value(1, 2, 3); // BOT RIGHT
    ao_out[2] = ao_value(3, 4, 5); // TOP LEFT
    ao_out[3] = ao_value(5, 6, 7); // TOP RIGHT
    *flip = !((ao_out[0] + ao_out[2] < ao_out[1] + ao_out[3]));
}

size_t chunk_generate_mesh_data(struct chunk *c, void *buffer) {
    ASSERT_CHUNK(c);
    assert(buffer != NULL);
    if (c->fill_count <= 0 || !c->dirty)
        return 0;

    if (!c->blocks) {
        c->quad_count = 0;
        c->dirty = false;
        return 0;
    }

    struct mesh_quad *quads = buffer;

    size_t added = 0;
    u8 ao[4] = {0, 0, 0, 0};
    for (int y = 0; y < C_SIZE; y++) {
        for (int x = 0; x < C_SIZE; x++) {
            for (int z = 0; z < C_SIZE; z++) {
                const block_t block = chunk_get_block(c, x, y, z);
                if (!block_is_solid(block))
                    continue;

                for (int face = 0; face < DIR_COUNT; face++) {
                    ivec3 normal = FACE_NORMALS[face];
                    int ox = x + normal.x;
                    int oy = y + normal.y;
                    int oz = z + normal.z;

                    const block_t adjacent_block = chunk_get_block(c, ox, oy, oz);
                    if (block_create_face(block, face, adjacent_block)) {
                        bool flip = false;
                        get_face_ao(c, x, y, z, face, ao, &flip);

                        light_t light = c->blocks[get_index(ox, oy, oz)].light;
                        struct mesh_quad quad = make_quad(x, y, z, face, block, light, ao, flip);
                        quads[added++] = quad;
                    }
                }
            }
        }
    }

    c->quad_count = added;
    c->dirty = false;
    return added;
}

#define TMP_LIGHT_MASK 31 // (1 << 5) - 1

struct light_node {
    iterator_t it;
    light_t light;
};

static inline struct block_state *get_block_state_ptr(struct chunk *c, int x, int y, int z) {
    ASSERT_CHUNK(c);
    if (!pos_inside_padded(x, y, z))
        return NULL;
    if (c->blocks == NULL)
        return NULL;
    return &c->blocks[get_index(x, y, z)];
}

static inline bool is_solid_blockstate(struct block_state *state) {
    return state != NULL && block_is_solid(state->block);
}

// temporary light functions, will be replaced with proper multi channel light system later
static inline bool light_lessthan(light_t a, light_t b) {
    // return a < b;
    return (a & TMP_LIGHT_MASK) < (b & TMP_LIGHT_MASK);
}

static inline light_t light_decrement(light_t light) {
    light_t l = (light & TMP_LIGHT_MASK) - 1;
    return (l > 0) ? l : 0;
}

static inline light_t light_max(light_t a, light_t b) {
    return ((a & TMP_LIGHT_MASK) > (b & TMP_LIGHT_MASK)) ? a : b;
}

static inline light_t light_greater_equal(light_t a, light_t b) {
    return light_lessthan(b, a) || (a & TMP_LIGHT_MASK) == (b & TMP_LIGHT_MASK);
}

static void add_floodfill_light(queue_t *q) {
    struct light_node node;
    while (!queue_empty(q)) {
        queue_dequeue(q, &node);
        struct block_state *current_state = iterator_get_block_state_ptr(&node.it);
        if (current_state == NULL) {
            fprintf(stderr, "tmp_floodfill_light: current state is NULL at (%d, %d, %d) in chunk (%d, %d, %d)\n", node.it.x, node.it.y, node.it.z, node.it.chunk->pos.x, node.it.chunk->pos.y, node.it.chunk->pos.z);
            continue;
        }

        // printf("Adding light at (%d, %d, %d) in chunk (%d, %d, %d) with light %u\n", node.it.x, node.it.y, node.it.z, node.it.chunk->pos.x, node.it.chunk->pos.y, node.it.chunk->pos.z, node.light);
        light_t declvl = light_decrement(current_state->light);
        if (declvl == 0)
            continue;

        for (int face = 0; face < DIR_COUNT; face++) {
            iterator_t it = node.it;
            iterator_move(&it, face);

            struct block_state *neighbour_state = iterator_get_block_state_ptr(&it);
            if (neighbour_state == NULL) {
                fprintf(stderr, "null neighbour block_state\n");
                continue;
            }

            if (!block_is_solid(neighbour_state->block)) {
                if (light_lessthan(neighbour_state->light, declvl)) {
                    neighbour_state->light = light_max(neighbour_state->light, declvl);
                    set_padded_blockstate(it.chunk, it.x, it.y, it.z, *neighbour_state);
                    it.chunk->dirty = true;

                    struct light_node neighbour_node;
                    neighbour_node.it = it;
                    neighbour_node.light = declvl;
                    //  printf("add_floodfill_light: Enqueuing light addition at (%d, %d, %d) in chunk (%d, %d, %d) with light %u\n", it.x, it.y, it.z, it.chunk->pos.x, it.chunk->pos.y, it.chunk->pos.z, declvl);
                    queue_enqueue(q, &neighbour_node);
                }
            }
        }
    }
}

static void rem_floodfill_light(queue_t *remq, queue_t *addq) {
    struct light_node node;

    while (!queue_empty(remq)) {
        queue_dequeue(remq, &node);
        struct block_state *current_state = iterator_get_block_state_ptr(&node.it);
        if (current_state == NULL) {
            fprintf(stderr, "null block_state\n");
            continue;
        }

        for (int face = 0; face < DIR_COUNT; face++) {
            iterator_t it = node.it;
            iterator_move(&it, (enum dir)face);

            struct block_state *neighbour_state = iterator_get_block_state_ptr(&it);
            if (neighbour_state == NULL) {
                fprintf(stderr, "null neighbour block_state\n");
                continue;
            }

            light_t neighbour_level = neighbour_state->light;
            if (neighbour_level != 0 && light_lessthan(neighbour_level, node.light)) {
                // set the light level
                neighbour_state->light = 0;
                set_padded_blockstate(it.chunk, it.x, it.y, it.z, *neighbour_state);

                struct light_node neighbour_node;
                neighbour_node.it = it;
                neighbour_node.light = neighbour_level;
                queue_enqueue(remq, &neighbour_node);
            } else if ( light_greater_equal(neighbour_level, node.light)) {
                struct light_node neighbour_node;
                neighbour_node.it = it;
                queue_enqueue(addq, &neighbour_node);
            }
        }
    }
}

void add_light_tmp(struct chunk *c, int x, int y, int z) {
    queue_t add_queue = queue_make(sizeof(struct light_node), C_SIZEP3);

    struct light_node node;
    node.it = iterator_make(c, x, y, z);
    printf("Starting light update from (%d, %d, %d) in chunk (%d, %d, %d) with light %u\n", x, y, z, c->pos.x, c->pos.y, c->pos.z, node.light);
    queue_enqueue(&add_queue, &node);
    add_floodfill_light(&add_queue);
    queue_destroy(&add_queue);
}

void rem_light_tmp(struct chunk *c, int x, int y, int z, light_t old_level) {
    queue_t rem_queue = queue_make(sizeof(struct light_node), C_SIZEP3);
    queue_t add_queue = queue_make(sizeof(struct light_node), C_SIZEP3);

    struct light_node node;
    node.it = iterator_make(c, x, y, z);
    node.light = old_level;
    queue_enqueue(&rem_queue, &node);

    printf("Starting light removal update from (%d, %d, %d) in chunk (%d, %d, %d) with light %u\n", x, y, z, c->pos.x, c->pos.y, c->pos.z, old_level);
    rem_floodfill_light(&rem_queue, &add_queue);
    add_floodfill_light(&add_queue);
    queue_destroy(&rem_queue);
    queue_destroy(&add_queue);
}

iterator_t iterator_make(struct chunk *c, int x, int y, int z) {
    iterator_t it;
    it.chunk = c;
    it.x = x;
    it.y = y;
    it.z = z;
    return it;
}

void iterator_up(iterator_t *it) {
    it->y++;
    if (it->y >= C_SIZE) {
        it->y = 0;
        it->chunk = it->chunk->neighbours[DIR_UP];
    }
}

void iterator_down(iterator_t *it) {
    it->y--;
    if (it->y < 0) {
        it->y = C_SIZE - 1;
        it->chunk = it->chunk->neighbours[DIR_DOWN];
    }
}

void iterator_move(iterator_t *it, enum dir dir) {
    switch (dir) {
    case DIR_UP:
        iterator_up(it);
        break;
    case DIR_DOWN:
        iterator_down(it);
        break;
    case DIR_LEFT:
        it->x--;
        if (it->x < 0) {
            it->x = C_SIZE - 1;
            it->chunk = it->chunk->neighbours[DIR_LEFT];
        }
        break;
    case DIR_RIGHT:
        it->x++;
        if (it->x >= C_SIZE) {
            it->x = 0;
            it->chunk = it->chunk->neighbours[DIR_RIGHT];
        }
        break;
    case DIR_FRONT:
        it->z++;
        if (it->z >= C_SIZE) {
            it->z = 0;
            it->chunk = it->chunk->neighbours[DIR_FRONT];
        }
        break;
    case DIR_BACK:
        it->z--;
        if (it->z < 0) {
            it->z = C_SIZE - 1;
            it->chunk = it->chunk->neighbours[DIR_BACK];
        }
        break;
    default:
        break;
    }
}

void iterator_update(iterator_t *it) {
    if (it->y < 0) {
        it->y = C_SIZE - 1;
        it->chunk = it->chunk->neighbours[DIR_DOWN];
    } else if (it->y >= C_SIZE) {
        it->y = 0;
        it->chunk = it->chunk->neighbours[DIR_UP];
    }

    if (it->x < 0) {
        it->x = C_SIZE - 1;
        it->chunk = it->chunk->neighbours[DIR_LEFT];
    } else if (it->x >= C_SIZE) {
        it->x = 0;
        it->chunk = it->chunk->neighbours[DIR_RIGHT];
    }

    if (it->z >= C_SIZE) {
        it->z = 0;
        it->chunk = it->chunk->neighbours[DIR_FRONT];
    } else if (it->z < 0) {
        it->z = C_SIZE - 1;
        it->chunk = it->chunk->neighbours[DIR_BACK];
    }
}

bool iterator_is_valid(iterator_t *it) {
    return it->chunk != NULL && pos_inside(it->x, it->y, it->z);
}

struct block_state *iterator_get_block_state_ptr(iterator_t *it) {
    if (!iterator_is_valid(it))
        return NULL;
    return get_block_state_ptr(it->chunk, it->x, it->y, it->z);
}
