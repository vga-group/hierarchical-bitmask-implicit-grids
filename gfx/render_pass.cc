#include "render_pass.hh"
#include "vulkan_helpers.hh"
#include "framebuffer.hh"

namespace rb::gfx
{

render_pass::target::target(
    VkImageLayout layout,
    VkFormat format,
    VkSampleCountFlagBits samples
): layout(layout), format(format), samples(samples)
{
}

render_pass::target::target(
    const render_target& tgt
): layout(tgt.get_layout()), format(tgt.get_format()), samples(tgt.get_samples())
{
}

render_pass::params::params(
    argvec<render_target*> targets,
    argvec<subpass> subpasses
):
    params(
        targets.map([&](const render_target* rt){
            return rt ? target(*rt) : target();
        }),
        subpasses
    )
{
}

render_pass::params::params(
    argvec<target> targets,
    argvec<subpass> subpasses
):  targets(targets.begin(), targets.end()),
    subpasses(subpasses.begin(), subpasses.end())
{
    for(size_t i = 0; i < targets.size(); ++i)
    {
        if(targets[i].format == VK_FORMAT_UNDEFINED)
            continue;

        VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
        VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE;
        VkImageLayout layout = targets[i].layout;
        if(layout == VK_IMAGE_LAYOUT_UNDEFINED)
            load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;

        attachments.push_back(VkAttachmentDescription{
            0,
            targets[i].format,
            targets[i].samples,
            load_op,
            store_op,
            load_op,
            store_op,
            layout,
            VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR
        });
        VkClearValue clear;
        if(format_is_depth(targets[i].format))
            clear.depthStencil = {0.0f, 0};
        else clear.color = {1.0f, 0.0f, 0.0f, 1.0f};
        clear_values.push_back(clear);
    }

    if(subpasses.size() == 0)
    {
        bitset outputs;
        for(unsigned i = 0; i < targets.size(); ++i)
            outputs.insert(i);
        this->subpasses.push_back({outputs, {}, {}});
    }

    uint32_t prev_subpass = VK_SUBPASS_EXTERNAL;
    for(uint32_t i = 0; i < this->subpasses.size() + 1; ++i)
    {
        VkSubpassDependency subpass_dependency = {
            prev_subpass, i == this->subpasses.size() ? VK_SUBPASS_EXTERNAL : i,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            0
        };
        if(subpass_dependency.srcSubpass == VK_SUBPASS_EXTERNAL)
        {
            subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            subpass_dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        }
        if(subpass_dependency.dstSubpass == VK_SUBPASS_EXTERNAL)
        {
            subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            subpass_dependency.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        }

        bool subpass_has_depth = false;
        if(i < this->subpasses.size())
        {
            for(unsigned j: this->subpasses[i].outputs)
                if(format_is_depth(targets[j].format))
                    subpass_has_depth = true;
        }

        if(subpass_has_depth)
        {
            subpass_dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            subpass_dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            subpass_dependency.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            subpass_dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        subpass_dependencies.push_back(subpass_dependency);
        prev_subpass = i;
    }
    multi_view_info = {
        VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
        nullptr,
        0,
        nullptr,
        0,
        nullptr,
        0,
        nullptr
    };
}

render_pass::render_pass(device& dev)
: dev(&dev)
{
}

void render_pass::init(const params& p)
{
    create_params = p;
    std::vector<VkSubpassDescription> subpasses;
    std::vector<std::vector<VkAttachmentReference>> tmp_refs;
    std::vector<std::vector<uint32_t>> tmp_preserve;

    for(subpass sp: p.subpasses)
    {
        std::vector<VkAttachmentReference> input;
        std::vector<VkAttachmentReference> color;
        std::vector<VkAttachmentReference> depth;
        std::vector<uint32_t> preserve;
        for(size_t i = 0, j = 0; i < p.targets.size(); ++i)
        {
            VkAttachmentReference ref = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR};
            if(p.targets[i].format != VK_FORMAT_UNDEFINED)
                ref = {(uint32_t)j, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR};

            if(sp.inputs.contains(i))
            {
                ref.layout = sp.outputs.contains(i) ?
                    VK_IMAGE_LAYOUT_GENERAL :
                    VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
                input.push_back(ref);
            }

            if(sp.outputs.contains(i))
            {
                if(format_is_depth(p.targets[i].format)) depth.push_back(ref);
                else color.push_back(ref);
            }

            if(sp.preserve.contains(i))
                preserve.push_back(ref.attachment);

            if(p.targets[i].format != VK_FORMAT_UNDEFINED)
                j++;
        }

        VkSubpassDescription subpass = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            (uint32_t)input.size(), input.data(),
            (uint32_t)color.size(), color.data(),
            nullptr,
            depth.size() != 0 ? depth.data() : nullptr,
            (uint32_t)preserve.size(),
            preserve.data()
        };
        subpasses.push_back(subpass);
        tmp_refs.emplace_back(std::move(input));
        tmp_refs.emplace_back(std::move(color));
        tmp_refs.emplace_back(std::move(depth));
        tmp_preserve.emplace_back(std::move(preserve));
    }

    // Setup fixed function structs
    VkRenderPass tmp_render_pass;
    VkRenderPassCreateInfo render_pass_info = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        p.multi_view_info.subpassCount != 0 ? &p.multi_view_info : nullptr,
        0,
        (uint32_t)create_params.attachments.size(),
        create_params.attachments.data(),
        (uint32_t)subpasses.size(),
        subpasses.data(),
        (uint32_t)p.subpass_dependencies.size(),
        p.subpass_dependencies.data()
    };
    vkCreateRenderPass(
        dev->logical_device,
        &render_pass_info,
        nullptr,
        &tmp_render_pass
    );
    pass = vkres(*dev, tmp_render_pass);
}

device& render_pass::get_device() const
{
    return *dev;
}

render_pass::operator VkRenderPass() const
{
    return *pass;
}

VkSampleCountFlagBits render_pass::get_samples() const
{
    return create_params.attachments[0].samples;
}

bool render_pass::subpass_has_depth(unsigned subpass_index) const
{
    for(unsigned j: create_params.subpasses[subpass_index].outputs)
        if(format_is_depth(create_params.targets[j].format))
            return true;
    return false;
}

unsigned render_pass::subpass_target_count(unsigned subpass_index) const
{
    return create_params.subpasses[subpass_index].outputs.size();
}

VkAttachmentDescription render_pass::get_attachment_description(
    unsigned attachment_index
) const {
    RB_CHECK(
        attachment_index >= create_params.attachments.size(),
        "Trying to index an attachment that doesn't exist"
    );
    return create_params.attachments[attachment_index];
}

void render_pass::begin(VkCommandBuffer buf, framebuffer& fb)
{
    uvec2 size = fb.get_size();
    VkRenderPassBeginInfo render_pass_info = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        nullptr,
        pass,
        fb,
        {{0,0}, {size.x, size.y}},
        (uint32_t)create_params.clear_values.size(),
        create_params.clear_values.data()
    };
    vkCmdBeginRenderPass(buf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    dev->gc.depend((VkFramebuffer)fb, buf);
    dev->gc.depend(pass, buf);
}

void render_pass::next(VkCommandBuffer buf)
{
    vkCmdNextSubpass(buf, VK_SUBPASS_CONTENTS_INLINE);
}

void render_pass::end(VkCommandBuffer buf)
{
    vkCmdEndRenderPass(buf);
}

}

