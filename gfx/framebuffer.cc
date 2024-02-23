#include "framebuffer.hh"
#include "render_pass.hh"
#include "vulkan_helpers.hh"

namespace rb::gfx
{

framebuffer::framebuffer()
: size(0, 0)
{
}

void framebuffer::init(render_pass& pass, argvec<render_target*> targets)
{
    device& dev = pass.get_device();
    std::vector<VkImageView> image_views;
    for(size_t i = 0, j = 0; i < targets.size(); ++i)
    {
        if(!targets[i])
            continue;
        targets[i]->set_layout(pass.get_attachment_description(j).finalLayout);
        image_views.push_back(targets[i]->get_view());
        size = targets[i]->get_size();
        j++;
    }

    VkFramebufferCreateInfo framebuffer_info = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        nullptr,
        0,
        pass,
        (uint32_t)image_views.size(),
        image_views.data(),
        size.x,
        size.y,
        1
    };
    VkFramebuffer fb;
    vkCreateFramebuffer(dev.logical_device, &framebuffer_info, nullptr, &fb);
    this->fb = vkres(dev, fb);
    for(VkImageView view: image_views)
        dev.gc.depend(view, fb);
    dev.gc.depend(pass, fb);
    this->pass = pass;
}

framebuffer::operator VkFramebuffer() const
{
    return *fb;
}

uvec2 framebuffer::get_size() const
{
    return size;
}

bool framebuffer::compatible_with(render_pass& pass) const
{
    return this->pass == pass;
}

}
