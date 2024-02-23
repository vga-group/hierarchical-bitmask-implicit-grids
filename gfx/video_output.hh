#ifndef RAYBASE_GFX_VIDEO_OUTPUT_HH
#define RAYBASE_GFX_VIDEO_OUTPUT_HH

#include "device.hh"
#include "event.hh"
#include "core/math.hh"
#include "render_target.hh"

namespace rb::gfx
{

class context;
class video_output
{
public:
    video_output(context& ctx);
    virtual ~video_output();

    context& get_context() const;
    virtual device& get_device() const = 0;

    time_ticks get_frame_time() const;

    // Returns true when command buffers referring to the swapchain must be
    // reset.
    virtual bool begin_frame(event& e) = 0;
    virtual bool end_frame(event e) = 0;

    virtual void reset_swapchain() = 0;

    virtual uint32_t get_image_index() const = 0;
    virtual uint32_t get_image_count() const = 0;

    virtual size_t get_viewport_count() const = 0;
    virtual render_target get_render_target(size_t image_index, size_t viewport = 0) const = 0;
    virtual ivec2 get_size(size_t viewport = 0) const = 0;
    virtual ivec2 get_image_origin(size_t viewport = 0) const = 0;
    virtual VkImageLayout get_output_layout(size_t viewport = 0) const = 0;

    float get_aspect(size_t viewport = 0) const;
    virtual bool require_linear_colors() const;

    // This function is slow. You should only ever use this for debugging.
    // You can open a second window and dump various textures to it for debug
    // purposes.
    void force_blit(render_target& src, argvec<event> wait_events);

private:
    context* ctx;
};

}

#endif


