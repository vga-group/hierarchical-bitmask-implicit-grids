#ifndef RAYBASE_GFX_TIMER_HH
#define RAYBASE_GFX_TIMER_HH
#include "device.hh"
#include <string>

namespace rb::gfx
{

class timer
{
public:
    timer(device& dev, const std::string& name);
    timer(const timer& other) = delete;
    timer(timer&& other);
    ~timer();

    const std::string& get_name() const;

    void start(VkCommandBuffer buf, uint32_t image_index, VkPipelineStageFlags2KHR stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);
    void stop(VkCommandBuffer buf, uint32_t image_index, VkPipelineStageFlags2KHR stage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);
private:
    device* dev;
    int32_t id;
    std::string name;
};

}

#endif
