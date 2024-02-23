#ifndef RAYBASE_GFX_RT_GENERIC_GLSL
#define RAYBASE_GFX_RT_GENERIC_GLSL

// This header pairs with the other rt_generic.* things; they're used for
// generic intersection testing in various RT kernels. Many .rchit, .rmiss etc.
// are the exact same, so this avoids some copy/pasta.

#define RAYBASE_RAY_TRACING
#include "scene.glsl"

struct bounce_payload
{
    uint seed;
    int instance_index;
    int primitive_index;
    vec2 hit_attribs;
};

#endif
