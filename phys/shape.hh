#ifndef RAYBASE_PHYS_SHAPE_HH
#define RAYBASE_PHYS_SHAPE_HH
#include "core/ecs.hh"
#include "core/types.hh"
#include "core/math.hh"
#include "core/thread_pool.hh"
#include "core/resource_store.hh"
#include "context.hh"

class btCollisionShape;
class btTransform;

namespace rb::phys
{

class shape
{
public:
    struct params
    {
        float margin = 0.04f;
        vec3 scaling = vec3(1.0f);
        vec3 origin = vec3(0.0f);
        quat orientation = quat(1,0,0,0);
    };

    struct box
    {
        box() = default;
        box(const aabb& bounding_box);

        // "Radius" of the box.
        vec3 half_extent = vec3(1);
    };

    struct sphere
    {
        sphere() = default;
        sphere(const aabb& bounding_box);
        float radius = 1.0f;
    };

    struct capsule
    {
        capsule() = default;
        capsule(const aabb& bounding_box);
        float radius = 1.0f;
        float height = 2.0f;
    };

    struct cylinder
    {
        cylinder() = default;
        cylinder(const aabb& bounding_box);
        vec3 half_extent = vec3(1);
    };

    struct cone
    {
        cone() = default;
        cone(const aabb& bounding_box);
        float radius = 1.0f;
        float height = 1.0f;
    };

    struct convex_hull
    {
        std::vector<vec3> points;
    };

    // Extremely slow! Prefer convex_hull instead!
    struct dynamic_mesh
    {
        std::vector<uint32_t> indices;
        std::vector<vec3> points;
    };

    struct static_mesh
    {
        std::vector<uint32_t> indices;
        std::vector<vec3> points;
    };

    shape(context& ctx, box&& c, const params& p);
    shape(context& ctx, sphere&& c, const params& p);
    shape(context& ctx, capsule&& c, const params& p);
    shape(context& ctx, cylinder&& c, const params& p);
    shape(context& ctx, cone&& c, const params& p);
    shape(context& ctx, convex_hull&& c, const params& p);
    shape(context& ctx, dynamic_mesh&& c, const params& p);
    shape(context& ctx, static_mesh&& c, const params& p);
    // This is the instantiation constructor: it creates a shape based on the
    // other one with different transform.
    shape(shape& other, const params& p);
    ~shape();

    bool is_loaded() const;
    void wait() const;

    // After calling reset() on a convex hull, the colliders using this
    // shape must be refreshed by calling physics_scene::refresh().
    btCollisionShape* get_bt_shape() const;
    void reset(const params& p);

    // Can be dangerous, calls reset().
    void apply_scale(vec3 scaling);
    vec3 get_scaling() const;
    void get_bt_offset(btTransform& t) const;
    mat4 get_offset() const;

private:
    static void set_shape_params(void* shape, const params& p);

    struct impl_data;
    std::unique_ptr<impl_data> data;
    mutable thread_pool::ticket loading_ticket;

    vec3 scaling;
    vec3 origin;
    quat orientation;
};

}

#endif

