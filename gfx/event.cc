#include "event.hh"
#include "vulkan_helpers.hh"

namespace rb::gfx
{

void event::wait(device& dev)
{
    wait_timeline_semaphore(dev, timeline_semaphore, value);
}

void event::finish_callback(device& dev, std::function<void()>&& callback)
{
    // Yeah, I know it's kinda weird to use the GC for this, but it already
    // tracks the semaphore values so ¯\_(ツ)_/¯
    dev.gc.add_trigger(timeline_semaphore, value, std::move(callback));
}

bool event::finished(const device& dev) const
{
    if(timeline_semaphore == VK_NULL_HANDLE) return true;
    uint64_t value = 0;
    vkGetSemaphoreCounterValue(dev.logical_device, timeline_semaphore, &value);
    return value >= this->value;
}

}
