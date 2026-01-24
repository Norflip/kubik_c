#version 460

struct quad {
    uint pos_face;
    uint tex;
    uint light;
    uint _padding;
};

struct chunk_data {
    int x, y, z;
    uint tint;
};

layout(location = 0) uniform mat4 vp;

layout(std430, binding = 0) buffer QuadBuffer {
    quad quads[];
};

layout(std430, binding = 1) buffer ChunkDataBuffer {
    chunk_data per_chunk_data[];
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

uint rng_value (uint seed) {
    uint x = seed;
    x += (x << 10);
    x ^= (x >> 6);
    x += (x << 3);
    x ^= (x >> 11);
    x += (x << 15);
    return x;
}

#if 1
const float AO_VALUES[4] = float[](0.60, 0.70, 0.85, 1.0);
#else 
const float AO_VALUES[4] = float[](0.0, 0.4, 0.7, 1.0);
#endif

out VSOut {
    flat uint texIndex;
    flat uint face;
    flat uint tint;
    vec2 uv;
    float ao;
    float light_test;
    vec3 world_pos;
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

void unpack_light (quad q, out float light_value){
    light_value = float(q.light & 0xFF) / 0xFF;
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
    wpos.x += per_chunk_data[gl_DrawID].x;
    wpos.y += per_chunk_data[gl_DrawID].y;
    wpos.z += per_chunk_data[gl_DrawID].z;
    gl_Position = vp * vec4(vec3(wpos), 1.0);

    outData.texIndex = q.tex;
    outData.face = face;
    outData.tint = per_chunk_data[gl_DrawID].tint;
    outData.light_test = float(q.light & 31) / 31.0;
    outData.world_pos = vec3(wpos);

    //float((q.light >> (corner_idx * 8)) & 0xFF) / 255.0;
    //uint uv_corner_offset = rng_value(idx / 6) % 4;
    //outData.uv = corner_uv_lookup[(uv_corner_offset + corner_idx) % 4];
    outData.uv = corner_uv_lookup[corner_idx];
    outData.ao = clamp(AO_VALUES[get_quad_ao(q, corner_idx)] + 0.1f, 0.0f, 1.0f);
}