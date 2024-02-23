#include "render_stage.hh"
#include "vulkan_helpers.hh"
#include "gpu_buffer.hh"
#include "core/stack_allocator.hh"

namespace rb::gfx
{

render_stage::render_stage(device& dev)
: dev(&dev), first_frame(true)
{
    command_buffers.resize(dev.get_in_flight_count());
    graphics_pool = dev.get_graphics_pool();
    compute_pool = dev.get_compute_pool();
    transfer_pool = dev.get_transfer_pool();
}

event render_stage::run(uint32_t frame_index, argvec<event> wait)
{
    update_buffers(frame_index);

    wait_events.clear();
    wait_events.insert(wait_events.end(), wait.begin(), wait.end());

    event e = {};
    for(size_t i = 0; i < command_buffers[frame_index].size(); ++i)
    {
        const vkres<VkCommandBuffer>& cmd = command_buffers[frame_index][i];

        if(cmd.get_pool() == graphics_pool)
            e = dev->queue_graphics(wait_events, *cmd);
        else if(cmd.get_pool() == compute_pool)
            e = dev->queue_compute(wait_events, *cmd);
        else if(cmd.get_pool() == transfer_pool)
            e = dev->queue_transfer(wait_events, *cmd);

        wait_events.clear();
        wait_events.push_back(e);
    }

    return e;
}

device& render_stage::get_device() const
{
    return *dev;
}

void render_stage::update_buffers(uint32_t)
{
}

VkCommandBuffer render_stage::compute_commands(bool one_time_submit)
{
    return commands(compute_pool, one_time_submit);
}

event render_stage::use_compute_commands(VkCommandBuffer buf, uint32_t frame_index)
{
    use_commands(buf, compute_pool, frame_index);
    return dev->get_next_compute_frame_event(command_buffers.size()-1);
}

VkCommandBuffer render_stage::graphics_commands(bool one_time_submit)
{
    return commands(graphics_pool, one_time_submit);
}

event render_stage::use_graphics_commands(VkCommandBuffer buf, uint32_t frame_index)
{
    use_commands(buf, graphics_pool, frame_index);
    return dev->get_next_graphics_frame_event(command_buffers.size()-1);
}

VkCommandBuffer render_stage::transfer_commands(bool one_time_submit)
{
    return commands(transfer_pool, one_time_submit);
}

event render_stage::use_transfer_commands(VkCommandBuffer buf, uint32_t frame_index)
{
    use_commands(buf, transfer_pool, frame_index);
    return dev->get_next_transfer_frame_event(command_buffers.size()-1);
}

void render_stage::upload(
    VkCommandBuffer cmd,
    argvec<gpu_buffer*> buffers,
    uint32_t frame_index,
    bool barrier
){
    if(buffers.size() == 0)
         return;

    for(gpu_buffer* b: buffers)
    {
        if(b->get_size() == 0)
            continue;
        VkBuffer source = b->get_staging_buffer(frame_index);
        VkBufferCopy copy = {0, 0, b->get_size()};
        vkCmdCopyBuffer(cmd, source, *b, 1, &copy);
        dev->gc.depend((VkBuffer)*b, cmd);
    }

    if(barrier)
    {
        auto barriers = stack_allocate<VkBufferMemoryBarrier2KHR>(buffers.size());
        size_t count = 0;
        for(size_t i = 0; i < buffers.size(); ++i)
        {
            if(buffers[i]->get_size() == 0)
                continue;
            barriers[count] = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
                nullptr,
                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
                VK_ACCESS_2_MEMORY_WRITE_BIT_KHR | VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR,
                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
                VK_ACCESS_2_MEMORY_WRITE_BIT_KHR | VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                *buffers[i],
                0,
                VK_WHOLE_SIZE
            };
            count++;
        }
        VkDependencyInfoKHR dependency_info = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
            nullptr,
            0,
            0, nullptr,
            (uint32_t)count, barriers.get(),
            0, nullptr
        };
        vkCmdPipelineBarrier2KHR(cmd, &dependency_info);
    }
}

void render_stage::transfer(argvec<gpu_buffer*> buffers, uint32_t frame_index)
{
    VkCommandBuffer buf = transfer_commands(false);
    upload(buf, buffers, frame_index, false);
    use_transfer_commands(buf, frame_index);
}

VkCommandBuffer render_stage::commands(VkCommandPool pool, bool one_time_submit)
{
    VkCommandBuffer buf = dev->allocate_command_buffer(pool);
    VkCommandBufferBeginInfo begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        (VkCommandBufferUsageFlags)(one_time_submit ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT),
        nullptr
    };
    vkBeginCommandBuffer(buf, &begin_info);

    return buf;
}

void render_stage::use_commands(VkCommandBuffer buf, VkCommandPool pool, uint32_t frame_index)
{
    vkEndCommandBuffer(buf);
    command_buffers[frame_index].emplace_back(*dev, pool, buf);
}

void render_stage::clear_commands()
{
    for(auto& cmds: command_buffers)
        cmds.clear();
}

bool render_stage::has_commands() const
{
    for(const auto& cmds: command_buffers)
        if(!cmds.empty()) return true;
    return false;
}

}
