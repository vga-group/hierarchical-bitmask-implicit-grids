#include "gbuffer.hh"

namespace rb::gfx
{

gbuffer::gbuffer(device& dev, uvec2 size, uint32_t buffer_mask, bool temporal)
: gbuffer(dev, size, buffer_mask, temporal ? buffer_mask & (~MOTION) : 0)
{
}

gbuffer::gbuffer(device& dev, uvec2 size, uint32_t buffer_mask, uint32_t temporal_mask)
{
    init(dev, size, buffer_mask, temporal_mask);
}

gbuffer_target gbuffer::get_target()
{
    gbuffer_target target;

    if(textures.color) target.color = textures.color->get_render_target(0);
    if(textures.depth) target.depth = textures.depth->get_render_target(0);
    if(textures.normal) target.normal = textures.normal->get_render_target(0);
    if(textures.flat_normal) target.flat_normal = textures.flat_normal->get_render_target(0);
    if(textures.position) target.position = textures.position->get_render_target(0);
    if(textures.albedo) target.albedo = textures.albedo->get_render_target(0);
    if(textures.emission) target.emission = textures.emission->get_render_target(0);
    if(textures.material) target.material = textures.material->get_render_target(0);
    if(textures.transmission) target.transmission = textures.transmission->get_render_target(0);
    if(textures.sheen) target.sheen = textures.sheen->get_render_target(0);
    if(textures.material_extra) target.material_extra = textures.material_extra->get_render_target(0);
    if(textures.motion) target.motion = textures.motion->get_render_target(0);

    return target;
}

gbuffer_target gbuffer::get_temporal_target()
{
    gbuffer_target target;

#define TEST(name) if(name && name->get_layer_count() == 2)
    TEST(textures.color) target.color = textures.color->get_render_target(1);
    TEST(textures.depth) target.depth = textures.depth->get_render_target(1);
    TEST(textures.normal) target.normal = textures.normal->get_render_target(1);
    TEST(textures.flat_normal) target.flat_normal = textures.flat_normal->get_render_target(1);
    TEST(textures.position) target.position = textures.position->get_render_target(1);
    TEST(textures.albedo) target.albedo = textures.albedo->get_render_target(1);
    TEST(textures.emission) target.emission = textures.emission->get_render_target(1);
    TEST(textures.material) target.material = textures.material->get_render_target(1);
    TEST(textures.transmission) target.transmission = textures.transmission->get_render_target(1);
    TEST(textures.sheen) target.sheen = textures.sheen->get_render_target(1);
    TEST(textures.material_extra) target.material_extra = textures.material_extra->get_render_target(1);
    TEST(textures.motion) target.motion = textures.motion->get_render_target(1);
#undef TEST

    return target;
}

void gbuffer::init(device& dev, uvec2 size, uint32_t buffer_mask, bool temporal)
{
    init(dev, size, buffer_mask, temporal ? buffer_mask : 0);
}

void gbuffer::init(device& dev, uvec2 size, uint32_t buffer_mask, uint32_t temporal_mask)
{
    buffer_mask |= temporal_mask;

    if(buffer_mask & gbuffer::COLOR)
        textures.color.emplace(texture::create_framebuffer(
            dev, size, VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_GENERAL, false,
            temporal_mask & gbuffer::COLOR ? 2 : 1
        ));
    else textures.color.reset();

    if(buffer_mask & gbuffer::DEPTH)
        textures.depth.emplace(texture::create_framebuffer(
            dev, size, VK_FORMAT_D32_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_GENERAL, false,
            temporal_mask & gbuffer::DEPTH ? 2 : 1
        ));
    else textures.depth.reset();

    if(buffer_mask & (gbuffer::NORMAL|gbuffer::TANGENT))
    {
        textures.normal.emplace(texture::create_framebuffer(
            dev, size,
            (buffer_mask & gbuffer::TANGENT) ? VK_FORMAT_R16G16B16A16_SNORM : VK_FORMAT_R16G16_SNORM,
            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_GENERAL, false,
            temporal_mask & (gbuffer::NORMAL|gbuffer::TANGENT) ? 2 : 1
        ));
    }
    else textures.normal.reset();

    if(buffer_mask & gbuffer::FLAT_NORMAL)
        textures.flat_normal.emplace(texture::create_framebuffer(
            dev, size, VK_FORMAT_R16G16_SNORM,
            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_GENERAL, false,
            temporal_mask & gbuffer::FLAT_NORMAL ? 2 : 1
        ));
    else textures.flat_normal.reset();

    if(buffer_mask & gbuffer::POSITION)
        textures.position.emplace(texture::create_framebuffer(
            dev, size, VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_GENERAL, false,
            temporal_mask & gbuffer::POSITION ? 2 : 1
        ));
    else textures.position.reset();

    if(buffer_mask & gbuffer::ALBEDO)
        textures.albedo.emplace(texture::create_framebuffer(
            dev, size, VK_FORMAT_R8G8B8A8_UNORM,
            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_GENERAL, false,
            temporal_mask & gbuffer::ALBEDO ? 2 : 1
        ));
    else textures.albedo.reset();

    if(buffer_mask & gbuffer::EMISSION)
        textures.emission.emplace(texture::create_framebuffer(
            dev, size, VK_FORMAT_R32_UINT,
            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_GENERAL, false,
            temporal_mask & gbuffer::EMISSION ? 2 : 1
        ));
    else textures.emission.reset();

    if(buffer_mask & gbuffer::MATERIAL)
        textures.material.emplace(texture::create_framebuffer(
            dev, size, VK_FORMAT_R8G8B8A8_UNORM,
            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_GENERAL, false,
            temporal_mask & gbuffer::MATERIAL ? 2 : 1
        ));
    else textures.material.reset();

    if(buffer_mask & gbuffer::TRANSMISSION)
        textures.transmission.emplace(texture::create_framebuffer(
            dev, size, VK_FORMAT_R8G8B8A8_UNORM,
            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_GENERAL, false,
            temporal_mask & gbuffer::TRANSMISSION ? 2 : 1
        ));
    else textures.transmission.reset();

    if(buffer_mask & gbuffer::SHEEN)
        textures.sheen.emplace(texture::create_framebuffer(
            dev, size, VK_FORMAT_R8G8B8A8_UNORM,
            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_GENERAL, false,
            temporal_mask & gbuffer::SHEEN ? 2 : 1
        ));
    else textures.sheen.reset();

    if(buffer_mask & gbuffer::MATERIAL_EXTRA)
        textures.material_extra.emplace(texture::create_framebuffer(
            dev, size, VK_FORMAT_R8G8B8A8_UNORM,
            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_GENERAL, false,
            temporal_mask & gbuffer::MATERIAL_EXTRA ? 2 : 1
        ));
    else textures.material_extra.reset();

    if(buffer_mask & gbuffer::MOTION)
        textures.motion.emplace(texture::create_framebuffer(
            dev, size, VK_FORMAT_R32G32_UINT,
            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_GENERAL, false,
            temporal_mask & gbuffer::MOTION ? 2 : 1
        ));
    else textures.motion.reset();
}

}
