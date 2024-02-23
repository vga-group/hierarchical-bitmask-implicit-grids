#include "timer.hh"

namespace rb::gfx
{

timer::timer(device& dev, const std::string& name)
: dev(&dev), name(name)
{
    id = dev.add_timer(name);
}

timer::timer(timer&& other)
: dev(other.dev), id(other.id)
{
    other.dev = nullptr;
    other.id = -1;
}

timer::~timer()
{
    if(dev)
        dev->remove_timer(id);
}

const std::string& timer::get_name() const
{
    return name;
}

void timer::start(VkCommandBuffer buf, uint32_t image_index, VkPipelineStageFlags2KHR stage)
{
    if(id >= 0)
    {
        VkQueryPool pool = dev->get_timestamp_query_pool(image_index);
        vkCmdResetQueryPool(buf, pool, id*2, 2);
        vkCmdWriteTimestamp2KHR(buf, stage, pool, (uint32_t)id*2);
    }
}

void timer::stop(VkCommandBuffer buf, uint32_t image_index, VkPipelineStageFlags2KHR stage)
{
    if(id >= 0)
    {
        VkQueryPool pool = dev->get_timestamp_query_pool(image_index);
        vkCmdWriteTimestamp2KHR(buf, stage, pool, (uint32_t)id*2+1);
    }
}

}
