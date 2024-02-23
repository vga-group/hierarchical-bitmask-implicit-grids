#ifndef RAYBASE_GFX_DECAL_GLSL
#define RAYBASE_GFX_DECAL_GLSL
#include "material_data.glsl"

struct decal_metadata
{
    float pos_x, pos_y, pos_z;
    uint order;
};

struct decal
{
    mat4 world_to_obb;
    material_spec material;
};

#endif
