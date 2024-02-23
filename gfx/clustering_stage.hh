#ifndef RAYBASE_CLUSTERING_STAGE_HH
#define RAYBASE_CLUSTERING_STAGE_HH

#include "scene_stage.hh"
#include "ray_tracing_pipeline.hh"

namespace rb::gfx
{

class clustering_stage: public render_stage
{
public:

    struct options
    {
        // Yeah, we can do that and use just ~50kb :-) You should use large
        // values here, they don't cost as much as you think (scaling is linear
        // instead of cubic) and the clustering is done in a world-space
        // structure that covers the entire scene! The maximum is 65536.
        uint32_t light_cluster_resolution = 1024;

        // Decals are also clustered, just like lights. They live in a separate
        // cluster, so parameters can be adjusted separately. The same caveats
        // apply.
        uint32_t decal_cluster_resolution = 512;
    };

    clustering_stage(scene_stage& s, const options& opt);
    clustering_stage(clustering_stage&&) = delete;
    ~clustering_stage();

    void get_specialization_info(specialization_info& info) const;
    void get_light_cluster_axis_buffer_offsets(uint32_t* output) const;
    void get_decal_cluster_axis_buffer_offsets(uint32_t* output) const;

    scene_stage* get_scene_data() const;

protected:
    void update_buffers(uint32_t frame_index) override;

private:
    friend class scene_stage;

    void run_light_clustering(VkCommandBuffer cmd, uint32_t frame_index);
    void run_decal_clustering(VkCommandBuffer cmd, uint32_t frame_index);

    options opt;
    scene_stage* scene_data;
    timer stage_timer;
    timer sort_timer;
    timer range_timer;
    timer bitmask_timer;
    timer hierarchy_timer;
    uint64_t last_update_frame;

    std::optional<radix_sort> sorter;
    vkres<VkBuffer> light_cluster_slices;
    vkres<VkBuffer> light_cluster_ranges;
    vkres<VkBuffer> decal_cluster_slices;
    vkres<VkBuffer> decal_cluster_ranges;
    vkres<VkBuffer> sort_order;
    // Current and previous.
    vkres<VkBuffer> point_light_sort_indices[2];
    vkres<VkBuffer> sorted_point_lights;
    vkres<VkBuffer> sorted_decals;

    compute_pipeline light_morton_pipeline;
    compute_pipeline light_range_pipeline;
    compute_pipeline clustering_pipeline;
    compute_pipeline hierarchy_pipeline;
    compute_pipeline decal_order_pipeline;
    compute_pipeline decal_range_pipeline;

    descriptor_set clustering_data_set;
};

}

#endif

