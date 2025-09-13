#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define LOG_SIZE 256

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

void gfx_start_frame(const struct cam *c) {
    g_active_cam = c;
    dbg_clear();
}

void gfx_end_frame(void) {
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

    float len0 = v3_len(v3_sub(start, g_active_cam->pos));
    float len1 = v3_len(v3_sub(end, g_active_cam->pos));
    if (len0 > LINE_FADE_MAX_DISTANCE && len1 > LINE_FADE_MAX_DISTANCE)
        return;

    l_mapped[l_count * 2 + 0] = (struct linevert){start, adjust_alpha(color, len0)};
    l_mapped[l_count * 2 + 1] = (struct linevert){end, adjust_alpha(color, len1)};
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

void dbg_clear(void) {
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

GLuint gfx_load_atlas(const char *name, const char **paths, int count, int width, int height, int capacity, int *added_count) {
    if (capacity == -1)
        capacity = count;

    GLuint texarr;
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &texarr);
    glTextureParameteri(texarr, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(texarr, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTextureParameteri(texarr, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(texarr, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureStorage3D(texarr, 1, GL_RGBA8, width, height, capacity);

    int added = 0;
    int w, h, n;

    char *buffer = NULL;
    size_t buffer_size = 0;
    FILE *file;

    /*
        GLuint tmptex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &tmptex);
        glTextureStorage2D(tmptex, 1, GL_RGBA8, w, h);
    */
    for (int i = 0; i < count && added < capacity; i++) {
        file = fopen(paths[i], "rb");
        if (!file) {
            fprintf(stderr, "could not open file: \"%s\"\n", paths[i]);
            continue;
        }

        size_t size = file_size(file);
        if (size >= buffer_size) {
            buffer = realloc(buffer, size);
            if (!buffer) {
                fprintf(stderr, "could not allocate memory for file: \"%s\"\n", paths[i]);
                fclose(file);
                continue;
            }
            buffer_size = size;
        }

        fread(buffer, 1, size, file);
        fclose(file);

        uint8_t *data = stbi_load_from_memory((const unsigned char *)buffer, size, &w, &h, &n, 4);
        if (!data || (w == 0 || h == 0)) {
            fprintf(stderr, "could not load image %s: %s\n", paths[i], stbi_failure_reason());
            continue;
        }

        int cols = w / width;
        int rows = h / height;
        if ((rows * cols) + added > capacity) {
            fprintf(stderr, "atlas \"%s\" is full, cannot add more images\n\tskipping %i tiles from \"%s\"\n", name, rows * cols, paths[i]);
            continue;
        }

#if 1
        GLuint tmptex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &tmptex);
        glTextureStorage2D(tmptex, 1, GL_RGBA8, w, h);
#endif
        glTextureSubImage2D(tmptex, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);
        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                glCopyImageSubData(tmptex, GL_TEXTURE_2D, 0, x * width, y * height, 0, texarr, GL_TEXTURE_2D_ARRAY, 0, 0, 0, added, width, height, 1);
                added++;
            }
        }
        glDeleteTextures(1, &tmptex);
    }

    glGenerateTextureMipmap(texarr);
    glObjectLabel(GL_TEXTURE, texarr, -1, name);
    printf("assembled atlas \"%s\" with %i/%zu images\n", name, added, count);
    if (added_count)
        *added_count = added;
    free(buffer);
    return texarr;
}
