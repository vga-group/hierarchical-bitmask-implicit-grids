#ifndef RAYBASE_GFX_MODEL_HH
#define RAYBASE_GFX_MODEL_HH
#include "mesh.hh"
#include "material.hh"
#include <vector>

namespace rb::gfx
{

struct model
{
    mesh* m;
    std::vector<material> materials;

    bool potentially_transparent() const;
};

}

#endif
