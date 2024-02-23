#include "skeleton_collider.hh"
#include "shape.hh"
#include "core/skeleton.hh"

namespace rb::phys
{

void skeleton_collider::auto_joint_constraints(skeleton* skel)
{
    constraints.clear();
    size_t true_joint_count = skel->get_true_joint_count();
    RB_CHECK(
        !skel || true_joint_count != joints.size(),
        "Mismatching skeleton!"
    );

    for(size_t i = 0; i < joints.size(); ++i)
    {
        joint_collider& child_joint = joints[i];
        if(!child_joint.col.valid()) continue;

        skeleton::joint& sj = skel->get_joint(i);
        if(sj.constraint.type == skeleton::joint::NONE)
            continue;

        transformable* child = &sj.node;
        transformable* parent = child;
        int parent_index = -1;
        do {
            parent = parent->get_parent();
            parent_index = skel->get_joint_index(parent);
            if(parent_index >= 0 && parent_index < true_joint_count && joints[parent_index].col.valid())
                break;
        } while(parent_index >= 0);

        if(parent_index < 0 || parent_index >= true_joint_count)
            continue;

        joint_collider& parent_joint = joints[parent_index];
        shape* child_shape = child_joint.col.get_shape();
        shape* parent_shape = parent_joint.col.get_shape();
        if(!child_shape || !parent_shape) continue;

        mat4 child_mat = child_shape->get_offset();
        mat4 parent_mat =
           parent_shape->get_offset() * inverse(parent->get_global_transform())
           * child->get_global_transform();

        static const quat orientation_fix = quat(0.5, 0.5, 0.5, 0.5);
        vec3 child_origin, child_scale, parent_origin, parent_scale;
        quat child_orientation, parent_orientation;
        decompose_matrix(child_mat, child_origin, child_scale, child_orientation);
        decompose_matrix(parent_mat, parent_origin, parent_scale, parent_orientation);

        child_orientation *= orientation_fix;
        parent_orientation *= orientation_fix;

        constraint c;
        constraint::params par;
        par.disable_collisions = true;
        switch(sj.constraint.type)
        {
        case skeleton::joint::FIXED:
            c.set_type(constraint::FIXED);
            break;
        case skeleton::joint::POINT:
            c.set_type(constraint::POINT);
            break;
        case skeleton::joint::CONE_TWIST:
            c.set_type(constraint::CONE_TWIST);
            par.swing_span.x = sj.constraint.cone_twist.swing_span1;
            par.swing_span.y = sj.constraint.cone_twist.swing_span2;
            par.twist_span = sj.constraint.cone_twist.twist_span;
            break;
        default:
            RB_PANIC("Unknown joint constraint type");
            break;
        }
        c.set_params(par);
        c.set_collider(0, i, {child_origin, child_orientation});
        c.set_collider(1, parent_index, {parent_origin, parent_orientation});
        constraints.push_back(std::move(c));
    }
}

bool skeleton_collider::is_empty() const
{
    if(joints.size() == 0) return true;
    for(const joint_collider& j: joints)
        if(j.col.valid())
            return false;
    return true;
}

void skeleton_collider::apply_scale(vec3 scaling)
{
    for(joint_collider& j: joints)
    {
        if(j.col.valid())
            j.col.apply_scale(scaling * j.init_scaling);
    }

    for(constraint& c: constraints)
        c.set_scaling(scaling);

    current_scale = scaling;
}

vec3 skeleton_collider::get_current_scale() const
{
    return current_scale;
}

void skeleton_collider::init_joint_collider_scalings()
{
    for(joint_collider& j: joints)
    {
        if(j.col.valid())
            j.init_scaling = j.col.get_shape()->get_scaling();
    }
}

void skeleton_collider::set_category_flags(uint32_t category_flags)
{
    for(joint_collider& j: joints)
        j.col.set_category_flags(category_flags);
}

void skeleton_collider::set_category_mask(uint32_t category_mask)
{
    for(joint_collider& j: joints)
        j.col.set_category_mask(category_mask);
}

void skeleton_collider::make_static()
{
    for(joint_collider& j: joints) j.col.make_static();
}

void skeleton_collider::make_dynamic()
{
    for(joint_collider& j: joints) j.col.make_dynamic();
}

void skeleton_collider::make_kinematic()
{
    for(joint_collider& j: joints) j.col.make_kinematic();
}

}

