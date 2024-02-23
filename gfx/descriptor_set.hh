#ifndef RAYBASE_GFX_DESCRIPTOR_SET_HH
#define RAYBASE_GFX_DESCRIPTOR_SET_HH

#include "context.hh"
#include "vkres.hh"
#include "core/argvec.hh"
#include <vector>
#include <set>

namespace rb::gfx
{

struct shader_data;
struct raster_shader_data;
struct ray_tracing_shader_data;
class texture;
class sampler;

class descriptor_set_layout
{
public:
    descriptor_set_layout(device& dev, bool push_descriptor_set);
    descriptor_set_layout(descriptor_set_layout&& other) noexcept = default;
    ~descriptor_set_layout();

    void add(
        std::string_view name,
        const VkDescriptorSetLayoutBinding& binding,
        VkDescriptorBindingFlags flags = 0
    );
    void add(const shader_data& data, uint32_t target_set_index = 0);
    void add(const raster_shader_data& data, uint32_t target_set_index = 0);
    void add(const ray_tracing_shader_data& data, uint32_t target_set_index = 0);
    void set_binding_params(
        std::string_view name,
        uint32_t count,
        VkDescriptorBindingFlags flags = 0
    );

    struct set_binding: VkDescriptorSetLayoutBinding
    {
        VkDescriptorBindingFlags flags;
    };

    set_binding find_binding(std::string_view name) const;
    VkDescriptorSetLayout get_layout() const;

protected:
    void refresh() const;

    device* dev;
    bool push_descriptor_set;
    mutable bool dirty = true;
    mutable std::vector<VkDescriptorSetLayoutBinding> bindings;
    mutable vkres<VkDescriptorSetLayout> layout;
    mutable uint32_t descriptor_pool_capacity = 0;

    std::set<std::string> descriptor_names;
    std::unordered_map<std::string_view, set_binding> named_bindings;
};

class descriptor_set: public descriptor_set_layout
{
public:
    descriptor_set(device& dev);
    descriptor_set(descriptor_set&& other) noexcept = default;
    ~descriptor_set();

    void reset(uint32_t count);

    void set_image(
        uint32_t index,
        std::string_view name,
        argvec<VkImageView> views,
        argvec<VkSampler> samplers = {}
    );

    void set_texture(
        uint32_t index,
        std::string_view name,
        const texture& tex,
        const sampler& s
    );

    void set_image(
        uint32_t index,
        std::string_view name,
        const texture& tex
    );

    void set_buffer(
        uint32_t index,
        std::string_view name,
        argvec<VkBuffer> buffer,
        argvec<uint32_t> offsets = {}
    );

    void set_acceleration_structure(
        uint32_t index,
        std::string_view name,
        VkAccelerationStructureKHR tlas
    );

    void bind(
        VkCommandBuffer buf,
        VkPipelineLayout pipeline_layout,
        VkPipelineBindPoint bind_point,
        uint32_t alternative_index,
        uint32_t set_index
    ) const;

protected:
    std::vector<VkDescriptorSet> alternatives;
    vkres<VkDescriptorPool> pool;
};

class push_descriptor_set: public descriptor_set_layout
{
public:
    push_descriptor_set(device& dev);
    push_descriptor_set(push_descriptor_set&& other) noexcept = default;
    ~push_descriptor_set();

    void set_image(
        std::string_view name,
        argvec<VkImageView> views,
        argvec<VkSampler> samplers = {}
    );

    void set_texture(
        std::string_view name,
        const texture& tex,
        const sampler& s
    );

    void set_image(
        std::string_view name,
        const texture& tex
    );

    void set_buffer(
        std::string_view name,
        argvec<VkBuffer> buffer,
        argvec<uint32_t> offsets = {}
    );

    void set_acceleration_structure(
        std::string_view name,
        VkAccelerationStructureKHR tlas
    );

    void push(
        VkCommandBuffer buf,
        VkPipelineLayout pipeline_layout,
        VkPipelineBindPoint bind_point,
        uint32_t set_index
    );

protected:
    uint32_t image_info_index;
    std::vector<std::vector<VkDescriptorImageInfo>> tmp_image_infos;
    uint32_t buffer_info_index;
    std::vector<std::vector<VkDescriptorBufferInfo>> tmp_buffer_infos;
    uint32_t as_info_index;
    std::vector<VkAccelerationStructureKHR> tmp_as;
    std::vector<VkWriteDescriptorSetAccelerationStructureKHR> tmp_as_infos;
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<void*> dependencies;
};

}

#endif
