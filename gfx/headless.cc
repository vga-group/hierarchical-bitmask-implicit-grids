#include "headless.hh"
#include "vulkan_helpers.hh"
#include "context.hh"
#include "core/error.hh"
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <numeric>

namespace rb::gfx
{

headless::headless(
    context& ctx,
    ivec2 size,
    void* pixel_data,
    VkFormat format
):  video_output(ctx), dev(nullptr), size(size), pixel_data(pixel_data),
    format(format)
{
    bytes = size.x * size.y * get_format_size(format);
    select_device();
    init_swapchain();
}

headless::~headless()
{
    dev->finish();
    deinit_swapchain();
}

device& headless::get_device() const
{
    return *dev;
}

bool headless::begin_frame(event& e)
{
    dev->begin_frame();

    // Convert the binary semaphore into a timeline semaphore
    e = dev->queue_graphics({}, {}, false);

    return false;
}

bool headless::end_frame(event e)
{
    dev->end_frame(e);
    event copy = dev->queue_graphics({e}, {*copy_cmd}, false);

    if(pixel_data)
    {
        copy.wait(*dev);
        const VmaAllocator& allocator = dev->allocator;
        void* buf_data;
        vmaMapMemory(allocator, read_buf.get_allocation(), &buf_data);
        memcpy(pixel_data, buf_data, bytes);
        vmaUnmapMemory(allocator, read_buf.get_allocation());
    }

    return false;
}

void headless::reset_swapchain() {}

uint32_t headless::get_image_index() const
{
    return 0;
}

uint32_t headless::get_image_count() const
{
    return pixel_data ? 1 : 0;
}

size_t headless::get_viewport_count() const
{
    return pixel_data ? 1 : 0 ;
}

render_target headless::get_render_target(size_t, size_t) const
{
    return render_target(
        *render_image,
        *render_view,
        VK_IMAGE_LAYOUT_UNDEFINED,
        uvec2(size), 0, 1,
        VK_SAMPLE_COUNT_1_BIT,
        format
    );
}

ivec2 headless::get_size(size_t) const
{
    return size;
}

ivec2 headless::get_image_origin(size_t) const
{
    return ivec2(0);
}

VkImageLayout headless::get_output_layout(size_t) const
{
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
}

void headless::select_device()
{
    dev = nullptr;
    for(size_t i = 0; i < get_context().get_device_count(); ++i)
    {
        device* d = get_context().get_device(i);
        if(!d->supports_required_extensions)
            continue;

        if(
            dev == nullptr ||
            d->physical_device_props.properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU
        ) dev = d;

        if(d->supports_ray_tracing_pipeline)
            break;
    }

    if(!dev)
        RB_PANIC(
            "Unable to find a device with required extensions and ability to "
            "draw to window!"
        );

    dev->open(VK_NULL_HANDLE);
}

void headless::init_swapchain()
{
    if(!pixel_data)
        return;

    event e;
    render_image = create_gpu_image(
        *dev, e, uvec3(size, 1), 1, format,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|
        VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    );
    e.wait(*dev);

    render_view = create_image_view(*dev, render_image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    read_buf = create_readback_buffer(*dev, bytes);

    VkCommandPool pool = dev->get_graphics_pool();
    copy_cmd = vkres<VkCommandBuffer>(*dev, pool, dev->allocate_command_buffer(pool));
    VkCommandBufferBeginInfo begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        nullptr
    };
    vkBeginCommandBuffer(copy_cmd, &begin_info);

    VkBufferImageCopy copy = {
        0, 0, 0,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        {0, 0, 0},
        {(uint32_t)size.x, (uint32_t)size.y, 1}
    };
    vkCmdCopyImageToBuffer(
        *copy_cmd,
        *render_image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        *read_buf, 1, &copy
    );

    vkEndCommandBuffer(copy_cmd);
}

void headless::deinit_swapchain()
{
    dev->finish();
    render_view.reset();
    render_image.reset();
}

}
