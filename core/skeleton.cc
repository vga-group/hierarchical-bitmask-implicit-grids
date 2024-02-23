#include "skeleton.hh"
#include "error.hh"
#include <stdexcept>

namespace
{
using namespace rb;

size_t calc_buffer_size(size_t joint_count, skeleton::skinning_mode mode)
{
    size_t size = joint_count;
    if(mode == skeleton::DUAL_QUATERNION) size *= sizeof(dualquat);
    else size *= sizeof(mat4);
    return size;
}

}

namespace rb
{

skeleton::skeleton(
    size_t true_joint_count,
    size_t false_joint_count,
    skinning_mode mode
):  true_joint_count(true_joint_count),
    joints(true_joint_count + false_joint_count)
{
    set_skinning_mode(mode);
}

skeleton::skeleton(const skeleton& other)
:   true_joint_count(other.true_joint_count),
    joint_transforms(other.joint_transforms)
{
    copy_joint_hierarchy(other);
}

skeleton::skeleton(skeleton&& other) noexcept
:   true_joint_count(other.true_joint_count),
    joints(std::move(other.joints)),
    joint_transforms(std::move(other.joint_transforms))
{
}

time_ticks skeleton::set_animation(const std::string& name)
{
    time_ticks loop_time = 0;
    for(joint& j: joints)
    {
        j.cur_anim = nullptr;
        if(!j.pool) continue;

        auto it = j.pool->find(name);

        // Use "" as shorthand for "any" animation. Mostly useful for just
        // testing stuff around.
        if(name.size() == 0 && it == j.pool->end())
            it = j.pool->begin();

        if(it != j.pool->end())
        {
            j.cur_anim = &it->second;
            loop_time = std::max(loop_time, it->second.get_loop_time());
        }
    }
    return loop_time;
}

void skeleton::apply_animation(time_ticks time)
{
    for(joint& j: joints)
        if(j.cur_anim) j.cur_anim->apply(j.node, time);
}

skeleton& skeleton::operator=(skeleton&& other) noexcept
{
    true_joint_count = other.true_joint_count;
    joints = std::move(other.joints);
    joint_transforms = std::move(other.joint_transforms);

    return *this;
}

skeleton& skeleton::operator=(const skeleton& other)
{
    true_joint_count = other.true_joint_count;
    copy_joint_hierarchy(other);
    joint_transforms = other.joint_transforms;
    return *this;
}

void skeleton::set_skinning_mode(skinning_mode mode)
{
    switch(mode)
    {
    case LINEAR:
        joint_transforms = std::vector<mat4>(true_joint_count, mat4(1.0f));
        break;
    case DUAL_QUATERNION:
        joint_transforms = std::vector<dualquat>(
            true_joint_count, dualquat(mat3x4(1.0f))
        );
        break;
    }

    for(joint& j: joints)
        j.cached_revision--;

    refresh_joint_transforms();
}

skeleton::skinning_mode skeleton::get_skinning_mode() const
{
    return joint_transforms.index() == 0 ? LINEAR : DUAL_QUATERNION;
}

size_t skeleton::get_joint_count() const
{
    return joints.size();
}

size_t skeleton::get_true_joint_count() const
{
    return true_joint_count;
}

skeleton::joint& skeleton::get_joint(size_t i)
{
    return joints[i];
}

const skeleton::joint& skeleton::get_joint(size_t i) const
{
    return joints[i];
}

skeleton::joint& skeleton::get_joint(const std::string& name)
{
    for(joint& j: joints)
    {
        if(j.name == name)
            return j;
    }
    RB_PANIC(name, " not found in joints");
}

const skeleton::joint& skeleton::get_joint(const std::string& name) const
{
    return const_cast<skeleton*>(this)->get_joint(name);
}

int skeleton::get_joint_index(transformable* n) const
{
    // There's some ugly pointer arithmetic here: it works by finding the
    // index of the joint structure that contains the referred node.
    uintptr_t base = (uintptr_t)joints.data();
    uintptr_t end = (uintptr_t)(joints.data()+joints.size());
    uintptr_t cur = (uintptr_t)n;

    if(cur >= base && cur < end)
        return (cur - base) / sizeof(joint);
    else return -1;
}

void skeleton::reparent(transformable& node, const skeleton& from)
{
    // If the joint count doesn't match, the skeletons aren't compatible.
    if(from.joints.size() != joints.size())
        return;

    int i = from.get_joint_index(node.get_parent());
    if(i >= 0) node.set_parent(&joints[i].node);
}

void skeleton::reset()
{
    std::vector<transformable*> prev_parents;
    prev_parents.reserve(joints.size());
    // Remove parents and set transforms from inverse bind matrices
    for(joint& j: joints)
    {
        prev_parents.push_back(j.node.get_parent());
        j.node.set_parent(nullptr);
        j.node.set_transform(inverse(j.inverse_bind_matrix));
    }

    // Return in-skeleton parents without modifying transforms
    for(size_t i = 0; i < joints.size(); ++i)
    {
        joint& j = joints[i];
        if(get_joint_index(prev_parents[i]) >= 0)
            j.node.set_parent(prev_parents[i], true);
    }

    // Return out-of-skeleton parents _with_ transforms
    for(size_t i = 0; i < joints.size(); ++i)
    {
        joint& j = joints[i];
        if(get_joint_index(prev_parents[i]) < 0)
            j.node.set_parent(prev_parents[i], false);
    }
}

void skeleton::set_root_parent(transformable* parent, bool keep_transform)
{
    for(joint& j: joints)
    {
        if(j.root)
            j.node.set_parent(parent, keep_transform);
    }
}

bool skeleton::is_dirty() const
{
    for(size_t i = 0; i < true_joint_count; ++i)
    {
        uint16_t revision = joints[i].node.update_cached_transform();
        if(joints[i].cached_revision != revision)
            return true;
    }
    return false;
}

void* skeleton::refresh_joint_transforms()
{
    std::vector<mat4>* mat_joints =
        std::get_if<std::vector<mat4>>(&joint_transforms);
    std::vector<dualquat>* dq_joints =
        std::get_if<std::vector<dualquat>>(&joint_transforms);

    // We avoid even calculating the inverse mesh transform if possible.
    for(size_t i = 0; i < true_joint_count; ++i)
    {
        uint16_t revision = joints[i].node.update_cached_transform();
        if(joints[i].cached_revision != revision)
        {
            mat4 transform =
                joints[i].node.get_global_transform() *
                joints[i].inverse_bind_matrix;
            if(dq_joints)
            {
                // GLM's dualquat_cast uses a different formulation that doesn't
                // work well with blending -- that's why we don't use it.
                quat r = quat_cast(transpose(transform));
                quat t = r * quat(0.0f, vec3(transform[3])) * 0.5f;
                (*dq_joints)[i] = dualquat(r, t);
            }
            else (*mat_joints)[i] = transform;
            joints[i].cached_revision = revision;
        }
    }
    return mat_joints ? (void*)mat_joints->data() : (void*)dq_joints->data();
}

void skeleton::copy_joint_hierarchy(const skeleton& from)
{
    // Just copy the joints, then shift all parent addresses that resided in
    // the old vector into the same positions in our clone vector.
    joints = from.joints;

    for(joint& j: joints)
        reparent(j.node, from);
}

}
