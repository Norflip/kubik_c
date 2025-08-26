#include "cam.h"
#include <GLFW/glfw3.h>
#include <string.h>

#define CAM_NEAR_Z 0.1f
#define CAM_FAR_Z 1000.0f

#define FPS_MOUSE_SENSITIVITY 0.05f
#define FPS_BOOST_MULTIPLIER 6.0f
#define FPS_BASE_SPEED 30.0f

static void create_frustum(mat4 m, vec4 *frustum) {
    // -X plane
    frustum[0].x = m.m[3] + m.m[0];
    frustum[0].y = m.m[7] + m.m[4];
    frustum[0].z = m.m[11] + m.m[8];
    frustum[0].w = m.m[15] + m.m[12];

    // +X plane
    frustum[1].x = m.m[3] - m.m[0];
    frustum[1].y = m.m[7] - m.m[4];
    frustum[1].z = m.m[11] - m.m[8];
    frustum[1].w = m.m[15] - m.m[12];

    // -Y plane
    frustum[2].x = m.m[3] + m.m[1];
    frustum[2].y = m.m[7] + m.m[5];
    frustum[2].z = m.m[11] + m.m[9];
    frustum[2].w = m.m[15] + m.m[13];

    // +Y plane
    frustum[3].x = m.m[3] - m.m[1];
    frustum[3].y = m.m[7] - m.m[5];
    frustum[3].z = m.m[11] - m.m[9];
    frustum[3].w = m.m[15] - m.m[13];

    // inverted Z dir for opengl
    // -Z plane
    frustum[4].x = m.m[3] + m.m[2];
    frustum[4].y = m.m[7] + m.m[6];
    frustum[4].z = m.m[11] + m.m[10];
    frustum[4].w = m.m[15] + m.m[14];

    // +Z plane
    frustum[5].x = m.m[3] - m.m[2];
    frustum[5].y = m.m[7] - m.m[6];
    frustum[5].z = m.m[11] - m.m[10];
    frustum[5].w = m.m[15] - m.m[14];

    // normalize planes
    for (int i = 0; i < 6; i++) {
        float length = sqrtf(frustum[i].x * frustum[i].x + frustum[i].y * frustum[i].y + frustum[i].z * frustum[i].z);
        frustum[i].x /= length;
        frustum[i].y /= length;
        frustum[i].z /= length;
        frustum[i].w /= length;
    }
}

static void update_projection(struct cam *cam) {
    cam->proj = m4_perspective(cam->fov, cam->aspect, CAM_NEAR_Z, CAM_FAR_Z);
    cam->dirty = true;
}

static void update_view(struct cam *cam) {
    cam->view = m4_look_dir(cam->pos, cam_relative_vec(cam, V3_FORWARD), V3_UP);
    cam->view_proj = m4_mul(cam->proj, cam->view);
    create_frustum(cam->view_proj, cam->frustum);
}

void cam_initialize(struct cam *cam, int width, int height, float fov) {
    memset(cam, 0, sizeof(struct cam));
    cam->fov = fov;
    cam->aspect = (float)width / (float)height;
    cam->pos = V3_ZERO;
    cam->rot = quat_identity();
    update_projection(cam);
}

void cam_resize(struct cam *cam, int width, int height) {
    cam->aspect = (float)width / (float)height;
    update_projection(cam);
}

void cam_set_transform(struct cam *cam, vec3 position, vec4 rotation) {
    cam->pos = position;
    cam->rot = rotation;
    cam->dirty = true;
}

void cam_translate(struct cam *cam, vec3 transl) {
    cam->pos = v3_add(cam->pos, transl);
    cam->dirty = true;
}

void cam_look_at(struct cam *cam, vec3 pos) {
    cam->rot = quat_normalize(quat_look_rotation(v3_sub(pos, cam->pos), (vec3){0, 1, 0}));
    cam->dirty = true;
}

vec3 cam_relative_vec(const struct cam *cam, vec3 v) {
    return quat_mulv(cam->rot, v);
}

void cam_update(struct cam *cam) {
    if (cam->dirty) {
        update_view(cam);
        cam->dirty = false;
    }
}

bool cam_aabb_in_view(const struct cam *cam, vec3 min, vec3 max) {
    vec3 corners[8];
    corners[0] = (vec3){min.x, min.y, min.z};
    corners[1] = (vec3){min.x, min.y, max.z};
    corners[2] = (vec3){min.x, max.y, min.z};
    corners[3] = (vec3){min.x, max.y, max.z};
    corners[4] = (vec3){max.x, min.y, min.z};
    corners[5] = (vec3){max.x, min.y, max.z};
    corners[6] = (vec3){max.x, max.y, min.z};
    corners[7] = (vec3){max.x, max.y, max.z};

    for (int i = 0; i < 6; i++) {
        vec4 plane = cam->frustum[i];
        int out = 0;
        for (int j = 0; j < 8; j++) {
            vec3 point = corners[j];
            float dst = plane.x * point.x + plane.y * point.y + plane.z * point.z + plane.w;
            if (dst < 0.0f)
                out++;
        }

        if (out == 8)
            return false;
    }
    return true;
}

mat4 cam_view_proj(struct cam *cam) {
    cam_update(cam);
    return cam->view_proj;
}

float *cam_view_proj_ptr(struct cam *cam) {
    cam_update(cam);
    return cam->view_proj.m;
}

void cam_fps_update(struct cam *cam, void *window, double dt) {
    static bool mouse_captured = false;
    static float lastX = 0, lastY = 0;
    float rot_x = 0;
    float rot_y = 0;

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {

        if (!mouse_captured) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            lastX = xpos;
            lastY = ypos;
            mouse_captured = true;
        } else {
            rot_x = (xpos - lastX) * FPS_MOUSE_SENSITIVITY;
            rot_y = (lastY - ypos) * FPS_MOUSE_SENSITIVITY;
            lastX = xpos;
            lastY = ypos;
        }
    } else if (mouse_captured) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        mouse_captured = false;
        lastX = 0;
        lastY = 0;
    }

#if 0
    // use E and Q to rotate
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        rot_x += fps->rot_speed * dt;
        fps->cam.dirty = true;
    }

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        rot_x -= fps->rot_speed * dt;
        fps->cam.dirty = true;
    }
#endif

    if (rot_x != 0 || rot_y != 0) {
        vec4 q = quat_inverse(cam->rot);
        vec3 up = quat_mulv(q, (vec3){0, 1, 0});
        cam->rot = quat_mul(cam->rot, quat_axis_angle(up, to_rad(-rot_x)));
        cam->rot = quat_mul(cam->rot, quat_axis_angle(V3_RIGHT, to_rad(-rot_y)));
        cam->dirty = true;
    }

    int zt = 0;
    zt -= (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS);
    zt += (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);

    int xt = 0;
    xt -= (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
    xt += (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS);

    int yt = 0;
    yt -= (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
    yt += (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);

    if (xt != 0 || yt != 0 || zt != 0) {

        vec3 forward = cam_relative_vec(cam, V3_FORWARD);
        vec3 right = v3_normalize(v3_cross(forward, V3_UP));

        vec3 transl = {0};
        transl = v3_scale(right, xt);
        transl = v3_add(transl, v3_scale(forward, -zt));
        transl = v3_add(transl, v3_scale(V3_UP, yt));

        float speed = FPS_BASE_SPEED;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
            speed *= FPS_BOOST_MULTIPLIER;

        vec3 translation = v3_scale(v3_normalize(transl), speed * dt);
        cam_translate(cam, translation);
    }

    cam->dirty = true;
}