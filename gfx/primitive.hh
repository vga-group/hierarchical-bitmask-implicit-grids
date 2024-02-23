#ifndef RAYBASE_GFX_PRIMITIVE_HH
#define RAYBASE_GFX_PRIMITIVE_HH

#include "device.hh"
#include "vkres.hh"
#include "resource_loader.hh"
#include "core/math.hh"
#include <vector>

namespace rb::gfx
{

class primitive: public async_loadable_resource<primitive>
{
public:
    using attribute_flag = uint32_t;
    static constexpr attribute_flag POSITION = 1<<0;
    static constexpr attribute_flag NORMAL = 1<<1;
    static constexpr attribute_flag TANGENT = 1<<2;
    static constexpr attribute_flag TEXTURE_UV = 1<<3;
    static constexpr attribute_flag LIGHTMAP_UV = 1<<4;
    static constexpr attribute_flag JOINTS = 1<<5;
    static constexpr attribute_flag WEIGHTS = 1<<6;
    static constexpr attribute_flag COLOR = 1<<7;
    static constexpr attribute_flag PREV_POSITION = 1<<8;
    static constexpr attribute_flag ALL_ATTRIBS = 0xFFFFFFFF;

    struct vertex_data
    {
        // If left empty, you should call ensure_attributes(0) to create indices
        // for "unindexed" geometry.
        std::vector<uint32_t> index;

        // These vectors must either be empty or the same length as the other
        // non-empty vectors (note that the index vector can have a different
        // length).
        std::vector<pvec3> position;
        std::vector<pvec3> normal;
        std::vector<pvec4> tangent;
        std::vector<pvec2> texture_uv;
        std::vector<pvec2> lightmap_uv;
        std::vector<pivec4> joints;
        std::vector<pvec4> weights;
        std::vector<pvec4> color;

        // If the given attributes are missing, they're filled in with
        // placeholder values. Note that position cannot be filled in and is
        // assumed to exist when this is called.
        void ensure_attributes(attribute_flag mask);
        attribute_flag get_available_attributes() const;
        size_t get_vertex_count() const;
        void* get_attribute_data(attribute_flag attribute) const;
    };

    primitive(
        device& dev,
        vertex_data&& data,
        bool opaque = true
    );
    // This constructor is for creating animation copies.
    primitive(const primitive* source);
    primitive(primitive&& other) noexcept = default;

    primitive& operator=(primitive&& other) noexcept = default;

    VkBuffer get_vertex_buffer(attribute_flag attribute) const;
    VkDeviceAddress get_vertex_buffer_address(attribute_flag attribute) const;
    VkBuffer get_index_buffer() const;
    VkDeviceAddress get_index_buffer_address() const;

    bool is_opaque() const;

    aabb calculate_bounding_box() const;
    void set_bounding_box(aabb bounding_box);
    bool get_bounding_box(aabb& bounding_box) const;
    bool has_bounding_box() const;

    size_t get_index_count() const;
    size_t get_vertex_count() const;
    size_t get_attribute_offset(attribute_flag attribute) const;
    size_t get_attribute_size(attribute_flag attribute) const;
    bool has_attribute(attribute_flag attribute) const;
    attribute_flag get_available_attributes() const;

    const vertex_data& get_vertex_data() const;

    void draw(
        VkCommandBuffer buf,
        attribute_flag mask,
        uint32_t num_instances = 1
    ) const;

    static std::vector<VkVertexInputBindingDescription>
    get_bindings(attribute_flag mask);

    static std::vector<VkVertexInputAttributeDescription>
    get_attributes(attribute_flag mask);

    uint64_t get_unique_id() const;

    struct impl_data
    {
        const primitive* source;
        bool opaque;
        vertex_data data;
        bool has_bounding_box;
        aabb bounding_box;
        size_t alignment;
        attribute_flag available_attributes;
        vkres<VkBuffer> vertex_buffer;
        VkDeviceAddress vertex_buffer_address;
        vkres<VkBuffer> index_buffer;
        VkDeviceAddress index_buffer_address;

        // Each primitive is given an unique ID that is never reused. This exists for
        // tracking related acceleration structures in the acceleration structure
        // manager. They don't necessarily map 1:1 to each mesh, which is why they
        // can't be stored directly in the meshes themselves.
        uint64_t unique_id;

        size_t get_attribute_offset(attribute_flag attribute) const;
        size_t get_attribute_size(attribute_flag attribute) const;
        bool has_attribute(attribute_flag attribute) const;
        attribute_flag get_all_available_attributes() const;
    };
private:
    using super_impl_data = async_loadable_resource<primitive>::impl_data;
    static std::atomic_uint64_t id_counter;
};

}

#endif
