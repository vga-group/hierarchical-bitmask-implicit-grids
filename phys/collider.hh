#ifndef RAYBASE_PHYS_COLLIDER_HH
#define RAYBASE_PHYS_COLLIDER_HH
#include "core/ecs.hh"
#include "core/transformable.hh"
#include "core/types.hh"
#include "core/math.hh"
#include "context.hh"

namespace rb::phys
{

class shape;

class collider
{
public:
    // Collider categories; these can be combined. Some of them have semantic
    // meaning; STATIC and KINEMATIC affect how the collider responds to the
    // linked transformable_node:
    //  STATIC = all transforms ignored after adding
    //  KINEMATIC = all transforms (except scaling!) inherited
    //  (neither) = physics simulation overrides transform
    // You can add your own as well, with values of 1<<n, where n >= 7.
    static constexpr uint32_t NONE = 0;
    static constexpr uint32_t DYNAMIC = 1<<0;
    static constexpr uint32_t STATIC = 1<<1;
    static constexpr uint32_t KINEMATIC = 1<<2;
    static constexpr uint32_t DEBRIS = 1<<3;
    static constexpr uint32_t SENSOR = 1<<4;
    static constexpr uint32_t CHARACTER = 1<<5;
    static constexpr uint32_t PROJECTILE = 1<<6;
    static constexpr uint32_t ALL = 0xFFFFFFFF;

    collider();
    collider(
        shape* s,
        uint32_t category_flags = DYNAMIC,
        uint32_t category_mask = ALL,
        float mass = 0.0f
    );
    collider(const collider& other);
    collider(collider&& other) noexcept;
    ~collider();

    // mass = 0 makes the collider only participate in collision checks,
    // disabling physics for the collider.
    void set_mass(float mass = 0.0f);
    float get_mass() const;

    void set_friction(float friction = 0.2f);
    float get_friction() const;

    void set_restitution(float restitution = 0.0f);
    float get_restitution() const;

    void set_linear_damping(float linear_damping = 0.0f);
    float get_linear_damping() const;

    void set_angular_damping(float angular_damping = 0.0f);
    float get_angular_damping() const;

    void set_category_flags(uint32_t category_flags = DYNAMIC);
    uint32_t get_category_flags() const;

    // Easier shortcuts for setting specific category flags without touching
    // the others.
    void make_static();
    void make_dynamic();
    void make_kinematic();

    void set_category_mask(uint32_t category_mask = ALL);
    uint32_t get_category_mask() const;

    void set_shape(shape* s);
    shape* get_shape() const;

    void set_use_deactivation(bool use_deactivation = true);
    bool get_use_deactivation() const;

    void impulse(vec3 impulse);

    void set_velocity(vec3 velocity);
    vec3 get_velocity() const;

    collider& operator=(const collider& other);
    collider& operator=(collider&& other) noexcept;

    bool valid() const;

    // This one is _very_ dangerous, since it applies to the collision shape
    // itself. Every instance of that collision shape becomes scaled. It is
    // provided for convenience.
    void apply_scale(const transformable* t);
    void apply_scale(vec3 scaling);

    // Don't use this manually.
    struct impl_data;
    impl_data* get_internal() const;

private:
    impl_data& safe_get_impl(bool need_sync = true) const;
    void update_activation_state();

    mutable std::unique_ptr<impl_data> impl;
};

}

#endif
