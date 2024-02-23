#ifndef RAYBASE_PHYS_COLLISION_SYSTEM_HH
#define RAYBASE_PHYS_COLLISION_SYSTEM_HH
#include "core/ecs.hh"
#include "core/types.hh"
#include "core/math.hh"
#include "core/transformable.hh"
#include "context.hh"
#include "collision.hh"
#include "collider.hh"
#include <optional>
#include <memory>

class btCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btCollisionWorld;
class btIDebugDraw;

namespace rb::phys
{

class skeleton_collider;

struct collision_system_internal;

// This class (only) does collision-detection. See 'simulator' for the same
// thing with physics simulation as well.
// Note: you should only have one simulator/collision_system per scene!
// Using multiple will break things, as they step on each others toes!
class collision_system:
    public receiver<
        add_component<collider>,
        remove_component<collider>,
        add_component<skeleton_collider>,
        remove_component<skeleton_collider>
    >
{
public:
    struct options
    {
        bool multithreaded = true;
    };

    collision_system(context& ctx, scene& s, options opt);
    ~collision_system();

    // Sends 'collision' as events to the ECS and refreshes ray cast state
    void run();

    std::optional<intersection> cast_ray_closest_hit(
        const ray& r,
        float max_distance = 1000.0f
    ) const;

    std::vector<intersection> cast_ray_all_hits(
        const ray& r,
        float max_distance = 1000.0f
    ) const;

    void set_debug_drawer(btIDebugDraw* drawer);
    void debug_draw();

protected:
    void handle(scene& s, const add_component<collider>& event) override;
    void handle(scene& s, const remove_component<collider>& event) override;
    void handle(scene& s, const add_component<skeleton_collider>& event) override;
    void handle(scene& s, const remove_component<skeleton_collider>& event) override;

private:
    void add_collider(
        entity id,
        unsigned subindex,
        transformable* base_transform,
        mat4* local_transform,
        collider::impl_data* impl
    );
    void refresh_all();
    void refresh_collider(
        transformable* base_transform,
        mat4* local_transform,
        collider::impl_data* impl
    );

    scene* s;
    std::unique_ptr<btCollisionConfiguration> collision_configuration;
    std::unique_ptr<btCollisionDispatcher> dispatcher;
    std::unique_ptr<btBroadphaseInterface> pair_cache;
    std::unique_ptr<btCollisionWorld> world;
};

}

#endif

