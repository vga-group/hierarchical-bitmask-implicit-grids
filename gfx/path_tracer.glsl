#ifndef RAYBASE_GFX_PATH_TRACER_GLSL
#define RAYBASE_GFX_PATH_TRACER_GLSL

#define RAYBASE_VERTEX_DATA_TRIANGLE_POS
#include "rt_generic.glsl"

layout(constant_id = 10) const uint RB_FILM_SAMPLING_MODE = 0;
layout(constant_id = 11) const uint RB_APERTURE_SHAPE = 0;
layout(constant_id = 12) const uint RB_PATH_TRACING_FLAGS = 0;
layout(constant_id = 13) const uint RB_DECAL_MODE = 0;
layout(constant_id = 14) const uint RB_MAX_BOUNCES = 4;
layout(constant_id = 15) const uint RB_DIRECT_RAY_MASK = 0xFF;
layout(constant_id = 16) const uint RB_INDIRECT_RAY_MASK = 0xFF;

#define TRACE_DECALS_NEVER 0
#define TRACE_DECALS_PRIMARY 1
#define TRACE_DECALS_ALWAYS 2
#define STOCHASTIC_ALPHA_BLENDING (1<<0)
#define NEE_SAMPLE_ALL_LIGHTS (1<<1)
#define USE_BLUE_NOISE_FOR_DIRECT (1<<3)
#define SHOW_LIGHTS_ON_PRIMARY_RAY (1<<4)
#define USE_PATH_SPACE_REGULARIZATION (1<<5)
#define PATH_SPACE_REGULARIZATION_NEE_ONLY (1<<6)
#define USE_CLAMPING (1<<7)
#define CULL_BACK_FACES (1<<8)

struct intersection_info
{
    vertex_data vd;
    material mat;
    // Light sources are separated due to sampling concerns. This is needed for
    // implementing next event estimation.
    vec3 light;

    // These PDFs are used for MIS.
    float point_light_pdf;
    float directional_light_pdf;
    float tri_light_pdf;
    float envmap_pdf;
};

struct path_tracer_config
{
    vec4 light_type_sampling_probabilities;
    float min_ray_dist;

    float max_ray_dist;
    float film_parameter;
    float regularization_gamma;
    float clamping_threshold;
    int pad[3];
};

#endif
