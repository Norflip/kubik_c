#ifndef GFX_H
#define GFX_H
#include "cam.h"
#include "maf.h"
#include "utils.h"
#include <glad/gl.h>

#define MAX_LINES 8192

#ifndef KU_ASSET_PATH
#    define KU_ASSET_PATH ""
#endif
#define asset_path(x) (KU_ASSET_PATH x)

void gfx_initialize(void);
void gfx_shutdown(void);
void dbg_line(vec3 start, vec3 end, col32_t color);
void dbg_aabb(vec3 min, vec3 max, col32_t color);
void dbg_cube (vec3 pos, vec3 ext, col32_t color);
void gfx_tmp_draw_lines(struct cam *c);

GLuint gfx_load_program(const char *name, const char *vspath, const char *fspath);
GLuint gfx_load_atlas(const char *name, const char **paths, int count, int width, int height, int capacity, int *added_count);

#endif