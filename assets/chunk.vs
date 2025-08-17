#version 460

struct quad {
    uint pos_face;
    uint tex;
};

layout(location = 0) uniform mat4 vp;
layout(location = 1) uniform ivec3 chunk_pos;

layout(std430, binding = 0) buffer QuadBuffer {
    quad quads[];
};

const ivec3 VOXEL_CORNERS[8] = ivec3[8](
    ivec3(0, 0, 0), ivec3(1, 0, 0), ivec3(1, 0, 1), ivec3(0, 0, 1), 
    ivec3(0, 1, 0), ivec3(1, 1, 0), ivec3(1, 1, 1), ivec3(0, 1, 1)
);

const uint CUBE_FACE_LOOKUP[24] = uint[24](
    1u, 2u, 6u, 5u, // r (face 0)
    3u, 0u, 4u, 7u, // l (face 1)
    4u, 5u, 6u, 7u, // u (face 2)
    0u, 3u, 2u, 1u, // d (face 3)
    2u, 3u, 7u, 6u, // f (face 4)
    0u, 1u, 5u, 4u  // b (face 5)
);

const int tris_order [6] = {3, 2, 1, 3, 1, 0}; // order of the 4 corners to create the two triangles
const int flipped_tris_order [6] = {2, 1, 0, 2, 0, 3};
const vec2 corner_uv_lookup [4] = vec2[4] (vec2(0,1), vec2(1,1), vec2(1,0), vec2(0,0));

#if 0
const float AO_VALUES[4] = float[](0.60, 0.70, 0.85, 1.0);
#else 
const float AO_VALUES[4] = float[](0.0, 0.4, 0.7, 1.0);
#endif

out VSOut {
    flat uint texIndex;
    flat uint face;
    vec2 uv;
    float ao;
} outData;

ivec3 get_quad_pos (quad q, out uint face) {
    uint x = (q.pos_face & 0x3F);
    uint y = ((q.pos_face >> 6) & 0x3F);
    uint z = ((q.pos_face >> 12) & 0x3F);
    face = (q.pos_face >> 18) & 0x7;
    return ivec3(x, y, z);
}

uint get_quad_ao (quad q, int corner) {
    return (q.pos_face >> (21 + corner * 2)) & 0x3;
}

bool is_quad_flipped (quad q) {
    return bool((q.pos_face >> 29) & 0x1);
}

void main () {
    const uint idx = gl_VertexID;
    quad q = quads[idx / 6];

    uint face;
    ivec3 pos = get_quad_pos(q, face);

    int corner_idx;
    if(is_quad_flipped(q)) {
        corner_idx = flipped_tris_order[idx % 6];
    } else {
        corner_idx = tris_order[idx % 6];
    }

    uint c_index = CUBE_FACE_LOOKUP[(face * 4) + corner_idx];
    ivec3 voxel_pos = VOXEL_CORNERS[c_index];

    ivec3 wpos = voxel_pos + pos;
    wpos += chunk_pos;
    gl_Position = vp * vec4(vec3(wpos), 1.0);

    outData.texIndex = q.tex;
    outData.face = face;
    outData.uv = corner_uv_lookup[corner_idx];
    outData.ao = clamp(AO_VALUES[get_quad_ao(q, corner_idx)] * 1.0f, 0.0f, 1.0f);
}