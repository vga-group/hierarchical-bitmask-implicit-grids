#ifndef RAYBASE_RADIX_SORT_HH
#define RAYBASE_RADIX_SORT_HH

#include "core/ecs.hh"
#include "device.hh"
#include "compute_pipeline.hh"

namespace rb::gfx
{

class radix_sort
{
public:
    radix_sort(device& dev, size_t max_count, size_t keyval_dwords = 2);
    ~radix_sort();

    vkres<VkBuffer> create_keyval_buffer() const;

    void sort(
        VkCommandBuffer cmd,
        VkBuffer payload,
        VkBuffer keyvals,
        VkBuffer output,
        size_t payload_size,
        size_t count,
        size_t key_bits = 32
    );

    // Writes the resulting sort mapping into the given output buffer.
    // Essentially, output[unsorted index] = index after sorting
    void get_sort_index(
        VkCommandBuffer cmd,
        VkBuffer output,
        size_t count
    );

    // Using the sort order from the previous sort(), sort another buffer.
    // Use it as a last _resort_ ;)
    void resort(
        VkCommandBuffer cmd,
        VkBuffer payload,
        VkBuffer output,
        size_t payload_size,
        size_t count
    );

private:
    device* dev;
    void* rs_instance;
    size_t max_count;
    size_t keyval_dwords;
    size_t alloc_alignment;
    size_t alloc_keyvals_size;
    size_t alloc_internal_size;
    VkDescriptorBufferInfo keyvals_sorted;
    vkres<VkBuffer> scratch;
    push_descriptor_set sort_set;
    push_descriptor_set order_set;
    compute_pipeline placement_pipeline;
    compute_pipeline order_pipeline;
};

}

#endif

