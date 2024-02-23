#ifndef RAYBASE_GFX_FRAMEBUFFER_HH
#define RAYBASE_GFX_FRAMEBUFFER_HH

#include "render_target.hh"
#include "vkres.hh"

namespace rb::gfx
{

class render_pass;
class framebuffer
{
public:
    framebuffer();

    void init(render_pass& pass, argvec<render_target*> targets);

    operator VkFramebuffer() const;
    uvec2 get_size() const;

    bool compatible_with(render_pass& pass) const;

private:
    uvec2 size;
    vkres<VkFramebuffer> fb;
    VkRenderPass pass;
};

}

#endif
