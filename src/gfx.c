#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include "load.h"

struct linevert {
    vec3 pos;
    col32_t col;
};

static GLuint l_program;
static GLuint l_vbo;
static struct linevert *l_mapped;
static size_t l_count, l_cap;
static GLuint empty_vao;

static const struct cam *g_active_cam = NULL;
static bool g_lines_enabled = true;
static bool l_fade_enabled = true;

struct {
    GLuint ow_query_objects[2]; // Double buffer for async queries
    uint64_t ow_samples_passed;
    uint64_t ow_fragments_written;
    int pingpong_frame_index;
    float ow_ratio;
} stats;

void gfx_initialize(void) {
    glGenVertexArrays(1, &empty_vao);

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

    glGenQueries(2, stats.ow_query_objects);
    stats.pingpong_frame_index = 0;
}

void gfx_shutdown(void) {
    glDeleteBuffers(1, &l_vbo);
    glDeleteProgram(l_program);
}

void gfx_start_frame(const struct cam *c) {
    g_active_cam = c;
    dbg_clear();
    glBeginQuery(GL_SAMPLES_PASSED, stats.ow_query_objects[stats.pingpong_frame_index]);
}

void gfx_end_frame(void) {

    glEndQuery(GL_SAMPLES_PASSED);

    glGetQueryObjectui64v(stats.ow_query_objects[stats.pingpong_frame_index], GL_QUERY_RESULT, &stats.ow_samples_passed);
    stats.ow_ratio = (float)stats.ow_samples_passed / (g_active_cam->width * g_active_cam->height);
    stats.pingpong_frame_index = 1 - stats.pingpong_frame_index;

    if (l_count > 0) {
        const size_t size = l_count * sizeof(struct linevert) * 2;
        // glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
        glUseProgram(l_program);

        glUniformMatrix4fv(glGetUniformLocation(l_program, "vp"), 1, GL_FALSE, cam_view_proj_ptr(g_active_cam));
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

struct gfx_stats gfx_get_stats(void) {
    struct gfx_stats s = {0};
    s.samples_passed = stats.ow_samples_passed;
    s.overdraw_ratio = stats.ow_ratio;
    return s;
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

    if (g_active_cam == NULL) {
        fprintf(stderr, "dbg_line: no active camera set, call gfx_start_frame() first\n");
        return;
    }

    if (l_fade_enabled) {
        float len0 = v3_len(v3_sub(start, g_active_cam->pos));
        float len1 = v3_len(v3_sub(end, g_active_cam->pos));
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

void dbg_aabb(vec3 min, vec3 max, col32_t color) {

    if (l_count >= l_cap - 1 || !g_lines_enabled)
        return;

    if (g_active_cam == NULL) {
        fprintf(stderr, "dbg_aabb: no active camera set, call gfx_start_frame() first\n");
        return;
    }

    static const vec3 BOX_CORNERS[8] = {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}, {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}};
    vec3 corners[8];
    for (int i = 0; i < 8; i++) {
        corners[i] = (vec3){BOX_CORNERS[i].x * (max.x - min.x) + min.x, BOX_CORNERS[i].y * (max.y - min.y) + min.y, BOX_CORNERS[i].z * (max.z - min.z) + min.z};
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
    dbg_aabb(min, max, color);
}

void dbg_fade(bool enable) {
    l_fade_enabled = enable;
}

void dbg_clear(void) {
    l_count = 0;
}
