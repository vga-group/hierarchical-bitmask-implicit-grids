#ifndef RAYBASE_SKELETON_HH
#define RAYBASE_SKELETON_HH
#include "animation.hh"
#include "transformable.hh"
#include <vector>
#include <variant>

namespace rb
{

class skeleton
{
public:
    struct joint
    {
        std::string name;
        transformable node;
        mat4 inverse_bind_matrix = mat4(1.0f);
        const rigid_animation_pool* pool = nullptr;
        const rigid_animation* cur_anim = nullptr;
        bool root = true;

        enum constraint_type
        {
            NONE = 0,
            FIXED = 1,
            POINT = 2,
            CONE_TWIST = 3
        };

        // You can constrain the allowed movement of a joint by setting this
        // constraint! It won't affect manual joint movement or animations,
        // though. But ragdolls are affected!
        struct
        {
            constraint_type type;
            union
            {
                struct
                {
                    float swing_span1;
                    float swing_span2;
                    float twist_span;
                } cone_twist;
            };
        } constraint = {NONE, {}};
        mutable uint16_t cached_revision = 0;
    };

    enum skinning_mode
    {
        LINEAR = 0, // Works with scaling, most robust
        DUAL_QUATERNION = 1 // Faster, preserves volume, breaks with scaling
    };

    skeleton(
        size_t true_joint_count,
        size_t false_joint_count,
        skinning_mode mode = LINEAR
    );
    skeleton(const skeleton& other);
    skeleton(skeleton&& other) noexcept;

    time_ticks set_animation(const std::string& name);
    void apply_animation(time_ticks time);

    skeleton& operator=(skeleton&& other) noexcept;
    skeleton& operator=(const skeleton& other);

    skinning_mode get_skinning_mode() const;
    void set_skinning_mode(skinning_mode mode);

    size_t get_joint_count() const;
    size_t get_true_joint_count() const;
    joint& get_joint(size_t i);
    const joint& get_joint(size_t i) const;
    joint& get_joint(const std::string& name);
    const joint& get_joint(const std::string& name) const;

    // -1 if not a joint in this skeleton.
    int get_joint_index(transformable* n) const;

    // If the parent of 'node' is a joint in the given skeleton, it is replaced
    // by the corresponding joint in this skeleton.
    void reparent(transformable& node, const skeleton& from);

    // Returns the skeleton to its default state, which is deduced from the
    // inverse bind matrices.
    void reset();

    void set_root_parent(transformable* parent, bool keep_transform = false);

    bool is_dirty() const;
    void* refresh_joint_transforms();

private:
    void copy_joint_hierarchy(const skeleton& from);

    size_t true_joint_count;
    std::vector<joint> joints;
    std::variant<std::vector<mat4>, std::vector<dualquat>> joint_transforms;
};

}

#endif
