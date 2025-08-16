#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define LOG_SIZE 256

#define FLIP_LINE_Z 1

struct linevert {
    vec3 pos;
    col32_t col;
};

static GLuint l_program;
static GLuint l_vbo;
static struct linevert *l_mapped;
static size_t l_count, l_cap;

static GLuint empty_vao;

static inline size_t file_size(FILE *f) {
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    return size;
}

static GLuint load_shader(const char *name, const char *path, GLenum type) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "could not open file: \"%s\"\n", path);
        return GL_NONE;
    }

    size_t size = file_size(file);
    char *content = malloc(size + 1);
    if (!content) {
        fprintf(stderr, "could not load shader file: \"%s\"\n", path);
        fclose(file);
        return GL_NONE;
    }
    fread(content, 1, size, file);
    content[size] = '\0';

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, (const GLchar **)&content, NULL);
    glCompileShader(shader);

    int params = -1;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &params);
    if (params != GL_TRUE) {
        char error_log[LOG_SIZE];
        glGetShaderInfoLog(shader, LOG_SIZE, NULL, error_log);
        fprintf(stderr, "GL shader %s (%i) failed to compile\n", path, shader);
        fprintf(stderr, "Shader info log for GL index %u:\n\t%s", shader, error_log);
        return GL_NONE;
    }

    return shader;
}

void gfx_initialize(void) {
    glGenVertexArrays(1, &empty_vao);

    l_program = gfx_load_program("lines", asset_path("line.vs"), asset_path("line.fs"));
    l_cap = MAX_LINES;
    l_count = 0;

    const GLbitfield mapping_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;
    const GLbitfield storage_flags = GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
    GLint alignment = GL_NONE;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
    const GLsizei size_aligned = u_align((GLsizei)(l_cap * (sizeof(struct linevert) * 2)), alignment);

    glCreateBuffers(1, &(l_vbo));
    glNamedBufferStorage(l_vbo, size_aligned, NULL, storage_flags);
    l_mapped = glMapNamedBufferRange(l_vbo, 0, size_aligned, mapping_flags);
}

void gfx_shutdown(void) {
    glDeleteBuffers(1, &l_vbo);
    glDeleteProgram(l_program);
}

void dbg_line(vec3 start, vec3 end, col32_t color) {
    if (l_count >= l_cap - 1)
        return;
#if FLIP_LINE_Z
    start.z = -start.z;
    end.z = -end.z;
#endif

    l_mapped[l_count * 2 + 0] = (struct linevert){start, color};
    l_mapped[l_count * 2 + 1] = (struct linevert){end, color};
    l_count++;
}

void dbg_aabb(vec3 min, vec3 max, col32_t color) {
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

void gfx_tmp_draw_lines(struct cam *c) {

    if (l_count == 0)
        return;

    const size_t size = l_count * sizeof(struct linevert) * 2;

    // flush the memory that covers the added lines
    // glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    glUseProgram(l_program);

    glUniformMatrix4fv(glGetUniformLocation(l_program, "vp"), 1, GL_FALSE, cam_view_proj_ptr(c));
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

GLuint gfx_load_program(const char *name, const char *vspath, const char *fspath) {
    GLuint vs_shader, fs_shader;
    vs_shader = load_shader(name, vspath, GL_VERTEX_SHADER);
    fs_shader = load_shader(name, fspath, GL_FRAGMENT_SHADER);

    if (vs_shader == GL_NONE || fs_shader == GL_NONE) {
        fprintf(stderr, "Could not load shader program %s [%s, %s]\n", name, vspath, fspath);
        return GL_NONE;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs_shader);
    glAttachShader(program, fs_shader);
    glLinkProgram(program);
    glDeleteShader(vs_shader);
    glDeleteShader(fs_shader);

    int params = -1;
    glGetProgramiv(program, GL_LINK_STATUS, &params);
    if (GL_TRUE != params) {
        char error_log[LOG_SIZE];
        glGetProgramInfoLog(program, LOG_SIZE, NULL, error_log);
        fprintf(stderr, "Could not link shader program %s [%s, %s] GL index %u", name, vspath, fspath, program);
        fprintf(stderr, "Program log: %s", error_log);
        return GL_NONE;
    }

    glObjectLabel(GL_PROGRAM, program, -1, name);
    printf("loaded program \"%s\"\n", name);
    return program;
}
