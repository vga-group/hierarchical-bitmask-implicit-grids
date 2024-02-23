#ifndef RAYBASE_PHYS_CONSTRAINT_HH
#define RAYBASE_PHYS_CONSTRAINT_HH
#include "core/math.hh"
#include "core/ecs.hh"
#include <memory>

class btTypedConstraint;

namespace rb::phys
{

class collider;
class skeleton_collider;

// A constraint ties two colliders together. Constraints added to a physics
// scene are immutable, you will have to remove and add them back if you want
// changes to take place.
class constraint
{
public:
    enum type
    {
        FIXED = 0,
        POINT,
        HINGE,
        SLIDER,
        PISTON,
        GENERIC,
        GENERIC_SPRING,
        CONE_TWIST
    };

    struct frame
    {
        vec3 origin = vec3(0);
        quat orientation = quat(1,0,0,0);
    };

    // Not all of the parameters are meaningful with all constraint types.
    // Default values are stolen from Blender ;)
    struct params
    {
        bool disable_collisions = false;
        bvec3 use_limit_ang = bvec3(false);
        vec3 limit_ang_lower = vec3(-45);
        vec3 limit_ang_upper = vec3(-45);
        bvec3 use_limit_lin = bvec3(false);
        vec3 limit_lin_lower = vec3(-1);
        vec3 limit_lin_upper = vec3(1);
        vec3 spring_damping_ang = vec3(0.5);
        vec3 spring_damping = vec3(0.5);
        vec3 spring_stiffness_ang = vec3(10);
        vec3 spring_stiffness = vec3(10);
        bvec3 use_spring_ang= bvec3(false);
        bvec3 use_spring = bvec3(false);
        vec2 swing_span = vec2(45);
        float twist_span = 45;
        vec3 scaling = vec3(1);
    };

    constraint();
    constraint(
        type t,
        entity col_a, const frame& frame_a,
        entity col_b, const frame& frame_b,
        const params& p
    );
    constraint(constraint&& other) noexcept;
    constraint(const constraint& other);
    ~constraint();

    constraint& operator=(constraint&& other) noexcept;
    constraint& operator=(const constraint& other);

    void set_type(type t);
    type get_type() const;

    void set_collider(int index, entity col);
    void set_collider(int index, entity col, const frame& f);
    entity get_collider(int index)  const;

    void set_frame(int index, const frame& f);
    frame get_frame(int index);

    void set_params(const params& p);
    const params& get_params() const;

    void set_scaling(vec3 scale);

private:
    type t;
    entity colliders[2];
    frame frames[2];
    params p;

    bool create_bt_constraint_instance(scene& s);
    bool create_bt_constraint_instance(skeleton_collider& s);
    bool create_bt_constraint_instance(collider* a, collider* b);

    friend class simulator;
    bool needs_refresh;
    std::unique_ptr<btTypedConstraint> instance;
};

}

#endif
