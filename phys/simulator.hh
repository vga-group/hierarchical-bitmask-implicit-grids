#ifndef RAYBASE_PHYS_SIMULATOR_HH
#define RAYBASE_PHYS_SIMULATOR_HH
#include "core/ecs.hh"
#include "core/types.hh"
#include "core/math.hh"
#include "context.hh"
#include "collision.hh"
#include "collider.hh"
#include "constraint.hh"
#include <optional>

class btCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btDiscreteDynamicsWorld;
class btConstraintSolver;
class btConstraintSolverPoolMt;
class btIDebugDraw;

namespace rb
{
    class transformable;
}

namespace rb::phys
{

struct simulator_internal;

// This system does physics simulation. See 'collision_system' if you only want
// collision detection with no physics.
// Note: you should only have one simulator/collision_system per scene!
// Multiple simulators will break things, as they step on each others toes!
class simulator:
    public receiver<
        add_component<collider>,
        remove_component<collider>,
        add_component<constraint>,
        remove_component<constraint>,
        add_component<skeleton_collider>,
        remove_component<skeleton_collider>
    >
{
public:
    struct options
    {
        // 0   - No interpolation, time step bound to delta time.
        // > 0 - Interpolation, updates occur according to time step.
        time_ticks time_step = 16666;
        bool multithreaded = true;
    };

    simulator(context& ctx, scene& s, options opt);
    ~simulator();

    void set_gravity(vec3 g);
    vec3 get_gravity() const;

    void set_time_step(time_ticks time_step);
    time_ticks get_time_step() const;

    // Synchronous update
    void update(time_ticks delta_time);

    // Asynchronous update. Make 100% certain that the shapes referred by
    // colliders aren't deleted while this runs! If you get weird "pure virtual
    // function call" crashes, it's probably because of that!
    void start_update(time_ticks delta_time, uint32_t priority = 1);
    void finish_update();

    // Ray casting
    std::optional<intersection> cast_ray_closest_hit(
        const ray& r,
        float max_distance
    );

    std::vector<intersection> cast_ray_all_hits(
        const ray& r,
        float max_distance
    );

    // Don't call these directly yourself, they're called automatically by
    // phys_debug_draw_stage.
    void set_debug_drawer(btIDebugDraw* drawer);
    void debug_draw();

protected:
    void handle(scene& s, const add_component<collider>& event) override;
    void handle(scene& s, const remove_component<collider>& event) override;
    void handle(scene& s, const add_component<constraint>& event) override;
    void handle(scene& s, const remove_component<constraint>& event) override;
    void handle(scene& s, const add_component<skeleton_collider>& event) override;
    void handle(scene& s, const remove_component<skeleton_collider>& event) override;

private:
    void pre_update_impl();
    void post_update_impl();
    void step_simulation(time_ticks delta_time);
    void scene_state_to_world();
    void world_to_scene_state();

    void add_collider(
        entity id,
        unsigned subindex,
        transformable* base_transform,
        mat4* local_transform,
        collider::impl_data* impl
    );
    void refresh_collider(
        transformable* base_transform,
        mat4* local_transform,
        collider& c
    );

    thread_pool* pool;
    scene* s;
    options opt;
    thread_pool::ticket pending_ticket;
    bool pending_async_update;
    mutable std::recursive_mutex async_mutex;
    std::unique_ptr<btCollisionConfiguration> collision_configuration;
    std::unique_ptr<btCollisionDispatcher> dispatcher;
    std::unique_ptr<btBroadphaseInterface> pair_cache;
    std::unique_ptr<btConstraintSolver> constraint_solver;
    std::unique_ptr<btConstraintSolverPoolMt> constraint_solver_pool;
    std::unique_ptr<btDiscreteDynamicsWorld> world;
};

}

#endif
