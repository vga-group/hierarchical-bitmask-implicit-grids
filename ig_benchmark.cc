#include "core/core.hh"
#include "gfx/gfx.hh"
#include "phys/phys.hh"
#include "extra/gltf.hh"
#include "extra/game_object.hh"
#include <cstdio>
#include <chrono>
#define MULTIVIEW_GRID_W 16
#define MULTIVIEW_GRID_H 8
#define MULTIVIEW_CAMERA_OFFSET 0.1f
#define MULTIVIEW_VIEW_COUNT (MULTIVIEW_GRID_W * MULTIVIEW_GRID_H)
#define MULTIVIEW_SIZE rb::ivec2(256, 256)
#define AVG_OVER_FRAMES 100
#define SKIP_FRAMES 20

namespace rg = rb::gfx;
namespace rp = rb::phys;
namespace re = rb::extra;

uint32_t ITEM_COUNT = 0;
const uint32_t STEP_SIZE = 128;
const uint32_t MAX_ITEM_COUNT = 65536;
bool testing_lights = true;

RB_CVAR_ENUM(
    rg::path_tracer::decal_mode,
    NEVER,
    PRIMARY_ONLY,
    ALWAYS
);

struct avg_timer
{
    int count;
    std::vector<std::pair<std::string, double>> sum;

    bool add(const std::vector<std::pair<std::string, double>>& t)
    {
        if(count == SKIP_FRAMES + AVG_OVER_FRAMES) return true;

        if(count >= SKIP_FRAMES)
        {
            for(auto&[name, time]: t)
            {
                bool found = false;
                for(auto&[sname, stime]: sum)
                {
                    if(sname == name)
                    {
                        stime += time;
                        found = true;
                        break;
                    }
                }
                if(!found)
                {
                    sum.push_back({name, time});
                }
            }
        }
        count++;
        return false;
    }

    void finish()
    {
        count -= SKIP_FRAMES;
        std::cout << ITEM_COUNT << std::endl;
        for(auto& pair: sum)
        {
            std::cout << pair.first << ": " << pair.second*1e3 / count << std::endl;
        }
        sum.clear();
        count = 0;
    }
};

class mv_benchmark_renderer: public rg::render_pipeline
{
public:
    mv_benchmark_renderer(rg::video_output& output)
    : rg::render_pipeline(output), scene(nullptr)
    {
    }

    void set_scene(rb::scene* scene)
    {
        this->scene = scene;
        if(scene_data) scene_data->set_scene(scene);
    }

protected:
    rg::render_target reset() override
    {
        color.emplace(rg::texture::create_framebuffer(
            output->get_device(),
            MULTIVIEW_SIZE,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            false,
            MULTIVIEW_VIEW_COUNT
        ));
        depth.emplace(rg::texture::create_framebuffer(
            output->get_device(),
            MULTIVIEW_SIZE,
            VK_FORMAT_D32_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            false,
            MULTIVIEW_VIEW_COUNT
        ));
        layout_color.emplace(rg::texture::create_framebuffer(
            output->get_device(),
            output->get_size(),
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT
        ));
        output_color.emplace(rg::texture::create_framebuffer(
            output->get_device(),
            output->get_size(),
            VK_FORMAT_R8G8B8A8_UNORM
        ));
        rg::render_target color_target = color->get_render_target();
        rg::render_target depth_target = depth->get_render_target();
        rg::render_target layout_color_target = layout_color->get_render_target();
        rg::render_target output_color_target = output_color->get_render_target();

        rg::scene_stage::options scene_options;
        scene_options.max_lights = rb::max(rb::align_up_to(testing_lights ? ITEM_COUNT : 1, 128), 128u);
        scene_options.max_decals = rb::max(rb::align_up_to(testing_lights ? 0 : ITEM_COUNT, 128), 128u);
        scene_options.ray_tracing = true;
        scene_options.material_features =
            rg::MATERIAL_FEATURE_ALBEDO
            |rg::MATERIAL_FEATURE_METALLIC_ROUGHNESS
            //|rg::MATERIAL_FEATURE_EMISSION
            //|rg::MATERIAL_FEATURE_TRANSMISSION
            |rg::MATERIAL_FEATURE_NORMAL_MAP;

        scene_data.emplace(output->get_device(), scene_options);
        if(scene) scene_data->set_scene(scene);

        rg::clustering_stage::options cl;
        RB_CVAR(cl.light_cluster_resolution, 512);
        RB_CVAR(cl.decal_cluster_resolution, 512);
        clustering.emplace(*scene_data, cl);

        rg::multiview_forward_stage::options fw_options;
        fw_options.z_pre_pass = true;
        fw_options.dynamic_lighting = true;
        forward.emplace(*clustering, color_target, depth_target, fw_options);

        layout.emplace(
            output->get_device(), color_target, layout_color_target,
            rg::multiview_layout_stage::grid{0, rb::vec4(0), true}
            //rg::multiview_layout_stage::blend{}
        );

        rg::tonemap_stage::options tonemap_options;
        tonemap_options.op = rg::tonemap_operator::REINHARD;
        tonemap.emplace(
            output->get_device(),
            layout_color_target, output_color_target,
            tonemap_options
        );

        scene->foreach([&](rg::camera& cam){cam.set_aspect(color->get_aspect());});

        return output_color_target;
    }

    rg::event render_stages(uint32_t frame_index, rg::event frame_start) override
    {
        rg::event scene_event = scene_data->run(frame_index, frame_start);
        rg::event cluster_event = clustering->run(frame_index, scene_event);
        rg::event render_event = forward->run(frame_index, cluster_event);
        rg::event layout_event = layout->run(frame_index, render_event);
        rg::event tonemap_event = tonemap->run(frame_index, layout_event);
        return tonemap_event;
    }

private:
    rb::scene* scene;
    std::optional<rg::texture> color;
    std::optional<rg::texture> depth;
    std::optional<rg::texture> layout_color;
    std::optional<rg::texture> output_color;
    std::optional<rg::scene_stage> scene_data;
    std::optional<rg::clustering_stage> clustering;
    std::optional<rg::multiview_forward_stage> forward;
    std::optional<rg::multiview_layout_stage> layout;
    std::optional<rg::tonemap_stage> tonemap;
};

class rt_benchmark_renderer: public rg::render_pipeline
{
public:
    rt_benchmark_renderer(rg::video_output& output)
    : rg::render_pipeline(output), scene(nullptr)
    {
    }

    void reset_accumulation()
    {
        if(path_tracer) path_tracer->reset_accumulation();
    }

    void set_scene(rb::scene* scene)
    {
        this->scene = scene;
        if(scene_data) scene_data->set_scene(scene);
    }

protected:
    rg::render_target reset() override
    {
        accumulation_color.emplace(rg::texture::create_framebuffer(
            output->get_device(),
            output->get_size(),
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_LAYOUT_GENERAL
        ));
        output_color.emplace(rg::texture::create_framebuffer(
            output->get_device(),
            output->get_size(),
            VK_FORMAT_R8G8B8A8_UNORM
        ));
        rg::render_target accumulation_color_target = accumulation_color->get_render_target();
        rg::render_target output_color_target = output_color->get_render_target();

        rg::scene_stage::options scene_options;
        scene_options.max_lights = rb::max(rb::align_up_to(testing_lights ? ITEM_COUNT : 1, 128), 128u);
        scene_options.max_decals = rb::max(rb::align_up_to(testing_lights ? 0 : ITEM_COUNT, 128), 128u);
        scene_options.ray_tracing = true;
        scene_options.as_manager_opt.lazy_removal = false;
        scene_options.material_features =
            rg::MATERIAL_FEATURE_ALBEDO
            |rg::MATERIAL_FEATURE_METALLIC_ROUGHNESS
            //|rg::MATERIAL_FEATURE_EMISSION
            //|rg::MATERIAL_FEATURE_TRANSMISSION
            |rg::MATERIAL_FEATURE_NORMAL_MAP;

        scene_data.emplace(output->get_device(), scene_options);
        if(scene) scene_data->set_scene(scene);

        rg::clustering_stage::options cl;
        RB_CVAR(cl.light_cluster_resolution, 512);
        RB_CVAR(cl.decal_cluster_resolution, 512);
        clustering.emplace(*scene_data, cl);

        rg::path_tracer_stage::options pt;
        pt.filter.type = rg::film_filter::GAUSSIAN;
        pt.filter.sigma = 0.4f;
        pt.ap.shape = 6;
        pt.max_bounces = testing_lights ? 1 : 2;
        pt.decals = testing_lights ? rg::path_tracer::decal_mode::NEVER : rg::path_tracer::decal_mode::ALWAYS;
        RB_CVAR(pt.show_lights_directly, true);
        RB_CVAR(pt.accumulate, true);
        pt.nee_samples_all_lights = testing_lights ? true : false;
        RB_CVAR(pt.path_space_regularization, true);
        RB_CVAR(pt.path_space_regularization_nee_only, false);
        RB_CVAR(pt.path_space_regularization_gamma, 0.5f);
        RB_CVAR(pt.clamping, false);
        path_tracer.emplace(*clustering, accumulation_color_target, pt);

        rg::tonemap_stage::options tonemap_options;
        tonemap_options.op = rg::tonemap_operator::REINHARD;
        tonemap.emplace(
            output->get_device(),
            accumulation_color_target, output_color_target,
            tonemap_options
        );

        scene->foreach([&](rg::camera& cam){cam.set_aspect(output->get_aspect());});

        return output_color_target;
    }

    rg::event render_stages(uint32_t frame_index, rg::event frame_start) override
    {
        rg::event scene_event = scene_data->run(frame_index, frame_start);
        rg::event cluster_event = clustering->run(frame_index, scene_event);
        rg::event render_event = path_tracer->run(frame_index, cluster_event);
        rg::event tonemap_event = tonemap->run(frame_index, render_event);
        return tonemap_event;
    }

private:
    rb::scene* scene;
    std::optional<rg::texture> accumulation_color;
    std::optional<rg::texture> output_color;
    std::optional<rg::scene_stage> scene_data;
    std::optional<rg::clustering_stage> clustering;
    std::optional<rg::path_tracer_stage> path_tracer;
    std::optional<rg::tonemap_stage> tonemap;
};

rb::aabb scene_aabb = {rb::vec3(-20, -2, -10), rb::vec3(20, 12, 10)};

re::game_object setup_base_test(rg::window& win, rb::scene& scene, re::gltf_data& sponza)
{
    re::game_object camera = re::add_game_object(
        scene,
        rg::camera(),
        rb::transformable(),
        rg::rendered()
    );
    camera.camera->perspective(90, win.get_aspect(), 0.01f, 100.0f);
    camera.transform->translate(rb::vec3(0, 6, -5));

    sponza.add(scene);
    return camera;
}

void setup_light_test(rb::scene& scene)
{
    scene([&](rb::entity id, rg::point_light* pl, rg::decal* d){
        scene.remove(id);
    });

    // Flood scene with point lights
    srand(0);
    for(int i = 0; i < ITEM_COUNT; ++i)
    {
        rb::vec3 pos = rb::linearRand(scene_aabb.min, scene_aabb.max);
        scene.add(
            rg::point_light{normalize(rb::linearRand(rb::vec3(0), rb::vec3(1.0)))*0.1f, 0.0f, 0.1f},
            rb::transformable(pos),
            rg::rendered()
        );
    }
}

void setup_plain(rb::scene& scene)
{
    scene([&](rb::entity id, rg::point_light* pl, rg::decal* d){
        scene.remove(id);
    });

    scene.add(
        rg::point_light{rb::vec3(100), 0.0f, 1e-9f},
        rg::ambient_light{rb::vec3(0.1)},
        rb::transformable(rb::vec3(0,5,0)),
        rg::rendered()
    );
}

void setup_decal_test(rg::texture& decal_tex, rb::scene& scene, rp::collision_system& col)
{
    scene([&](rb::entity id, rg::point_light* pl, rg::decal* d){
        scene.remove(id);
    });

    rg::material decal_material;
    decal_material.color_texture.second = &decal_tex;
    decal_material.metallic = 0.0f;
    decal_material.roughness = 0.2f;

    srand(0);
    for(int i = 0; i < ITEM_COUNT;)
    {
        rb::ray r;
        r.o = rb::linearRand(scene_aabb.min, scene_aabb.max);
        r.dir = rb::sphericalRand(1.0f);

        if(auto intersection = col.cast_ray_closest_hit(r))
        {
            decal_material.color = rb::vec4(normalize(rb::linearRand(rb::vec3(0), rb::vec3(1.0))), 1.0f);
            scene.add(
                rg::decal{decal_material, 0},
                rb::transformable(
                    intersection->pos,
                    rb::vec3(1.5,1,0.2)*0.3f,
                    -intersection->normal,
                    rb::create_tangent(intersection->normal)
                ),
                rg::rendered()
            );
            ++i;
        }
    }

    scene.add(
        rg::point_light{rb::vec3(200), 0.0f, 1e-9f},
        rg::ambient_light{rb::vec3(0.1)},
        rb::transformable(rb::vec3(0,5.0,0)),
        rg::rendered()
    );
}

int main()
{
    rb::thread_pool thread_pool;
    rg::context gfx_ctx(&thread_pool);
    rp::context phys_ctx(&thread_pool);

    rb::scene scene;
    rp::collision_system col(phys_ctx, scene, {});

    rg::window win(gfx_ctx, "Implicit grid benchmark", rb::ivec2(1920, 1080), false);
    win.set_mouse_grab(false);
    win.set_vsync(false);

    rb::native_filesystem fs("data");
    rb::resource_store store("", {&fs});
    re::gltf_data& sponza = store.get<re::gltf_data>("sponza.glb", win.get_device(), &phys_ctx, re::gltf_data::options{true, true});
    rg::texture& decal_tex = store.get<rg::texture>("splat02.png", win.get_device());

    std::optional<rt_benchmark_renderer> rt_ren;
    std::optional<mv_benchmark_renderer> mv_ren;

    // Setup scene
    testing_lights = true;
    re::game_object camera = setup_base_test(win, scene, sponza);
    setup_light_test(scene);

    rt_ren.emplace(win);
    rt_ren->set_scene(&scene);
    rg::render_pipeline* cur_ren = &rt_ren.value();

    // Done, for now.
    RB_LOG("Loaded up!");

    bool quit = false;
    rb::time_ticks time = 0;
    float pitch = -15;
    float yaw = 180;
    float speed = 1;

    camera.transform->set_orientation(pitch, yaw);

    rb::begin_stdin_cmdline();
    avg_timer avg;
    bool timing = false;

    while(!quit)
    {
        if(rb::poll_input(
            [&](const SDL_Event& e){
                if(e.type == SDL_KEYDOWN)
                {
                    if(e.key.keysym.sym == SDLK_t)
                    {
                        timing = true;
                        avg.count = 0;
                        avg.sum.clear();
                        RB_LOG("Started timing");
                    }
                    if(e.key.keysym.sym == SDLK_r)
                    {
                        if(rt_ren)
                        { // Enter multi-view test
                            scene.remove<rg::rendered>(camera.id);
                            for(int y = 0; y < MULTIVIEW_GRID_W; ++y)
                            for(int x = 0; x < MULTIVIEW_GRID_W; ++x)
                            {
                                rb::transformable t(camera.transform);
                                t.translate(
                                    rb::vec3(
                                        x-MULTIVIEW_GRID_W*0.5f,
                                        -y+MULTIVIEW_GRID_H*0.5f,
                                        0
                                    ) * MULTIVIEW_CAMERA_OFFSET
                                );
                                scene.add(
                                    std::move(t),
                                    rg::rendered(),
                                    rg::camera(*camera.camera),
                                    rg::camera_order{uint32_t(x + y * MULTIVIEW_GRID_W)}
                                );
                            }
                            rt_ren.reset();
                            mv_ren.emplace(win);
                            mv_ren->set_scene(&scene);
                            cur_ren = &mv_ren.value();
                        }
                        else
                        { // Enter RT test
                            scene([&](rb::entity id, rg::camera& cam, rg::camera_order& order){
                                scene.remove(id);
                            });
                            scene.attach(camera.id, rg::rendered());
                            mv_ren.reset();
                            rt_ren.emplace(win);
                            rt_ren->set_scene(&scene);
                            cur_ren = &rt_ren.value();
                        }
                        timing = false;
                    }
                    if(e.key.keysym.sym == SDLK_f)
                    {
                        testing_lights = !testing_lights;
                        if(testing_lights) setup_light_test(scene);
                        else setup_decal_test(decal_tex, scene, col);
                        cur_ren->reinit();
                        timing = false;
                    }
                    if(e.key.keysym.sym == SDLK_F1)
                        win.set_mouse_grab(!win.is_mouse_grabbed());
                }
                return false;
            }
        )) break;

        if(timing)
        {
            if(avg.add(win.get_device().get_timing_results()))
            {
                avg.finish();

                ITEM_COUNT += STEP_SIZE;

                if(testing_lights) setup_light_test(scene);
                else setup_decal_test(decal_tex, scene, col);
                if(rt_ren) rt_ren->reset_accumulation();

                cur_ren->reinit();

                if(ITEM_COUNT > MAX_ITEM_COUNT)
                {
                    timing = false;
                    ITEM_COUNT = 65536;
                }
            }
        }

        if(rb::try_stdin_cmdline())
            cur_ren->reinit();

        col.run();

        cur_ren->render();
    }

    return 0;
}
