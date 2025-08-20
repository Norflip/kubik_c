#version 460

uniform sampler2DArray atlas_texarr;
layout(location = 3) uniform uint atlas_depth;

const vec3 normal_lookup[6] = {
vec3(0, 1, 0), vec3(0, - 1, 0), vec3(1, 0, 0), vec3(- 1, 0, 0), vec3(0, 0, 1), vec3(0, 0, - 1) };

in VSOut {
    flat uint texIndex;
    flat uint face;
    vec2 uv;
    float ao;
} inData;

out vec4 frag_color;

vec3 get_array_uv(vec2 uv, uint layer, uint cap) {
    return vec3(uv, max(0, min(float(cap - 1), floor(float(layer) + 0.5))));
}

void main() {
    vec3 normal = normal_lookup[inData.face];
    vec3 array_uv = get_array_uv(inData.uv, inData.texIndex, atlas_depth);
    vec4 color = texture(atlas_texarr, array_uv);

    const float dir_shadow_strength = 0.4;
    vec3 sun_dir = normalize(vec3(1.0, 1.0, 1.0));
    float ndots = (dot(normal, sun_dir) + 1.0) * 0.5;

    #if 0
    frag_color = vec4(inData.ao, inData.ao, inData.ao, 1.0);
    return;
    #endif

    color.rgb *= (1.0 - dir_shadow_strength)  + dir_shadow_strength * ndots;
    color.rgb *= 1.2;
    color.rgb *= inData.ao;

    frag_color = vec4(clamp(color.rgb, 0, 1), 1.0);
}