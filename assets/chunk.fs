#version 460
#define DEBUG_AO_VALUES 0
#define DEBUG_LIGHT_VALUES 0

uniform sampler2DArray atlas_texarr;
layout(location = 3) uniform uint atlas_depth;
layout(location = 4) uniform vec4 eye_pos_light; // x,y,z = eye pos, w = light level

const vec3 normal_lookup[6] = {
vec3(0, 1, 0), vec3(0, - 1, 0), vec3(1, 0, 0), vec3(- 1, 0, 0), vec3(0, 0, 1), vec3(0, 0, - 1) };

in VSOut {
    flat uint texIndex;
    flat uint face;
    flat uint tint;
    vec2 uv;
    float ao;
    float light_test;
    vec3 world_pos;
} inData;


out vec4 frag_color;

vec4 unpack_color(uint color) {
    float a = float((color >> 24) & 0xFF) / 255.0;
    float b = float((color >> 16) & 0xFF) / 255.0;
    float g = float((color >> 8) & 0xFF) / 255.0;
    float r = float(color & 0xFF) / 255.0;
    return vec4(a, b, g, r);
}

vec3 get_array_uv(vec2 uv, uint layer, uint cap) {
    return vec3(uv, max(0, min(float(cap - 1), floor(float(layer) + 0.5))));
}

vec4 calculate_eye_point_light(vec3 world_pos) {
    float distance = length(eye_pos_light.xyz - world_pos);
    float attenuation = 1.0 / (1.0 + 0.1 * distance + 0.01 * distance * distance);
    return vec4(eye_pos_light.w * attenuation);
}

void main() {
    vec3 normal = normal_lookup[inData.face];
    vec3 array_uv = get_array_uv(inData.uv, inData.texIndex, atlas_depth);
    vec4 color = texture(atlas_texarr, array_uv);
    vec4 final_color = color;

    const float dir_shadow_strength = 0.4;
    vec3 sun_dir = normalize(vec3(1.0, 1.0, 1.0));
    float ndots = (dot(normal, sun_dir) + 1.0) * 0.5;

    #if DEBUG_AO_VALUES
    frag_color = vec4(inData.ao, inData.ao, inData.ao, 1.0);
    return;
    #endif 

    #if DEBUG_LIGHT_VALUES
    frag_color = vec4(inData.light_test, inData.light_test, inData.light_test, 1.0);
    return;
    #endif

    float eye_light = calculate_eye_point_light(inData.world_pos).r;    
    const float AMBIENT_LIGHT = 0.05;

    final_color.rgb *= (1.0 - dir_shadow_strength)  + dir_shadow_strength * ndots;
    final_color.rgb *= clamp(inData.light_test + AMBIENT_LIGHT, 0.0, 1.0);
    final_color.rgb *= inData.ao * 1.2;

    vec4 tint = unpack_color(inData.tint);
    final_color = mix(final_color, tint, tint.a);
    final_color = mix(final_color, color, eye_light);

    frag_color = vec4(clamp(final_color.rgb, 0, 1), 1.0);
}