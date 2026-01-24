#include "gfx.h"
#include "load.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct linevert {
    vec3 pos;
    col32_t col;
};

static GLuint l_program;
static GLuint l_vbo;
static struct linevert *l_mapped;
static size_t l_count, l_cap;
static GLuint empty_vao;

// static const struct cam *g_active_cam = NULL;

static const mat4 *g_view_proj;
static vec3 g_eye_pos;
static vec4 g_frustum[6];
static bool g_lines_enabled = true;
static bool l_fade_enabled = true;

void gfx_initialize(void) {
    glGenVertexArrays(1, &empty_vao);

    g_view_proj = NULL;
    g_eye_pos = V3_ZERO;

    l_program = load_program("lines", asset_path("line.vs"), asset_path("line.fs"));
    l_cap = MAX_LINES;
    l_count = 0;
    l_fade_enabled = true;

    GLint alignment = GL_NONE;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
    const GLsizei size_aligned = u_align((GLsizei)(l_cap * (sizeof(struct linevert) * 2)), alignment);

    const GLbitfield mapping_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;
    const GLbitfield storage_flags = GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
    glCreateBuffers(1, &(l_vbo));
    glNamedBufferStorage(l_vbo, size_aligned, NULL, storage_flags);
    l_mapped = glMapNamedBufferRange(l_vbo, 0, size_aligned, mapping_flags);
}

void gfx_shutdown(void) {
    glDeleteBuffers(1, &l_vbo);
    glDeleteProgram(l_program);
}

void gfx_start_frame(const mat4 *view_proj, vec4 frustum[6], vec3 eye_pos) {
    g_view_proj = view_proj;
    g_eye_pos = eye_pos;
    memcpy(g_frustum, frustum, sizeof(vec4) * 6);
    dbg_clear();
}

void gfx_end_frame(void) {
    // render lines
    if (l_count > 0) {
        const size_t size = l_count * sizeof(struct linevert) * 2;
        // glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
        glUseProgram(l_program);

        glUniformMatrix4fv(glGetUniformLocation(l_program, "vp"), 1, GL_FALSE, (const GLfloat *)g_view_proj);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, l_vbo);
        glFlushMappedNamedBufferRange(l_vbo, 0, size);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthFunc(GL_LEQUAL);

        glBindVertexArray(empty_vao);
        glDrawArrays(GL_LINES, 0, l_count * 2);
        glDisable(GL_BLEND);

        l_count = 0;
    }
}

void gfx_draw_world(struct world *w, GLuint program, GLuint atlas, int atlas_depth) {
    mesh_buffer_clear_commands(w->batch);

    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        if (c->quad_count <= 0 || c->fill_count <= 0 || !c->mesh_handle.valid)
            continue;

        if (frustum_aabb_test(g_frustum, c->bounds)) {

            // struct mesh_quad *ptr = mesh_buffer_handle_ptr(&c->mesh_handle);
            struct draw_command cmd = {
                .baseInstance = 0,
                .instanceCount = 1,
                .first = mesh_buffer_calculate_offset(w->batch, &c->mesh_handle, sizeof(struct mesh_quad), 6),
                .count = c->quad_count * 6,
            };

            /*
            struct instance_data data = {
                .pos = iv3_scale(c->pos, C_SIZE),
                .tint = C32_CLEAR,
            };
            */

            ivec3 pos = iv3_scale(c->pos, C_SIZE);
            int data[4];
            data[0] = pos.x;
            data[1] = pos.y;
            data[2] = pos.z;
            data[3] = c->tint;

#if 1
            mesh_buffer_add_command(w->batch, &cmd, data);
#else
            mesh_buffer_add_command2(w->batch, &c->mesh_handle, c->quad_count, &data);
#endif
            dbg_aabb(c->bounds, C32_WHITE);
        }
    }

    dbg_fade(false);
    for (int i = 0; i < w->hunk_count; i++) {
        struct hunk *h = w->hunks[i];
        aabb_t bounds = world_get_hunk_bounds(w, h);
        dbg_aabb(bounds, C32_GREEN);
    }
    dbg_fade(true);

    glUseProgram(program);
    glBindTextureUnit(0, atlas);
    glUniformMatrix4fv(glGetUniformLocation(program, "vp"), 1, GL_FALSE, (const GLfloat *)g_view_proj);
    glUniform1ui(glGetUniformLocation(program, "atlas_depth"), (u32)atlas_depth);
    glUniform4f(glGetUniformLocation(program, "eye_pos_light"), g_eye_pos.x, g_eye_pos.y, g_eye_pos.z, 0.5f);

    static GLuint empty_vao = GL_NONE;
    if (empty_vao == GL_NONE)
        glGenVertexArrays(1, &empty_vao);
    glBindVertexArray(empty_vao);

    mesh_buffer_multidraw(w->batch, 0, 1);
}

void gfx_enable_lines(bool enable) {
    g_lines_enabled = enable;
}

static col32_t adjust_alpha(col32_t color, float distance) {
    distance = u_min(distance, LINE_FADE_MAX_DISTANCE) / LINE_FADE_MAX_DISTANCE;
    distance = u_clamp(distance, 0.0f, 1.0f);
    uint8_t alpha = (uint8_t)(0xFF * (1.0f - distance));
    return (color & 0xFFFFFF00) | (alpha);
}

void dbg_line(vec3 start, vec3 end, col32_t color) {
    if (l_count >= l_cap - 1 || !g_lines_enabled)
        return;

    if (l_fade_enabled) {
        float len0 = v3_len(v3_sub(start, g_eye_pos));
        float len1 = v3_len(v3_sub(end, g_eye_pos));
        if (len0 > LINE_FADE_MAX_DISTANCE && len1 > LINE_FADE_MAX_DISTANCE)
            return;
        l_mapped[l_count * 2 + 0] = (struct linevert){start, adjust_alpha(color, len0)};
        l_mapped[l_count * 2 + 1] = (struct linevert){end, adjust_alpha(color, len1)};
    } else {
        l_mapped[l_count * 2 + 0] = (struct linevert){start, color};
        l_mapped[l_count * 2 + 1] = (struct linevert){end, color};
    }

    l_count++;
}

void dbg_aabb(aabb_t a, col32_t color) {

    if (l_count >= l_cap - 1 || !g_lines_enabled)
        return;

    static const vec3 BOX_CORNERS[8] = {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}, {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}};
    vec3 corners[8];
    for (int i = 0; i < 8; i++) {
        corners[i] = (vec3){BOX_CORNERS[i].x * (a.max.x - a.min.x) + a.min.x, BOX_CORNERS[i].y * (a.max.y - a.min.y) + a.min.y, BOX_CORNERS[i].z * (a.max.z - a.min.z) + a.min.z};
    }

    int j = (4 - 1);
    for (int i = 0; i < 4; i++) {
        dbg_line(corners[i], corners[j], color);
        dbg_line(corners[i + 4], corners[j + 4], color);
        dbg_line(corners[i], corners[i + 4], color);
        j = i;
    }
}

void dbg_cube(vec3 pos, vec3 ext, col32_t color) {
    vec3 min = v3_sub(pos, ext);
    vec3 max = v3_add(pos, ext);
    dbg_aabb((aabb_t){min, max}, color);
}

void dbg_fade(bool enable) {
    l_fade_enabled = enable;
}

void dbg_clear(void) {
    l_count = 0;
}
