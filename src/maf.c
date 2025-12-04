#include "maf.h"
#include <assert.h>

// vec3
vec3 v3_add(vec3 a, vec3 b) {
    return (vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

vec3 v3_addf(vec3 v, float f) {
    return (vec3){v.x + f, v.y + f, v.z + f};
}

vec3 v3_sub(vec3 a, vec3 b) {
    return (vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

vec3 v3_scale(vec3 v, float s) {
    return (vec3){v.x * s, v.y * s, v.z * s};
}

vec3 v3_mul(vec3 a, vec3 b) {
    return (vec3){a.x * b.x, a.y * b.y, a.z * b.z};
}

vec3 v3_normalize(vec3 v) {
    float len = v3_len(v);
    if (len == 0.0f)
        return (vec3){0};
    return (vec3){v.x / len, v.y / len, v.z / len};
}

vec3 v3_cross(vec3 a, vec3 b) {
    return (vec3){.x = a.y * b.z - a.z * b.y, .y = a.z * b.x - a.x * b.z, .z = a.x * b.y - a.y * b.x};
}

float v3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float v3_len(vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

float v3_lensqr(vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

// ivec3
ivec3 iv3_add(ivec3 a, ivec3 b) {
    return (ivec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

ivec3 iv3_sub(ivec3 a, ivec3 b) {
    return (ivec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

ivec3 iv3_scale(ivec3 v, int s) {
    return (ivec3){v.x * s, v.y * s, v.z * s};
}

ivec3 iv3_div(ivec3 v, int d) {
    assert(d != 0);
#define idiv_(x, d) ((x > 0) ? (x) / (d) : ((x) - (d) + 1) / (d))
    return (ivec3){.x = idiv_(v.x, d), .y = idiv_(v.y, d), .z = idiv_(v.z, d)};
}

ivec3 iv3_floor(vec3 v) {
    return (ivec3){(int)floorf(v.x), (int)floorf(v.y), (int)floorf(v.z)};
}

// mat4
mat4 m4_identity() {
    mat4 out = {0};
    out.m[0] = 1.0f;
    out.m[5] = 1.0f;
    out.m[10] = 1.0f;
    out.m[15] = 1.0f;
    return out;
}

mat4 m4_ortho(float left, float right, float bottom, float top, float near, float far) {
    const float width = right - left;
    const float height = top - bottom;
    const float fn = far - near;
    assert(fn != 0.0f && width != 0.0f && height != 0.0f);

    mat4 out = {0};
    out.m[m4_index(0, 0)] = (2.0f / width);
    out.m[m4_index(1, 1)] = (2.0f / height);
    out.m[m4_index(2, 2)] = (-2.0f / fn);
    out.m[m4_index(0, 3)] = -(width / width);
    out.m[m4_index(1, 3)] = -(height / height);
    out.m[m4_index(2, 3)] = -((far + near) / fn);
    out.m[m4_index(3, 3)] = 1.0f;
    return out;
}

mat4 m4_perspective(float fov, float aspect, float near, float far) {
    float range = tanf(fov / 2.0f) * near;
    float sx = (2.0f * near) / (range * aspect + range * aspect);
    float sy = near / range;
    float sz = -(far + near) / (far - near);
    float pz = -(2.0f * far * near) / (far - near);

    mat4 out = {0};
    out.m[m4_index(0, 0)] = sx;
    out.m[m4_index(1, 1)] = sy;
    out.m[m4_index(2, 2)] = sz;
    // out.m[m4_index(3, 3)] = 1.0f;
    out.m[m4_index(2, 3)] = pz;
    out.m[m4_index(3, 2)] = -1.0f;
    return out;
}

mat4 m4_look_at(vec3 eye, vec3 center, vec3 up) {
    vec3 f = v3_normalize(v3_sub(center, eye));
    vec3 r = v3_normalize(v3_cross(f, up));
    vec3 u = v3_cross(r, f);
    mat4 m = {0};
    m.m[m4_index(0, 0)] = r.x;
    m.m[m4_index(0, 1)] = r.y;
    m.m[m4_index(0, 2)] = r.z;
    m.m[m4_index(0, 3)] = -v3_dot(r, eye);
    m.m[m4_index(1, 0)] = u.x;
    m.m[m4_index(1, 1)] = u.y;
    m.m[m4_index(1, 2)] = u.z;
    m.m[m4_index(1, 3)] = -v3_dot(u, eye);
    m.m[m4_index(2, 0)] = -f.x;
    m.m[m4_index(2, 1)] = -f.y;
    m.m[m4_index(2, 2)] = -f.z;
    m.m[m4_index(2, 3)] = v3_dot(f, eye);
    m.m[m4_index(3, 3)] = 1;
    return m;
}

mat4 m4_look_dir(vec3 eye, vec3 dir, vec3 up) {
    mat4 p = m4_translate((vec3){-eye.x, -eye.y, -eye.z});
    vec3 u, d, r;
    d = v3_normalize(dir);
    r = v3_normalize(v3_cross(d, up));
    u = v3_normalize(v3_cross(r, d));

    mat4 out = m4_identity();
    out.m[m4_index(0, 0)] = r.x;
    out.m[m4_index(0, 1)] = r.y;
    out.m[m4_index(0, 2)] = r.z;
    out.m[m4_index(1, 0)] = u.x;
    out.m[m4_index(1, 1)] = u.y;
    out.m[m4_index(1, 2)] = u.z;
    out.m[m4_index(2, 0)] = -d.x;
    out.m[m4_index(2, 1)] = -d.y;
    out.m[m4_index(2, 2)] = -d.z;
    return m4_mul(out, p);
}

mat4 m4_translate(vec3 v) {
    mat4 out = m4_identity();
    out.m[m4_index(0, 3)] = v.x;
    out.m[m4_index(1, 3)] = v.y;
    out.m[m4_index(2, 3)] = v.z;
    return out;
}

mat4 m4_scale(vec3 v) {
    mat4 out = m4_identity();
    out.m[m4_index(0, 0)] = v.x;
    out.m[m4_index(1, 1)] = v.y;
    out.m[m4_index(2, 2)] = v.z;
    return out;
}

mat4 m4_mul(mat4 a, mat4 b) {
    mat4 res = {0};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float s = 0;
            for (int k = 0; k < 4; k++)
                s += b.m[m4_index(k, i)] * a.m[m4_index(j, k)];
            res.m[m4_index(j, i)] = s;
        }
    }
    return res;
}

vec4 m4_mulv4(mat4 m, vec4 v) {
    vec4 out = {0};
    out.x = m.m[m4_index(0, 0)] * v.x + m.m[m4_index(0, 1)] * v.y + m.m[m4_index(0, 2)] * v.z + m.m[m4_index(0, 3)] * v.w;
    out.y = m.m[m4_index(1, 0)] * v.x + m.m[m4_index(1, 1)] * v.y + m.m[m4_index(1, 2)] * v.z + m.m[m4_index(1, 3)] * v.w;
    out.z = m.m[m4_index(2, 0)] * v.x + m.m[m4_index(2, 1)] * v.y + m.m[m4_index(2, 2)] * v.z + m.m[m4_index(2, 3)] * v.w;
    out.w = m.m[m4_index(3, 0)] * v.x + m.m[m4_index(3, 1)] * v.y + m.m[m4_index(3, 2)] * v.z + m.m[m4_index(3, 3)] * v.w;
    return out;
}

vec3 m4_mulv3(mat4 m, vec3 v) {
    vec3 out = {0};
    out.x = m.m[m4_index(0, 0)] * v.x + m.m[m4_index(0, 1)] * v.y + m.m[m4_index(0, 2)] * v.z; // + m.m[m4_index(0, 3)] * v.w;
    out.y = m.m[m4_index(1, 0)] * v.x + m.m[m4_index(1, 1)] * v.y + m.m[m4_index(1, 2)] * v.z; // + m.m[m4_index(1, 3)] * v.w;
    out.z = m.m[m4_index(2, 0)] * v.x + m.m[m4_index(2, 1)] * v.y + m.m[m4_index(2, 2)] * v.z; // + m.m[m4_index(2, 3)] * v.w;
    // out.w = m.m[m4_index(3, 0)] * v.x + m.m[m4_index(3, 1)] * v.y + m.m[m4_index(3, 2)] * v.z;// + m.m[m4_index(3, 3)] * v.w;
    return out;
}

mat4 m4_inverse(mat4 m) {

    float d = m4_determinant(m);
    mat4 dst = {0};
    if (d != 0.0f) {
        const float inv_det = 1.0f / d;
        dst.m[0] = inv_det * (m.m[9] * m.m[14] * m.m[7] - m.m[13] * m.m[10] * m.m[7] + m.m[13] * m.m[6] * m.m[11] - m.m[5] * m.m[14] * m.m[11] - m.m[9] * m.m[6] * m.m[15] + m.m[5] * m.m[10] * m.m[15]);
        dst.m[1] = inv_det * (m.m[13] * m.m[10] * m.m[3] - m.m[9] * m.m[14] * m.m[3] - m.m[13] * m.m[2] * m.m[11] + m.m[1] * m.m[14] * m.m[11] + m.m[9] * m.m[2] * m.m[15] - m.m[1] * m.m[10] * m.m[15]);
        dst.m[2] = inv_det * (m.m[5] * m.m[14] * m.m[3] - m.m[13] * m.m[6] * m.m[3] + m.m[13] * m.m[2] * m.m[7] - m.m[1] * m.m[14] * m.m[7] - m.m[5] * m.m[2] * m.m[15] + m.m[1] * m.m[6] * m.m[15]);
        dst.m[3] = inv_det * (m.m[9] * m.m[6] * m.m[3] - m.m[5] * m.m[10] * m.m[3] - m.m[9] * m.m[2] * m.m[7] + m.m[1] * m.m[10] * m.m[7] + m.m[5] * m.m[2] * m.m[11] - m.m[1] * m.m[6] * m.m[11]);
        dst.m[4] = inv_det * (m.m[12] * m.m[10] * m.m[7] - m.m[8] * m.m[14] * m.m[7] - m.m[12] * m.m[6] * m.m[11] + m.m[4] * m.m[14] * m.m[11] + m.m[8] * m.m[6] * m.m[15] - m.m[4] * m.m[10] * m.m[15]);
        dst.m[5] = inv_det * (m.m[8] * m.m[14] * m.m[3] - m.m[12] * m.m[10] * m.m[3] + m.m[12] * m.m[2] * m.m[11] - m.m[0] * m.m[14] * m.m[11] - m.m[8] * m.m[2] * m.m[15] + m.m[0] * m.m[10] * m.m[15]);
        dst.m[6] = inv_det * (m.m[12] * m.m[6] * m.m[3] - m.m[4] * m.m[14] * m.m[3] - m.m[12] * m.m[2] * m.m[7] + m.m[0] * m.m[14] * m.m[7] + m.m[4] * m.m[2] * m.m[15] - m.m[0] * m.m[6] * m.m[15]);
        dst.m[7] = inv_det * (m.m[4] * m.m[10] * m.m[3] - m.m[8] * m.m[6] * m.m[3] + m.m[8] * m.m[2] * m.m[7] - m.m[0] * m.m[10] * m.m[7] - m.m[4] * m.m[2] * m.m[11] + m.m[0] * m.m[6] * m.m[11]);
        dst.m[8] = inv_det * (m.m[8] * m.m[13] * m.m[7] - m.m[12] * m.m[9] * m.m[7] + m.m[12] * m.m[5] * m.m[11] - m.m[4] * m.m[13] * m.m[11] - m.m[8] * m.m[5] * m.m[15] + m.m[4] * m.m[9] * m.m[15]);
        dst.m[9] = inv_det * (m.m[12] * m.m[9] * m.m[3] - m.m[8] * m.m[13] * m.m[3] - m.m[12] * m.m[1] * m.m[11] + m.m[0] * m.m[13] * m.m[11] + m.m[8] * m.m[1] * m.m[15] - m.m[0] * m.m[9] * m.m[15]);
        dst.m[10] = inv_det * (m.m[4] * m.m[13] * m.m[3] - m.m[12] * m.m[5] * m.m[3] + m.m[12] * m.m[1] * m.m[7] - m.m[0] * m.m[13] * m.m[7] - m.m[4] * m.m[1] * m.m[15] + m.m[0] * m.m[5] * m.m[15]);
        dst.m[11] = inv_det * (m.m[8] * m.m[5] * m.m[3] - m.m[4] * m.m[9] * m.m[3] - m.m[8] * m.m[1] * m.m[7] + m.m[0] * m.m[9] * m.m[7] + m.m[4] * m.m[1] * m.m[11] - m.m[0] * m.m[5] * m.m[11]);
        dst.m[12] = inv_det * (m.m[12] * m.m[9] * m.m[6] - m.m[8] * m.m[13] * m.m[6] - m.m[12] * m.m[5] * m.m[10] + m.m[4] * m.m[13] * m.m[10] + m.m[8] * m.m[5] * m.m[14] - m.m[4] * m.m[9] * m.m[14]);
        dst.m[13] = inv_det * (m.m[8] * m.m[13] * m.m[2] - m.m[12] * m.m[9] * m.m[2] + m.m[12] * m.m[1] * m.m[10] - m.m[0] * m.m[13] * m.m[10] - m.m[8] * m.m[1] * m.m[14] + m.m[0] * m.m[9] * m.m[14]);
        dst.m[14] = inv_det * (m.m[12] * m.m[5] * m.m[2] - m.m[4] * m.m[13] * m.m[2] - m.m[12] * m.m[1] * m.m[6] + m.m[0] * m.m[13] * m.m[6] + m.m[4] * m.m[1] * m.m[14] - m.m[0] * m.m[5] * m.m[14]);
        dst.m[15] = inv_det * (m.m[4] * m.m[9] * m.m[2] - m.m[8] * m.m[5] * m.m[2] + m.m[8] * m.m[1] * m.m[6] - m.m[0] * m.m[9] * m.m[6] - m.m[4] * m.m[1] * m.m[10] + m.m[0] * m.m[5] * m.m[10]);
    }
    return dst;
}

float m4_determinant(mat4 m) {
    const float d = m.m[m4_index(0, 0)] * m.m[m4_index(1, 1)] * m.m[m4_index(2, 2)] * m.m[m4_index(3, 3)] + m.m[m4_index(0, 0)] * m.m[m4_index(1, 2)] * m.m[m4_index(2, 3)] * m.m[m4_index(3, 1)] +
                    m.m[m4_index(0, 0)] * m.m[m4_index(1, 3)] * m.m[m4_index(2, 1)] * m.m[m4_index(3, 2)] + m.m[m4_index(0, 1)] * m.m[m4_index(1, 0)] * m.m[m4_index(2, 3)] * m.m[m4_index(3, 2)] +
                    m.m[m4_index(0, 1)] * m.m[m4_index(1, 2)] * m.m[m4_index(2, 0)] * m.m[m4_index(3, 3)] + m.m[m4_index(0, 1)] * m.m[m4_index(1, 3)] * m.m[m4_index(2, 2)] * m.m[m4_index(3, 0)] +
                    m.m[m4_index(0, 2)] * m.m[m4_index(1, 0)] * m.m[m4_index(2, 1)] * m.m[m4_index(3, 3)] + m.m[m4_index(0, 2)] * m.m[m4_index(1, 1)] * m.m[m4_index(2, 3)] * m.m[m4_index(3, 0)] +
                    m.m[m4_index(0, 2)] * m.m[m4_index(1, 3)] * m.m[m4_index(2, 0)] * m.m[m4_index(3, 1)] + m.m[m4_index(0, 3)] * m.m[m4_index(1, 0)] * m.m[m4_index(2, 2)] * m.m[m4_index(3, 1)] +
                    m.m[m4_index(0, 3)] * m.m[m4_index(1, 1)] * m.m[m4_index(2, 0)] * m.m[m4_index(3, 2)] + m.m[m4_index(0, 3)] * m.m[m4_index(1, 2)] * m.m[m4_index(2, 1)] * m.m[m4_index(3, 0)] -
                    m.m[m4_index(0, 0)] * m.m[m4_index(1, 1)] * m.m[m4_index(2, 3)] * m.m[m4_index(3, 2)] - m.m[m4_index(0, 0)] * m.m[m4_index(1, 2)] * m.m[m4_index(2, 1)] * m.m[m4_index(3, 3)] -
                    m.m[m4_index(0, 0)] * m.m[m4_index(1, 3)] * m.m[m4_index(2, 2)] * m.m[m4_index(3, 1)] - m.m[m4_index(0, 1)] * m.m[m4_index(1, 0)] * m.m[m4_index(2, 2)] * m.m[m4_index(3, 3)] -
                    m.m[m4_index(0, 1)] * m.m[m4_index(1, 2)] * m.m[m4_index(2, 3)] * m.m[m4_index(3, 0)] - m.m[m4_index(0, 1)] * m.m[m4_index(1, 3)] * m.m[m4_index(2, 0)] * m.m[m4_index(3, 2)] -
                    m.m[m4_index(0, 2)] * m.m[m4_index(1, 0)] * m.m[m4_index(2, 3)] * m.m[m4_index(3, 1)] - m.m[m4_index(0, 2)] * m.m[m4_index(1, 1)] * m.m[m4_index(2, 0)] * m.m[m4_index(3, 3)] -
                    m.m[m4_index(0, 2)] * m.m[m4_index(1, 3)] * m.m[m4_index(2, 1)] * m.m[m4_index(3, 0)] - m.m[m4_index(0, 3)] * m.m[m4_index(1, 0)] * m.m[m4_index(2, 1)] * m.m[m4_index(3, 2)] -
                    m.m[m4_index(0, 3)] * m.m[m4_index(1, 1)] * m.m[m4_index(2, 2)] * m.m[m4_index(3, 0)] - m.m[m4_index(0, 3)] * m.m[m4_index(1, 2)] * m.m[m4_index(2, 0)] * m.m[m4_index(3, 1)];
    return d;
}

// quat
vec4 quat_identity() {
    return (vec4){.x = 0, .y = 0, .z = 0, .w = 1};
}

vec4 quat_euler(float pitch, float yaw, float roll) {
    float cy = cosf(yaw * 0.5f);
    float sy = sinf(yaw * 0.5f);
    float cr = cosf(roll * 0.5f);
    float sr = sinf(roll * 0.5f);
    float cp = cosf(pitch * 0.5f);
    float sp = sinf(pitch * 0.5f);

    vec4 q;
    q.w = cy * cr * cp + sy * sr * sp;
    q.x = cy * sr * cp - sy * cr * sp;
    q.y = cy * cr * sp + sy * sr * cp;
    q.z = sy * cr * cp - cy * sr * sp;
    return q;
}

vec3 quat_to_euler(vec4 q) {
    vec3 euler;
    float sqw = q.w * q.w;
    float sqx = q.x * q.x;
    float sqy = q.y * q.y;
    float sqz = q.z * q.z;
    euler.z = to_deg(atan2f(2.0f * (q.x * q.y + q.z * q.w), sqx - sqy - sqz + sqw));
    euler.x = to_deg(atan2f(2.0f * (q.y * q.z + q.x * q.w), -sqx - sqy + sqz + sqw));
    euler.y = to_deg(asinf(-2.0f * (q.x * q.z - q.y * q.w)));
    return euler;
}

vec4 quat_axis_angle(vec3 axis, float angle) {
    const float s = sinf(angle / 2.0f);
    return (vec4){.x = axis.x * s, .y = axis.y * s, .z = axis.z * s, .w = cosf(angle / 2.0f)};
}

vec4 quat_look_rotation(vec3 forward, vec3 up) {
    forward = v3_normalize(forward);
    up = v3_normalize(up);
    vec3 right = v3_cross(up, forward);
    right = v3_normalize(right);
    up = v3_cross(forward, right);

    mat4 rot = {0};
    rot.m[m4_index(0, 0)] = right.x;
    rot.m[m4_index(1, 0)] = right.y;
    rot.m[m4_index(2, 0)] = right.z;
    rot.m[m4_index(0, 1)] = up.x;
    rot.m[m4_index(1, 1)] = up.y;
    rot.m[m4_index(2, 1)] = up.z;
    rot.m[m4_index(0, 2)] = forward.x;
    rot.m[m4_index(1, 2)] = forward.y;
    rot.m[m4_index(2, 2)] = forward.z;

    vec4 q;
    float trace = rot.m[m4_index(0, 0)] + rot.m[m4_index(1, 1)] + rot.m[m4_index(2, 2)];

    // what the fuck
    // basically tries to reconstruct the quaterinion by finding the largest component
    // this is called maximum trace and hurts my brain
    // https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation
    // https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
    if (trace > 0.0f) {
        float S = sqrtf(trace + 1.0f) * 2.0f;
        q.w = 0.25f * S;
        q.x = (rot.m[m4_index(2, 1)] - rot.m[m4_index(1, 2)]) / S;
        q.y = (rot.m[m4_index(0, 2)] - rot.m[m4_index(2, 0)]) / S;
        q.z = (rot.m[m4_index(1, 0)] - rot.m[m4_index(0, 1)]) / S;
    } else {
        if (rot.m[m4_index(0, 0)] > rot.m[m4_index(1, 1)] && rot.m[m4_index(0, 0)] > rot.m[m4_index(2, 2)]) {
            float S = sqrtf(1.0f + rot.m[m4_index(0, 0)] - rot.m[m4_index(1, 1)] - rot.m[m4_index(2, 2)]) * 2.0f;
            q.w = (rot.m[m4_index(2, 1)] - rot.m[m4_index(1, 2)]) / S;
            q.x = 0.25f * S;
            q.y = (rot.m[m4_index(0, 1)] + rot.m[m4_index(1, 0)]) / S;
            q.z = (rot.m[m4_index(0, 2)] + rot.m[m4_index(2, 0)]) / S;
        } else if (rot.m[m4_index(1, 1)] > rot.m[m4_index(2, 2)]) {
            float S = sqrtf(1.0f + rot.m[m4_index(1, 1)] - rot.m[m4_index(0, 0)] - rot.m[m4_index(2, 2)]) * 2.0f;
            q.w = (rot.m[m4_index(0, 2)] - rot.m[m4_index(2, 0)]) / S;
            q.x = (rot.m[m4_index(0, 1)] + rot.m[m4_index(1, 0)]) / S;
            q.y = 0.25f * S;
            q.z = (rot.m[m4_index(1, 2)] + rot.m[m4_index(2, 1)]) / S;
        } else {
            float S = sqrtf(1.0f + rot.m[m4_index(2, 2)] - rot.m[m4_index(0, 0)] - rot.m[m4_index(1, 1)]) * 2.0f;
            q.w = (rot.m[m4_index(1, 0)] - rot.m[m4_index(0, 1)]) / S;
            q.x = (rot.m[m4_index(0, 2)] + rot.m[m4_index(2, 0)]) / S;
            q.y = (rot.m[m4_index(1, 2)] + rot.m[m4_index(2, 1)]) / S;
            q.z = 0.25f * S;
        }
    }

    return quat_normalize(q);
}

vec4 quat_mul(vec4 a, vec4 b) {
    vec4 out;
    out.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    out.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    out.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    out.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    return out;
}

vec3 quat_mulv(vec4 q, vec3 v) {
    vec4 p = {.x = v.x, .y = v.y, .z = v.z, .w = 0};

    // Quaternion multiplication: q * p * q^-1
    vec4 q_inv = quat_conjugate(q);
    vec4 rotated = quat_mul(quat_mul(q, p), q_inv);
    return (vec3){rotated.x, rotated.y, rotated.z};
}

mat4 quat_mat4(vec4 q) {
    mat4 m = {0};
    q = quat_normalize(q);

    float x = q.x, y = q.y, z = q.z, w = q.w;
    float x2 = x * x, y2 = y * y, z2 = z * z;
    m.m[m4_index(0, 0)] = 1.0f - 2.0f * (y2 + z2);
    m.m[m4_index(1, 1)] = 1.0f - 2.0f * (x2 + z2);
    m.m[m4_index(2, 2)] = 1.0f - 2.0f * (x2 + y2);

    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;
    m.m[m4_index(0, 1)] = 2.0f * (xy - wz);
    m.m[m4_index(0, 2)] = 2.0f * (xz + wy);
    m.m[m4_index(1, 0)] = 2.0f * (xy + wz);
    m.m[m4_index(1, 2)] = 2.0f * (yz - wx);
    m.m[m4_index(2, 0)] = 2.0f * (xz - wy);
    m.m[m4_index(2, 1)] = 2.0f * (yz + wx);
    m.m[m4_index(3, 3)] = 1.0f;
    return m;
}

// https://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/slerp/
vec4 quat_conjugate(vec4 q) {
    return (vec4){-q.x, -q.y, -q.z, q.w};
}

vec4 quat_normalize(vec4 q) {
    float len = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len == 0.0f)
        return (vec4){0};
    return (vec4){q.x / len, q.y / len, q.z / len, q.w / len};
}

vec4 quat_inverse(vec4 q) {
    return quat_normalize(quat_conjugate(q));
}

void frustum_make(mat4 m, vec4 frustum[6]) {
    // -x plane
    frustum[0].x = m.m[3] + m.m[0];
    frustum[0].y = m.m[7] + m.m[4];
    frustum[0].z = m.m[11] + m.m[8];
    frustum[0].w = m.m[15] + m.m[12];

    // +x plane
    frustum[1].x = m.m[3] - m.m[0];
    frustum[1].y = m.m[7] - m.m[4];
    frustum[1].z = m.m[11] - m.m[8];
    frustum[1].w = m.m[15] - m.m[12];

    // -y plane
    frustum[2].x = m.m[3] + m.m[1];
    frustum[2].y = m.m[7] + m.m[5];
    frustum[2].z = m.m[11] + m.m[9];
    frustum[2].w = m.m[15] + m.m[13];

    // +y plane
    frustum[3].x = m.m[3] - m.m[1];
    frustum[3].y = m.m[7] - m.m[5];
    frustum[3].z = m.m[11] - m.m[9];
    frustum[3].w = m.m[15] - m.m[13];

    // inverted Z dir for opengl
    // -z plane
    frustum[4].x = m.m[3] + m.m[2];
    frustum[4].y = m.m[7] + m.m[6];
    frustum[4].z = m.m[11] + m.m[10];
    frustum[4].w = m.m[15] + m.m[14];

    // +z plane
    frustum[5].x = m.m[3] - m.m[2];
    frustum[5].y = m.m[7] - m.m[6];
    frustum[5].z = m.m[11] - m.m[10];
    frustum[5].w = m.m[15] - m.m[14];

    // normalize
    for (int i = 0; i < 6; i++) {
        float length = sqrtf(frustum[i].x * frustum[i].x + frustum[i].y * frustum[i].y + frustum[i].z * frustum[i].z);
        frustum[i].x /= length;
        frustum[i].y /= length;
        frustum[i].z /= length;
        frustum[i].w /= length;
    }
}

bool frustum_aabb_test(vec4 frustum[6], vec3 min, vec3 max) {
    vec3 corners[8];
    corners[0] = (vec3){min.x, min.y, min.z};
    corners[1] = (vec3){min.x, min.y, max.z};
    corners[2] = (vec3){min.x, max.y, min.z};
    corners[3] = (vec3){min.x, max.y, max.z};
    corners[4] = (vec3){max.x, min.y, min.z};
    corners[5] = (vec3){max.x, min.y, max.z};
    corners[6] = (vec3){max.x, max.y, min.z};
    corners[7] = (vec3){max.x, max.y, max.z};

    for (int i = 0; i < 6; i++) {
        vec4 plane = frustum[i];
        int out = 0;
        for (int j = 0; j < 8; j++) {
            vec3 point = corners[j];
            float dst = plane.x * point.x + plane.y * point.y + plane.z * point.z + plane.w;
            if (dst < 0.0f)
                out++;
        }

        if (out == 8)
            return false;
    }
    return true;
}
