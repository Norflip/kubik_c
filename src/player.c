#include "player.h"
#include "utils.h"
#include <GLFW/glfw3.h>
#include <string.h>

#define MOUSE_SENSITIVITY 0.05f
#define BOOST_MULTIPLIER 6.0f
#define BASE_SPEED 30.0f
#define MAX_RAY_DISTANCE 100.0f
#define NEAR_Z 0.1f
#define FAR_Z 1000.0f

static bool g_mouse_state[GLFW_MOUSE_BUTTON_LAST][2] = {0};
#define mousekey_down(key) (g_mouse_state[key][0] && !g_mouse_state[key][1])

static void update_projection(struct player *player) {
    float aspect = (float)player->width / (float)player->height;
    player->proj = m4_perspective(player->fov, aspect, NEAR_Z, FAR_Z);
    player->dirty = true;
}

static void update_view(struct player *player) {
    player->view = m4_look_dir(player->pos, quat_mulv(player->rot, (vec3){0, 0, 1}), (vec3){0, 1, 0});
    player->view_proj = m4_mul(player->proj, player->view);
    frustum_make(player->view_proj, player->frustum);
}

static void handle_pos_input(struct player *player, float dt);
static void handle_click_input(struct player *player);

void player_init(struct player *player, struct world *world, void *window, float fov) {
    memset(player, 0, sizeof(struct player));
    player->fov = fov;
    player->window = window;
    glfwGetWindowSize((GLFWwindow *)window, &player->width, &player->height);
    player->world = world;
    player->pos = (vec3){0, 0, 0};
    player->rot = quat_identity();
    player->dirty = true;
    update_projection(player);
    update_view(player);
}

void player_resize_viewport(struct player *player, int width, int height) {
    player->width = width;
    player->height = height;
    update_projection(player);
}

void player_set_pos(struct player *player, vec3 pos) {
    player->pos = pos;
    player->dirty = true;
}

void player_select_block(struct player *player, block_t block) {
    assert(block < BLK_COUNT);
    player->selected_block = block;
}

void player_tick(struct player *player, double dt, bool can_capture_mouse) {
    for (u64 i = 0; i < GLFW_MOUSE_BUTTON_LAST; i++) {
        g_mouse_state[i][1] = g_mouse_state[i][0];                                      // previous = current
        g_mouse_state[i][0] = glfwGetMouseButton(player->window, (int)i) == GLFW_PRESS; // current = new
    }
    handle_pos_input(player, (float)dt);
    if (can_capture_mouse)
        handle_click_input(player);
}

mat4 const *player_view_proj_ptr(struct player *player) {
    if (player->dirty) {
        update_view(player);
        player->dirty = false;
    }
    return &player->view_proj;
}

void handle_pos_input(struct player *player, float dt) {
    float rot_x = 0;
    float rot_y = 0;

    double xpos, ypos;
    glfwGetCursorPos(player->window, &xpos, &ypos);
    if (glfwGetMouseButton(player->window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        if (!player->mouse_captured) {
            glfwSetInputMode(player->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            player->last_x = xpos;
            player->last_y = ypos;
            player->mouse_captured = true;
            if (!player->first_frame) {
                vec3 forward = quat_mulv(player->rot, V3_FORWARD);
                player->pitch = asinf(-forward.y);
                player->yaw = atan2f(forward.x, forward.z);
                player->first_frame = true;
            }
        } else {
            rot_x = (xpos - player->last_x) * MOUSE_SENSITIVITY;
            rot_y = (player->last_y - ypos) * MOUSE_SENSITIVITY;
            player->last_x = xpos;
            player->last_y = ypos;
        }
    } else if (player->mouse_captured) {
        glfwSetInputMode(player->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        player->mouse_captured = false;
        player->last_x = 0;
        player->last_y = 0;
    }

    if (rot_x != 0 || rot_y != 0) {
        player->yaw += to_rad(-rot_x);
        player->pitch += to_rad(-rot_y);

        const float MAX_PITCH = to_rad(89.999f);
        player->pitch = u_clamp(player->pitch, -MAX_PITCH, MAX_PITCH);
        vec4 q_yaw = quat_axis_angle(V3_UP, player->yaw);
        vec4 q_pitch = quat_axis_angle(V3_RIGHT, player->pitch);
        player->rot = quat_mul(q_yaw, q_pitch);

        player->dirty = true;
    }

    int zt = 0;
    zt -= (glfwGetKey(player->window, GLFW_KEY_W) == GLFW_PRESS);
    zt += (glfwGetKey(player->window, GLFW_KEY_S) == GLFW_PRESS);

    int xt = 0;
    xt -= (glfwGetKey(player->window, GLFW_KEY_A) == GLFW_PRESS);
    xt += (glfwGetKey(player->window, GLFW_KEY_D) == GLFW_PRESS);
    int yt = 0;
    yt -= (glfwGetKey(player->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
    yt += (glfwGetKey(player->window, GLFW_KEY_SPACE) == GLFW_PRESS);

    if (xt != 0 || yt != 0 || zt != 0) {
        vec3 forward = quat_mulv(player->rot, V3_FORWARD);
        vec3 right = v3_normalize(v3_cross(forward, V3_UP));

        vec3 transl = {0};
        transl = v3_scale(right, xt);
        transl = v3_add(transl, v3_scale(forward, -zt));
        transl = v3_add(transl, v3_scale(V3_UP, yt));

        float speed = BASE_SPEED;
        if (glfwGetKey(player->window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
            speed *= BOOST_MULTIPLIER;

        vec3 translation = v3_scale(v3_normalize(transl), speed * dt);
        player->pos = v3_add(player->pos, translation);
    }

    player->dirty = true;
}

static void get_ray(struct player *player, vec3 *out_origin, vec3 *out_dir) {

    if (player->mouse_captured) {
        *out_origin = player->pos;
        *out_dir = quat_mulv(player->rot, V3_FORWARD);
        return;
    }

    double mouse_x, mouse_y;
    glfwGetCursorPos(player->window, &mouse_x, &mouse_y);
    float ndc_x = (2.0f * mouse_x) / (float)player->width - 1.0f;
    float ndc_y = (2.0f * mouse_y) / (float)player->height - 1.0f;

    vec4 ray_clip = (vec4){ndc_x, ndc_y, -1.0f, 1.0f};
    vec4 ray_eye = m4_mulv4(m4_inverse(player->proj), ray_clip);
    ray_eye = (vec4){ray_eye.x, ray_eye.y, -1.0f, 0.0f};

    vec3 ray_world = quat_mulv(player->rot, (vec3){ray_eye.x, ray_eye.y, ray_eye.z});
    ray_world = v3_normalize(ray_world);
    ray_world = v3_scale(ray_world, -1.0f);

    *out_origin = player->pos;
    *out_dir = ray_world;
}

void handle_click_input(struct player *player) {
    vec3 ro, rd;
    get_ray(player, &ro, &rd);
    world_trace(player->world, ro, rd, MAX_RAY_DISTANCE, &player->last_hit);
    if (!player->last_hit.did_hit)
        return;

    if (mousekey_down(GLFW_MOUSE_BUTTON_MIDDLE)) {
        player->selected_block = player->last_hit.block;
    } else if (mousekey_down(GLFW_MOUSE_BUTTON_LEFT)) {
        struct world_hit hit = player->last_hit;

        // check glfw for shift
        if (glfwGetKey(player->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(player->window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS || player->selected_block == BLK_AIR) {
            // remove block
            world_set_block(player->world, hit.pos.x, hit.pos.y, hit.pos.z, BLK_AIR);
        } else {
            // place block
            ivec3 place_pos = hit.pos;
            place_pos.x += hit.normal.x;
            place_pos.y += hit.normal.y;
            place_pos.z += hit.normal.z;
            world_set_block(player->world, place_pos.x, place_pos.y, place_pos.z, player->selected_block);
        }
    }
}