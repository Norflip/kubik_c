#ifndef LOAD_H
#define LOAD_H
#include <glad/gl.h>

#define asset_path(x) (KU_ASSET_PATH x)

GLuint load_program(const char *name, const char *vspath, const char *fspath);
GLuint load_atlas(const char *name, const char **paths, int count, int width, int height, int capacity, int *added_count);

#endif