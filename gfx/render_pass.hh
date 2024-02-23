#ifndef RAYBASE_GFX_RENDER_PASS_HH
#define RAYBASE_GFX_RENDER_PASS_HH

#include "gpu_pipeline.hh"
#include "render_target.hh"
#include "core/bitset.hh"

namespace rb::gfx
{

class framebuffer;

class render_pass
{
public:
    struct subpass
    {
        bitset outputs;
        bitset inputs;
        bitset preserve;
    };

    struct target
    {
        target(
            VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED,
            VkFormat format = VK_FORMAT_UNDEFINED,
            VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT
        );
        target(const render_target& target);

        VkImageLayout layout;
        VkFormat format;
        VkSampleCountFlagBits samples;
    };

    struct params
    {
        params() = default;
        params(
            argvec<target> targets,
            argvec<subpass> subpasses = {}
        );
        params(
            argvec<render_target*> targets,
            argvec<subpass> subpasses = {}
        );

        std::vector<target> targets;
        std::vector<subpass> subpasses;
        std::vector<VkSubpassDependency> subpass_dependencies;

        // These are all auto-filled to reasonable defaults; you can modify
        // them afterwards to suit your specific needs.
        std::vector<VkAttachmentDescription> attachments;
        std::vector<VkClearValue> clear_values;
        VkRenderPassMultiviewCreateInfo multi_view_info;
    };

    render_pass(device& dev);

    void init(const params& p);

    device& get_device() const;

    operator VkRenderPass() const;

    VkSampleCountFlagBits get_samples() const;
    bool subpass_has_depth(unsigned subpass_index) const;
    unsigned subpass_target_count(unsigned subpass_index) const;
    VkAttachmentDescription get_attachment_description(unsigned attachment_index) const;

    void begin(VkCommandBuffer buf, framebuffer& fb);
    void next(VkCommandBuffer buf);
    void end(VkCommandBuffer buf);

private:
    device* dev;
    params create_params;
    vkres<VkRenderPass> pass;
};

}

#endif

