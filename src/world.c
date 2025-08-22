#include "world.h"
#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>

static struct chunk *add_chunk(struct world *w, int x, int y, int z) {
    if (w->chunk_count >= w->chunk_cap) {
        return NULL;
    }

    struct chunk *c = malloc(sizeof(struct chunk));
    chunk_initialize(c, x, y, z);
    w->chunks[w->chunk_count++] = c;
    return c;
}

void world_init_demo(struct world *w) {
    w->chunk_count = 0;
    w->chunk_cap = 3 * 3 * 3;
    w->chunks = malloc(sizeof(struct chunk) * w->chunk_cap);
    glGenVertexArrays(1, &w->empty_vao);

    for (int z = 0; z < 3; z++) {
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 3; x++) {
                add_chunk(w, x - 1, y - 1, z - 1);
            }
        }
    }

    printf("World initialized with %d chunks\n", w->chunk_count);

    // collect the neighbours
    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        for (int face = 0; face < 6; face++) {
            int dx = c->x + FACE_NORMALS[face].x;
            int dy = c->y + FACE_NORMALS[face].y;
            int dz = c->z + FACE_NORMALS[face].z;
            struct chunk *neighbour = world_get_chunk(w, dx, dy, dz);
            chunk_link(c, neighbour, face);
        }
    }

    // generate data after neighbours
    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        tmp_fill_chunk_data(c, false);
    }

    // generate mesh after the data is done
    void *mesh_data = NULL;
    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        chunk_generate_mesh_data(c, &mesh_data);
        chunk_upload_mesh_data(c, mesh_data);
    }
}

struct chunk *world_get_chunk(struct world *w, int x, int y, int z) {
    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        if (c->x == x && c->y == y && c->z == z) {
            return c;
        }
    }
    return NULL;
}

void tmp_world_draw(struct world *w, struct cam *cam, GLuint program, GLuint atlas, int atlas_depth) {

    if (w->chunk_count == 0)
        return;

    w->draw_count = 0;
    cam_update(cam);

  
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "vp"), 1, GL_FALSE, cam_view_proj_ptr(cam));
    glUniform1ui(glGetUniformLocation(program, "atlas_depth"), (u32)atlas_depth);
    glBindTextureUnit(0, atlas);

    for (int i = 0; i < w->chunk_count; i++) {
        struct chunk *c = w->chunks[i];
        if (c->quad_count == 0 || c->quad_buffer == GL_NONE)
            continue;
        vec3 min = {(float)c->x * C_SIZE, (float)c->y * C_SIZE, (float)c->z * C_SIZE};
        vec3 max = {(float)(c->x + 1) * C_SIZE, (float)(c->y + 1) * C_SIZE, (float)(c->z + 1) * C_SIZE};

        if (!cam_aabb_in_view(cam, min, max))
            continue;

        glUniform3i(glGetUniformLocation(program, "chunk_pos"), (u32)c->x * C_SIZE, (u32)c->y * C_SIZE, (u32)c->z * C_SIZE);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, c->quad_buffer);
        glBindVertexArray(w->empty_vao);
        glDrawArrays(GL_TRIANGLES, 0, c->quad_count * 6);

        dbg_aabb(min, max, C32_CYAN);
        w->draw_count++;
    }
}
