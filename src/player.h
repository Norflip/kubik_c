#ifndef PLAYER_H
#define PLAYER_H
#include "block.h"
#include "maf.h"
#include "world.h"
#include <stdbool.h>

struct player {
    vec3 pos;
    vec4 rot;

    float fov;
    int width, height;
    mat4 proj, view;
    mat4 view_proj;
    vec4 frustum[6];

    struct world *world;
    struct world_hit last_hit;
    block_t selected_block;

    bool dirty;
    bool mouse_captured;
    float last_x, last_y;
    float yaw, pitch;
    bool first_frame;
    void *window;
};

void player_init(struct player *player, struct world *world, void *window, float fov);
void player_resize_viewport(struct player *player, int width, int height);
void player_set_pos(struct player *player, vec3 pos);
void player_select_block(struct player *player, block_t block);
void player_tick(struct player *player, double dt, bool can_capture_mouse);

mat4 const *player_view_proj_ptr(struct player *player);

#endif // PLAYER_H