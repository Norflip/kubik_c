#ifndef LOAD_H
#define LOAD_H
#include <glad/gl.h>

#ifdef KU_ASSET_PATH
#    define asset_path(x) (KU_ASSET_PATH x)
#else
#    define asset_path(x) (x)
#    error "KU_ASSET_PATH not defined"
#endif

GLuint load_program(const char *name, const char *vspath, const char *fspath);
GLuint load_atlas(const char *name, const char **paths, int count, int width, int height, int capacity, int *added_count);

#endif