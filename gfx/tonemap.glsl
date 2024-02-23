#ifndef RAYBASE_GFX_TONEMAP_GLSL
#define RAYBASE_GFX_TONEMAP_GLSL

#include "color.glsl"

layout(binding = 0) uniform sampler3D lut;
layout(binding = 1) uniform parameter_buffer
{
    mat4 color_transform;
    uint tonemap_operator;
    uint correction;
    float gain;
    float saturation;
    float max_white;
    float gamma;
} parameters;

layout(push_constant) uniform push_constant_buffer
{
    ivec2 size;
    uint use_lookup_texture;
} pc;

vec3 reinhard(vec3 c)
{
    return c / (1.0f + c);
}

vec3 reinhard_extended(vec3 c, float max_white)
{
    return (c + c * c / vec3(max_white * max_white)) / (1.0f + c);
}

vec3 reinhard_jodie(vec3 c)
{
    float l = luminance(c);
    vec3 rc = reinhard(c);
    return mix(c / (1.0f + l), rc, rc);
}

vec3 filmic_hable(vec3 c)
{
    return (c * (0.8274390f * c + 0.1379065f) + 0.00551626f)/(c * (0.60f * c + 1.0f) + 0.06f) - 0.091937f;
}

// Fitting math from: https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
vec3 filmic_aces(vec3 v)
{
    v = v * mat3(
        0.59719f, 0.35458f, 0.04823f,
        0.07600f, 0.90834f, 0.01566f,
        0.02840f, 0.13383f, 0.83777f
    );
    return (v * (v + 0.0245786f) - 9.0537e-5f) /
        (v * (0.983729f * v + 0.4329510f) + 0.238081f) * mat3(
        1.60475f, -0.53108f, -0.07367f,
        -0.10208f,  1.10813f, -0.00605f,
        -0.00327f, -0.07276f,  1.07602f
    );
}

void apply_lookup_texture(inout vec4 color)
{
    if(pc.use_lookup_texture != 0)
    {
        ivec3 size = textureSize(lut, 0);
        vec3 off = 0.5f / vec3(size);
        vec3 scale = vec3(size-1)/vec3(size);
        color.rgb = texture(lut, color.rgb * scale + off).rgb;
    }
}

void apply_tonemapping(inout vec4 color)
{
    switch(parameters.tonemap_operator)
    {
    case 1:
        color.rgb = reinhard(color.rgb);
        break;
    case 2:
        color.rgb = reinhard_extended(color.rgb, parameters.max_white);
        break;
    case 3:
        color.rgb = reinhard_jodie(color.rgb);
        break;
    case 4:
        color.rgb = filmic_hable(color.rgb);
        break;
    case 5:
        color.rgb = filmic_aces(color.rgb);
        break;
    default:
        break;
    }
}

void apply_parametric_transforms(inout vec4 color)
{
    color.rgb *= parameters.gain;
    color = parameters.color_transform * color;

    float lum = luminance(color.rgb);
    color.rgb = (color.rgb - lum) * parameters.saturation + lum;
    color = max(color, vec4(0));
}

void apply_correction(inout vec4 color)
{
    switch(parameters.correction)
    {
    case 1: // Gamma correction
        color.rgb = pow(color.rgb, vec3(parameters.gamma));
        break;
    case 2: // SRGB correction
        color.rgb = srgb_correction(color.rgb);
        break;
    default:
        break;
    }
}

#endif
