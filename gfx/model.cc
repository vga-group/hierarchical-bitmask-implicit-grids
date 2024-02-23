#include "model.hh"

namespace rb::gfx
{

bool model::potentially_transparent() const
{
    for(const auto& mat: materials)
        if(mat.potentially_transparent()) return true;
    return false;
}

}
