#include "constraint.hh"
#include "internal.cc"
#include "collider_internal.cc"
#include "skeleton_collider.hh"

namespace rb::phys
{

constraint::constraint(): t(FIXED), colliders{INVALID_ENTITY, INVALID_ENTITY} {}

constraint::constraint(
    type t,
    entity col_a, const frame& frame_a,
    entity col_b, const frame& frame_b,
    const params& p
): t(t), colliders{col_a, col_b}, frames{frame_a, frame_b}, p(p), needs_refresh(false)
{
}

constraint::constraint(constraint&& other) noexcept = default;

constraint::constraint(const constraint& other)
{
    operator=(other);
}

constraint::~constraint() {}

constraint& constraint::operator=(constraint&& other) noexcept = default;
constraint& constraint::operator=(const constraint& other)
{
    instance.reset();
    t = other.t;
    colliders[0] = other.colliders[0];
    colliders[1] = other.colliders[1];
    frames[0] = other.frames[0];
    frames[1] = other.frames[1];
    p = other.p;
    needs_refresh = false;
    return *this;
}

void constraint::set_type(type t) { this->t = t; }

constraint::type constraint::get_type() const
{
    return t;
}

void constraint::set_collider(int index, entity col)
{
    colliders[index] = col;
    needs_refresh = true;
}

void constraint::set_collider(int index, entity col, const frame& f)
{
    colliders[index] = col;
    frames[index] = f;
    needs_refresh = true;
}

entity constraint::get_collider(int index) const
{
    return colliders[index];
}

void constraint::set_frame(int index, const frame& f)
{
    frames[index] = f;
    needs_refresh = true;
}

constraint::frame constraint::get_frame(int index)
{
    return frames[index];
}

void constraint::set_params(const params& p)
{
    this->p = p;
    needs_refresh = true;
}

const constraint::params& constraint::get_params() const
{
    return p;
}

bool constraint::create_bt_constraint_instance(scene& s)
{
    needs_refresh = false;
    collider* ca = s.get<collider>(colliders[0]);
    collider* cb = s.get<collider>(colliders[1]);
    if(!ca || !cb) return false;
    return create_bt_constraint_instance(ca, cb);
}

bool constraint::create_bt_constraint_instance(skeleton_collider& s)
{
    needs_refresh = false;
    if(colliders[0] > s.joints.size() || colliders[1] > s.joints.size())
        return false;
    collider* ca = &s.joints[colliders[0]].col;
    collider* cb = &s.joints[colliders[1]].col;
    return create_bt_constraint_instance(ca, cb);
}

bool constraint::create_bt_constraint_instance(collider* ca, collider* cb)
{
    btRigidBody& rb_a = ca->get_internal()->body;
    btRigidBody& rb_b = cb->get_internal()->body;
    vec3 origin_a = frames[0].origin * p.scaling;
    vec3 origin_b = frames[1].origin * p.scaling;
    btTransform frame_a = btTransform(
        glm_to_bt(frames[0].orientation),
        glm_to_bt(origin_a)
    );
    btTransform frame_b = btTransform(
        glm_to_bt(frames[1].orientation),
        glm_to_bt(origin_b)
    );

    switch(t)
    {
    case FIXED:
        instance.reset(new btFixedConstraint(rb_a, rb_b, frame_a, frame_b));
        return true;
    case POINT:
        instance.reset(new btPoint2PointConstraint(
            rb_a, rb_b,
            glm_to_bt(origin_a),
            glm_to_bt(origin_b)
        ));
        return true;
    case HINGE:
        {
            btHingeConstraint* ret = new btHingeConstraint(
                rb_a, rb_b, frame_a, frame_b
            );
            if(p.use_limit_ang.z)
            {
                ret->setLimit(
                    radians(p.limit_ang_lower.z),
                    radians(p.limit_ang_upper.z)
                );
            }
            instance.reset(ret);
            return true;
        }
    case SLIDER:
    case PISTON:
        {
            btSliderConstraint* ret = new btSliderConstraint(
                rb_a, rb_b, frame_a, frame_b, t == PISTON
            );
            if(p.use_limit_lin.x)
            {
                ret->setLowerLinLimit(p.limit_lin_lower.x);
                ret->setUpperLinLimit(p.limit_lin_upper.x);
            }
            instance.reset(ret);
            return true;
        }
    case GENERIC:
        {
            btGeneric6DofConstraint* ret = new btGeneric6DofConstraint(
                rb_a, rb_b, frame_a, frame_b, true
            );
            for(int i = 0; i < 3; ++i)
            {
                if(p.use_limit_lin[i])
                    ret->setLimit(
                        i, p.limit_lin_lower[i], p.limit_lin_upper[i]
                    );
                if(p.use_limit_ang[i])
                    ret->setLimit(
                        3+i,
                        radians(p.limit_ang_lower[i]),
                        radians(p.limit_ang_upper[i])
                    );
            }
            instance.reset(ret);
            return true;
        }
    case GENERIC_SPRING:
        {
            btGeneric6DofSpringConstraint* ret =
                new btGeneric6DofSpringConstraint(
                    rb_a, rb_b, frame_a, frame_b, true
            );
            for(int i = 0; i < 3; ++i)
            {
                if(p.use_limit_lin[i])
                    ret->setLimit(
                        i, p.limit_lin_lower[i], p.limit_lin_upper[i]
                    );
                if(p.use_limit_ang[i])
                    ret->setLimit(
                        3+i,
                        radians(p.limit_ang_lower[i]),
                        radians(p.limit_ang_upper[i])
                    );
                if(p.use_spring[i])
                {
                    ret->setStiffness(i, p.spring_stiffness[i]);
                    ret->setDamping(i, p.spring_damping[i]);
                }
                if(p.use_spring_ang[i])
                {
                    ret->setStiffness(3+i, p.spring_stiffness_ang[i]);
                    ret->setDamping(3+i, p.spring_damping_ang[i]);
                }
            }
            instance.reset(ret);
            return true;
        }
    case CONE_TWIST:
        {
            btConeTwistConstraint* ret =
                new btConeTwistConstraint(rb_a, rb_b, frame_a, frame_b);
            ret->setLimit(
                radians(p.swing_span[0]),
                radians(p.swing_span[1]),
                radians(p.twist_span)
            );
            instance.reset(ret);
            return true;
        }
    }
    return false;
}

void constraint::set_scaling(vec3 scale)
{
    p.scaling = scale;
}

}

