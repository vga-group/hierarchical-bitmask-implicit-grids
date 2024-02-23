#ifndef RAYBASE_ENVIRONMENT_MAP_HH
#define RAYBASE_ENVIRONMENT_MAP_HH
#include "core/transformable.hh"
#include "compute_pipeline.hh"
#include "descriptor_set.hh"
#include "texture.hh"
#include "framebuffer.hh"
#include "sampler.hh"
#include <optional>

namespace rb::gfx
{

struct environment_map
{
    enum parallax_type
    {
        NONE = 0,
        BOX,
        SPHERE
    };

    environment_map(
        const texture* cubemap = nullptr,
        parallax_type parallax = NONE
    );
    ~environment_map();

    aabb get_aabb(const transformable& self) const;

    bool point_inside(const transformable& self, vec3 p) const;
    void refresh(unsigned render_samples = 4096, unsigned filter_samples = 4096);
    void refresh_filter(unsigned filter_samples = 4096);

    const texture* cubemap;
    float guard_radius;
    parallax_type parallax;
    vec2 clip_range;

    unsigned current_render_samples;
    unsigned total_render_samples;

    unsigned current_filter_samples;
    unsigned total_filter_samples;
};

entity find_sky_envmap(scene& s);

// An additional component that can be added to environment maps in order to
// allow for importance sampling with ray tracing methods.
struct environment_map_alias_table
{
    vkres<VkBuffer> alias_table;
    // The average luminance is used for certain sampling modes.
    float average_luminance;
};

class environment_map_alias_table_generator
{
public:
    environment_map_alias_table_generator(device& dev);

    // Warning: this is dog slow, it partially runs on the CPU. The algorithm
    // is pretty non-trivial to parallelize, so this is very far from real-time.
    void generate(environment_map_alias_table& table, const texture& cubemap);

private:
    device* dev;
    sampler envmap_sampler;
    compute_pipeline importance_pipeline;
    push_descriptor_set importance_set;
};

// Turns equirectangular envmaps into cubemaps for easier consumption in the
// engine.
class equirectangular_to_cubemap_converter
{
public:
    equirectangular_to_cubemap_converter(device& dev);

    // Warning: this is slow-ish, it waits for equirectangular to load first.
    // If target_resolution is -1, it's deduced from the source texture.
    texture convert(const texture& equirectangular, int target_resolution = -1);

private:
    device* dev;
    sampler envmap_sampler;
    compute_pipeline pipeline;
    push_descriptor_set set;
};

}

#endif

