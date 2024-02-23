#ifndef RAYBASE_GFX_DECAL_HH
#define RAYBASE_GFX_DECAL_HH
#include "material.hh"

namespace rb::gfx
{

// The decal component is simply a material. You can orient and scale it using
// a transformable component.
struct decal
{
    material mat;
    // Lower order gets applied first. Equal order is applied in an unspecified order.
    uint32_t order;
};

// This component can be used to disable receiving decals on a per-object basis.
struct disable_decals {};

}

#endif
