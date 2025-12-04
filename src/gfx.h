#ifndef GFX_H
#define GFX_H
#include "maf.h"
#include "utils.h"
#include "world.h"
#include <glad/gl.h>

#define MAX_LINES 8192
#define LINE_FADE_MAX_DISTANCE 400.0f

void gfx_initialize(void);
void gfx_shutdown(void);
void gfx_start_frame(const mat4 *view_proj, vec4 frustum[6], vec3 eye_pos);
void gfx_end_frame(void);
void gfx_draw_world(struct world *w, GLuint program, GLuint atlas, int atlas_depth);
void gfx_enable_lines(bool enable);

void dbg_line(vec3 start, vec3 end, col32_t color);
void dbg_aabb(vec3 min, vec3 max, col32_t color);
void dbg_cube(vec3 pos, vec3 ext, col32_t color);
void dbg_fade(bool enable);
void dbg_clear(void);

#endif