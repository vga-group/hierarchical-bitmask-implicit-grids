#ifndef RAYBASE_GFX_GPU_PIPELINE_HH
#define RAYBASE_GFX_GPU_PIPELINE_HH

#include "descriptor_set.hh"
#include <map>

#define RB_SHADER_REGISTRY_ENTRY(binary, binary_path, source_path) \
    namespace rb:: binary##_registry_detail { \
        static inline rb::gfx::registry_helper helper(binary, binary_path, source_path); \
    }

namespace rb::gfx
{

using specialization_info = std::map<uint32_t /*index*/, uint32_t>;

struct specialization_info_wrapper
{
    specialization_info_wrapper(const specialization_info& info);
    specialization_info_wrapper(specialization_info_wrapper&& other) noexcept = default;
    specialization_info_wrapper& operator=(specialization_info_wrapper&& other) = default;

    std::vector<VkSpecializationMapEntry> entries;
    std::vector<uint32_t> data;
    VkSpecializationInfo info;
};

struct shader_data
{
    shader_data() = default;

    template<typename U>
    shader_data(
        const U& init,
        const specialization_info& specialization = {}
    ): bytes(sizeof(init[0]) * std::size(init)), data(std::data(init)), specialization(specialization) {}

    size_t bytes = 0;
    const uint32_t* data = nullptr;
    specialization_info specialization;
};

// Tracks all linked shaders, used for hot reloading.
struct shader_registry_entry
{
    std::string binary_path;
    std::string source_path;
    const uint32_t* data_ptr;
};
std::vector<shader_registry_entry>& get_shader_registry();

struct registry_helper
{
    registry_helper(const uint32_t* data, const char* binary_path, const char* source_path);
};

class gpu_pipeline
{
public:
    gpu_pipeline(device& dev, VkPipelineBindPoint bind_point);
    gpu_pipeline(gpu_pipeline&& other) noexcept = default;
    ~gpu_pipeline();

    void push_constants(VkCommandBuffer buf, const void* data);

    void bind(VkCommandBuffer buf);
    void set_descriptors(
        VkCommandBuffer buf,
        const descriptor_set& set,
        uint32_t index = 0,
        uint32_t set_index = 0
    );
    void push_descriptors(
        VkCommandBuffer buf,
        push_descriptor_set& set,
        uint32_t set_index = 0
    );

    // This function will be called whenever a shader is loaded. It can be used
    // to log shader loading or even dynamically replace the shader!
    static void add_shader_load_hook(
        std::function<void(shader_data&)>&& hook
    );
    static void get_hooked_shader(shader_data& data);

protected:
    void init_bindings(
        size_t push_constant_size = 0,
        argvec<VkDescriptorSetLayout> sets = {}
    );

    vkres<VkShaderModule> load_shader(shader_data data);

    static std::function<void(shader_data&)> shader_load_callback;
    device* dev;
    vkres<VkPipeline> pipeline;
    vkres<VkPipelineLayout> pipeline_layout;
    VkPipelineBindPoint bind_point;
    size_t push_constant_size;
};

}

#endif
