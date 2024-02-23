#ifndef RAYBASE_PHYS_SKELETON_COLLIDER_HH
#define RAYBASE_PHYS_SKELETON_COLLIDER_HH
#include "collider.hh"
#include "constraint.hh"

namespace rb
{
class skeleton;
}

namespace rb::phys
{

struct joint_collider
{
    // If you don't actually want the joint to have a collider, you can leave
    // it as the default, as the default collider simply does nothing.
    collider col = collider();
    // You don't have to fill this one in, it's updated automatically.
    vec3 init_scaling = vec3(1);
};

// You can add colliders and constraints to skeletal nodes using this class.
// Skeleton nodes aren't directly in the ECS (they're through the skeleton
// component), so to attach colliders to those, we need this class.
//
// It also abuses the constraints a bit; instead of ECS entities, the collider
// 'entity' indices refer to joint collider indices in this class instead.
struct skeleton_collider
{
    skeleton_collider() = default;
    skeleton_collider(const skeleton_collider& other) = default;
    skeleton_collider(skeleton_collider&& other) noexcept = default;
    skeleton_collider& operator=(const skeleton_collider& other) = default;
    skeleton_collider& operator=(skeleton_collider&& other) noexcept = default;

    // You must have one joint_collider per true joint of the skeleton. Note
    // that you can default-construct the collider in order to have it not
    // actually exist; you don't have to have an actual, valid collider for
    // every joint.
    //
    // Also, don't touch this vector after you've added the this component to
    // the ECS.
    std::vector<joint_collider> joints;

    // Note: the constraint collider entity IDs must refer to the 'joints'
    // vector indices instead of ECS entities!
    std::vector<constraint> constraints;

    void auto_joint_constraints(skeleton* skel);

    bool is_empty() const;

    // Can be dangerous, changes the collision shapes themselves so changes
    // affect all other colliders using those shapes as well.
    // Yet, it's called automatically by collision_system and simulator.
    // Yee haw!
    void apply_scale(vec3 scale);
    vec3 get_current_scale() const;

    // Call this only after you've manually constructed the joints. You don't
    // need it if you're not modifying the joints with get_joint_collider or you
    // are using the default gltf importer.
    void init_joint_collider_scalings();

    void set_category_flags(uint32_t category_flags);
    void set_category_mask(uint32_t category_mask);

    void make_static();
    void make_dynamic();
    void make_kinematic();

private:
    vec3 current_scale = vec3(1);
};

}

#endif
