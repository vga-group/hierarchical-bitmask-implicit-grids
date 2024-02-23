#ifndef RAYBASE_GFX_PATH_TRACER_HH
#define RAYBASE_GFX_PATH_TRACER_HH

#include "ray_tracing_pipeline.hh"
#include "environment_map.hh"
#include "camera.hh"

namespace rb::gfx
{

class scene_stage;
class clustering_stage;

struct gpu_path_tracer_config
{
    pvec4 light_type_sampling_probabilities;
    float min_ray_dist;
    float max_ray_dist;
    float film_parameter;
    float regularization_gamma;
    float clamping_threshold;
    int32_t pad[3];
};

// This class acts as a common implementation of path tracing for multiple
// different methods. Each individual method gets to override the ray generation
// shader.
struct path_tracer
{
    enum class decal_mode: int
    {
        NEVER = 0,
        PRIMARY_ONLY = 1,
        ALWAYS = 2
    };

    inline static constexpr unsigned PINHOLE_APERTURE = 0;
    inline static constexpr unsigned DISK_APERTURE = 1;

    struct options
    {
        unsigned max_bounces = 4;
        float min_ray_dist = 1e-5;
        float max_ray_dist = 1e9;

        film_filter filter;
        aperture ap = {6};

        // Can be used to disable alpha blending for a performance benefit.
        bool opaque_only = false;

        // Can be used to only render static objects (may be useful for baking)
        bool static_only = false;

        // Use blue noise for light sampling?
        bool light_sampling_blue_noise = true;

        // Show lights in primary rays or not?
        bool show_lights_directly = false;

        bool cull_back_faces = false;

        // You can trade noise for a bit of bias with path space regularization.
        // With it enabled, some lighting can appear blurrier than it should.
        bool path_space_regularization = false;
        // Use PSR with next-event estimation rays only?
        bool path_space_regularization_nee_only = false;
        // Good gamma values are usually small, below 1.
        float path_space_regularization_gamma = 0.5f;

        // You can clamp overly bright indirect bounces with this flag. This
        // causes a lot of bias, but is very effective at removing fireflies.
        // Raybase uses a clamping variation that preserves color saturation.
        bool clamping = false;
        float clamping_threshold = 10.0f;

        // Sample all or just one light with NEE rays?
        bool nee_samples_all_lights = false;

        decal_mode decals = decal_mode::ALWAYS;
    };

    path_tracer(clustering_stage& clustering);
    void init(
        argvec<uint32_t> rgen_spirv,
        size_t push_constant_buffer_size,
        VkDescriptorSetLayout layout,
        const options& opt
    );

    gpu_path_tracer_config bind(VkCommandBuffer cmd);

    clustering_stage* cluster_data;
    ray_tracing_pipeline pipeline;
    descriptor_set set;
    options opt;
    VkBuffer envmap_alias_table;
    environment_map_alias_table_generator emat_gen;
};

}

#endif
