#ifndef RAYBASE_GFX_EVENT_HH
#define RAYBASE_GFX_EVENT_HH
#include "volk.h"
#include <functional>

namespace rb::gfx
{

class device;
struct event
{
    VkSemaphore timeline_semaphore = VK_NULL_HANDLE;
    uint64_t value = 0;

    void wait(device& dev);
    void finish_callback(device& dev, std::function<void()>&& callback);
    bool finished(const device& dev) const;
};

}

#endif
