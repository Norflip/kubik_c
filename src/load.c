#include "load.h"
#include <stdio.h>
#include <stdlib.h>



#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define LOG_SIZE 256


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

GLuint load_program(const char *name, const char *vspath, const char *fspath) {
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

GLuint load_atlas(const char *name, const char **paths, int count, int width, int height, int capacity, int *added_count) {
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
