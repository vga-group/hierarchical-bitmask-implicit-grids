#ifndef RAYBASE_GFX_RENDER_STAGE_HH
#define RAYBASE_GFX_RENDER_STAGE_HH

#include "device.hh"
#include "vkres.hh"
#include "event.hh"
#include "core/argvec.hh"
#include <vector>

namespace rb::gfx
{

class gpu_buffer;
class render_stage
{
public:
    render_stage(device& dev);
    render_stage(render_stage&& other) noexcept = default;
    render_stage(const render_stage& other) = delete;
    virtual ~render_stage() = default;

    event run(uint32_t frame_index, argvec<event> wait);

    device& get_device() const;

protected:
    virtual void update_buffers(uint32_t frame_index);

    // Compute pipelines only:
    VkCommandBuffer compute_commands(bool one_time_submit = false);

    // The returned event is the one that is used the next time this command
    // runs; if you don't reset commands every time update_buffers() runs, it 
    // gets outdated fast.
    event use_compute_commands(VkCommandBuffer buf, uint32_t frame_index);

    // Graphics pipelines only:
    VkCommandBuffer graphics_commands(bool one_time_submit = false);
    event use_graphics_commands(VkCommandBuffer buf, uint32_t frame_index);

    // Transfer pipelines only: (note that these can't be timed!)
    VkCommandBuffer transfer_commands(bool one_time_submit = false);
    event use_transfer_commands(VkCommandBuffer buf, uint32_t frame_index);

    // Safe bulk buffer upload: Uploads everything, then issues a common barrier.
    void upload(
        VkCommandBuffer buf,
        argvec<gpu_buffer*> buffers,
        uint32_t frame_index,
        bool barrier = true
    );
    void transfer(argvec<gpu_buffer*> buffers, uint32_t frame_index);

    // General:
    void clear_commands();
    bool has_commands() const;

    device* dev;

private:
    VkCommandBuffer commands(VkCommandPool pool, bool one_time_submit = false);
    void use_commands(VkCommandBuffer buf, VkCommandPool pool, uint32_t frame_index);

    VkCommandPool graphics_pool;
    VkCommandPool compute_pool;
    VkCommandPool transfer_pool;

    bool first_frame;
    std::vector<std::vector<vkres<VkCommandBuffer>>> command_buffers;
    std::vector<event> wait_events;
};

}

#endif

