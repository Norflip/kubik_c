#version 450
//layout(location = 0) in vec3 aPos;
//layout(location = 1) in uint aColor;
layout(location = 0) uniform mat4 vp;

struct vertex {
    vec3 pos;
    uint col;
};

layout(std430, binding = 0) buffer VertexBuffer {
    vertex vertices[];
};

out vec4 f_color;

vec4 unpack_u32(uint color) {
    float r = float((color >> 24) & 0xFF) / 255.0;
    float g = float((color >> 16) & 0xFF) / 255.0;
    float b = float((color >> 8) & 0xFF) / 255.0;
    float a = float(color & 0xFF) / 255.0;
    return vec4(r, g, b, a);
}

void main() {
    uint index = gl_VertexID;
    vec3 position = vertices[index].pos;
    uint color_packed = vertices[index].col;
    f_color = unpack_u32(color_packed);
    gl_Position = vp * vec4(position.xyz, 1.0);
}