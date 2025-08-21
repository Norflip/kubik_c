#ifndef CAM_H
#define CAM_H
#include "maf.h"
#include <stdbool.h>

struct cam {
    vec3 pos;
    vec4 rot;
    float fov, aspect;
    mat4 proj, view;
    mat4 view_proj;
    bool dirty;
};

void cam_initialize(struct cam *cam, int width, int height, float fov);
void cam_resize(struct cam *cam, int width, int height);
void cam_set_transform(struct cam *cam, vec3 position, vec4 rotation);
void cam_translate(struct cam *cam, vec3 transl);
void cam_look_at(struct cam *cam, vec3 pos);

vec3 cam_relative_vec(const struct cam *cam, vec3 v);


mat4 cam_view_proj(struct cam *cam);
float *cam_view_proj_ptr(struct cam *cam);

void cam_fps_update(struct cam *cam, void *window, double dt);

#endif