#include "path_tracer.hh"
#include "clustering_stage.hh"
#include "rt_generic.rchit.h"
#include "rt_generic.rahit.h"
#include "rt_generic.rmiss.h"
#include "rt_generic_point_light.rchit.h"
#include "rt_generic_point_light.rahit.h"
#include "rt_generic_point_light.rint.h"

namespace
{
using namespace rb;

vec4 get_light_type_probabilities(gfx::scene_stage* stage)
{
    const float point_weight = stage->get_active_point_light_count() > 0 ? 1.0f : 0.0f;
    const float dir_weight = stage->get_active_directional_light_count() > 0 ? 1.0f : 0.0f;
    const float tri_light_weight = stage->get_active_tri_light_count() > 0 ? 1.0f : 0.0f;
    const float envmap_weight = stage->has_active_envmap() ? 1.0f : 0.0f;

    vec4 prob = vec4(
        point_weight, dir_weight, tri_light_weight, envmap_weight
    );

    float sum = prob.x + prob.y + prob.z + prob.w;
    prob *= sum <= 0.0f ? 0.0f : (1.0f/sum + 1e-5f);
    return prob;
}

}

namespace rb::gfx
{

path_tracer::path_tracer(
    clustering_stage& clustering
):  cluster_data(&clustering),
    pipeline(clustering.get_device()),
    set(clustering.get_device()),
    envmap_alias_table(VK_NULL_HANDLE),
    emat_gen(clustering.get_device())
{
}

void path_tracer::init(
    argvec<uint32_t> rgen_spirv,
    size_t push_constant_buffer_size,
    VkDescriptorSetLayout layout,
    const options& opt
){
    this->opt = opt;
    specialization_info specialization;
    cluster_data->get_specialization_info(specialization);
    specialization[10] = (int)opt.filter.type;
    specialization[11] = (int)opt.ap.shape;
    specialization[12] =
        (opt.opaque_only ? 0 : (1<<0)) |
        (opt.nee_samples_all_lights ? (1<<1) : 0) |
        (opt.light_sampling_blue_noise ? (1<<3) : 0) |
        (opt.show_lights_directly ? (1<<4) : 0) |
        (opt.path_space_regularization ? (1<<5) : 0) |
        (opt.path_space_regularization && opt.path_space_regularization_nee_only ? (1<<6) : 0) |
        (opt.clamping ? (1<<7) : 0) |
        (opt.cull_back_faces ? (1<<8) : 0);
    specialization[13] = (int)opt.decals;
    specialization[14] = (int)opt.max_bounces;

    uint32_t direct_ray_mask = 0xFF;
    uint32_t indirect_ray_mask = 0xFF;

    if(!opt.show_lights_directly)
        direct_ray_mask ^= TLAS_LIGHT_MASK;

    if(opt.static_only)
    {
        direct_ray_mask ^= TLAS_DYNAMIC_MESH_MASK;
        indirect_ray_mask ^= TLAS_DYNAMIC_MESH_MASK;
    }

    specialization[15] = (int)direct_ray_mask;
    specialization[16] = (int)indirect_ray_mask;

    ray_tracing_shader_data shader;
    shader.generation.data = rgen_spirv.data();
    shader.generation.bytes = rgen_spirv.size() * sizeof(uint32_t);
    shader.generation.specialization = specialization;
    shader.miss = rt_generic_rmiss_shader_binary;
    shader.miss.specialization = specialization;
    shader.max_recursion = 1;
    // Triangles
    shader.hit_groups.emplace_back(hit_group{
        VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        {rt_generic_rchit_shader_binary, specialization},
        (opt.opaque_only ? shader_data{} : shader_data{rt_generic_rahit_shader_binary, specialization}),
        {}
    });
    // Point lights
    shader.hit_groups.emplace_back(hit_group{
        VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
        {rt_generic_point_light_rchit_shader_binary, specialization},
        {rt_generic_point_light_rahit_shader_binary, specialization},
        {rt_generic_point_light_rint_shader_binary, specialization}
    });
    set.add(shader, 2);
    set.set_binding_params("envmap_alias_table", 1, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
    set.reset(1);

    pipeline.init(
        shader, push_constant_buffer_size,
        {layout, cluster_data->get_scene_data()->get_descriptor_set().get_layout(), set.get_layout()}
    );
}

gpu_path_tracer_config path_tracer::bind(VkCommandBuffer cmd)
{
    gpu_path_tracer_config  config;

    bool refresh_descriptor_sets = false;

    scene_stage* scene_data = cluster_data->get_scene_data();

    config.light_type_sampling_probabilities = get_light_type_probabilities(scene_data);
    config.min_ray_dist = opt.min_ray_dist;
    config.max_ray_dist = opt.max_ray_dist;
    config.film_parameter = opt.filter.radius;

    scene* s = scene_data->get_scene();
    entity id = find_sky_envmap(*s);
    config.regularization_gamma = opt.path_space_regularization_gamma;
    config.clamping_threshold = opt.clamping_threshold;

    if(id != INVALID_ENTITY)
    {
        environment_map* em = s->get<environment_map>(id);
        environment_map_alias_table* alias_table = s->get<environment_map_alias_table>(id);
        if(!alias_table)
        {
            s->attach(id, environment_map_alias_table{});
            alias_table = s->get<environment_map_alias_table>(id);
        }
        if(!alias_table->alias_table)
            emat_gen.generate(*alias_table, *em->cubemap);

        if(alias_table->alias_table != envmap_alias_table)
        {
            envmap_alias_table = alias_table->alias_table;
            refresh_descriptor_sets = true;
        }
    }

    if(refresh_descriptor_sets)
    {
        set.reset(1);
        set.set_buffer(0, "envmap_alias_table", envmap_alias_table);
    }

    pipeline.bind(cmd);
    pipeline.set_descriptors(cmd, scene_data->get_descriptor_set(), 0, 1);
    pipeline.set_descriptors(cmd, set, 0, 2);
    return config;
}

}
