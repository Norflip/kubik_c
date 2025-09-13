#ifndef MAF_H
#define MAF_H
#include <math.h>
#include <stdint.h>

// general shiet
#define PI (3.14159265359f)
#define TAU (6.28318530718f)
#define RAD (0.01745329251f)
#define DEG (57.2957795131f)
#define to_rad(x) ((x) * RAD)
#define to_deg(x) ((x) * DEG)

// STRUCTS
typedef struct {
    float x, y, z;
} vec3;

typedef struct {
    int x, y, z;
} ivec3;

typedef struct {
    float x, y, z, w;
} vec4;

typedef struct {
    float m[16];
} mat4;

// casts to float*
#define maf_ptr(f) ((float *)&(f))

// vec3
#define V3_ZERO (vec3){0.0f, 0.0f, 0.0f}
#define V3_UP (vec3){0.0f, 1.0f, 0.0f}
#define V3_RIGHT (vec3){1.0f, 0.0f, 0.0f}
#define V3_FORWARD (vec3){0.0f, 0.0f, 1.0f}

#define v3_cast(v)                         \
    (vec3) {                               \
        (float)v.x, (float)v.y, (float)v.z \
    }

vec3 v3_add(vec3 a, vec3 b);
vec3 v3_addf(vec3 v, float f);
vec3 v3_sub(vec3 a, vec3 b);
vec3 v3_scale(vec3 v, float s);
vec3 v3_mul(vec3 a, vec3 b);
vec3 v3_normalize(vec3 v);
vec3 v3_cross(vec3 a, vec3 b);
float v3_dot(vec3 a, vec3 b);
float v3_len(vec3 v);
float v3_lensqr(vec3 v);

// ivec3
ivec3 iv3_add(ivec3 a, ivec3 b);
ivec3 iv3_floor(vec3 v);

// mat4
// http://www.opengl-tutorial.org/beginners-tutorials/tutorial-3-matrices/
// https://learnopengl.com/Getting-started/Transformations
// https://en.wikipedia.org/wiki/Matrix_multiplication_algorithm
// http://www.cg.info.hiroshima-cu.ac.jp/~miyazaki/knowledge/teche23.html
// https://web.archive.org/web/20130304050125/http://www.cg.info.hiroshima-cu.ac.jp/~miyazaki/knowledge/teche23.html

#define m4_index(x, y) (((y) * 4) + (x))
mat4 m4_identity();
mat4 m4_ortho(float left, float right, float bottom, float top, float near, float far);
mat4 m4_perspective(float fov, float aspect, float near, float far);
mat4 m4_look_at(vec3 eye, vec3 center, vec3 up);
mat4 m4_look_dir(vec3 eye, vec3 dir, vec3 up);
mat4 m4_translate(vec3 v);
mat4 m4_scale(vec3 v);
mat4 m4_mul(mat4 a, mat4 b);
vec4 m4_mulv4(mat4 m, vec4 v);
vec3 m4_mulv3(mat4 m, vec3 v);
mat4 m4_inverse(mat4 m);
float m4_determinant(mat4 m);

// quaternion
// this is focked
// https://www.3dgep.com/understanding-quaternions/
// https://cs.stanford.edu/~acoates/quaternion.h
// https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
vec4 quat_identity();
vec4 quat_euler(float pitch, float yaw, float roll);
vec3 quat_to_euler(vec4 q);
vec4 quat_axis_angle(vec3 axis, float angle);
vec4 quat_look_rotation(vec3 forward, vec3 up);
vec4 quat_mul(vec4 a, vec4 b);
vec3 quat_mulv(vec4 q, vec3 v);
mat4 quat_mat4(vec4 q);
vec4 quat_conjugate(vec4 q);
vec4 quat_normalize(vec4 q);
vec4 quat_inverse(vec4 q);

#endif
