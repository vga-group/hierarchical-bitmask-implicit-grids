#include "collider.hh"
#include "shape.hh"
#include "simulator.hh"
#include "collider_internal.cc"

namespace
{

using namespace rb::phys;

uint32_t get_collision_flags(uint32_t category_flags)
{
    uint32_t collision_flags = 0;
    if(category_flags & collider::DYNAMIC)
        collision_flags |= btCollisionObject::CF_DYNAMIC_OBJECT;
    if(category_flags & collider::STATIC)
        collision_flags |= btCollisionObject::CF_STATIC_OBJECT;
    if(category_flags & collider::KINEMATIC)
        collision_flags |= btCollisionObject::CF_KINEMATIC_OBJECT;
    if(category_flags & collider::CHARACTER)
        collision_flags |= btCollisionObject::CF_CHARACTER_OBJECT;
    return collision_flags;
}

}

namespace rb::phys
{

collider::collider()
{}

collider::collider(
    shape* s,
    uint32_t category_flags,
    uint32_t category_mask,
    float mass
){
    impl.reset(new impl_data());
    impl->mass = max(mass, 0.0f);
    if(s) set_shape(s);
    set_category_flags(category_flags);
    set_category_mask(category_mask);
}

collider::collider(const collider& other)
{
    operator=(other);
}

collider::collider(collider&& other) noexcept
{
    operator=(std::move(other));
}

collider::~collider()
{
#ifdef DEBUG_PHYSICS
    if(impl && impl->currently_in_scene())
        throw std::runtime_error(
            "Attempted to destroy a collider that is in a scene. "
            "This would result in undefined behaviour."
        );
#endif
}

void collider::set_mass(float mass)
{
    impl_data& impl = safe_get_impl();
    if(mass <= 0) mass = 0;
    impl.mass = mass;

    btCollisionShape* bt_shape = impl.s ?  impl.s->get_bt_shape() : nullptr;

    btVector3 inertia(0, 0, 0);
    if(
        !(impl.category_flags & (collider::KINEMATIC|collider::STATIC)) &&
        bt_shape && mass > 0
    ) bt_shape->calculateLocalInertia(mass, inertia);
    else mass = 0.0f;

    impl.body.setMassProps(mass, inertia);
    impl.body.updateInertiaTensor();
}

float collider::get_mass() const
{
    impl_data& impl = safe_get_impl(false);
    return impl.mass;
}

void collider::set_friction(float friction)
{
    impl_data& impl = safe_get_impl();
    impl.body.setFriction(friction);
}

float collider::get_friction() const
{
    impl_data& impl = safe_get_impl(false);
    return impl.body.getFriction();
}

void collider::set_restitution(float restitution)
{
    impl_data& impl = safe_get_impl();
    impl.body.setRestitution(restitution);
}

float collider::get_restitution() const
{
    impl_data& impl = safe_get_impl(false);
    return impl.body.getRestitution();
}

void collider::set_linear_damping(float linear_damping)
{
    impl_data& impl = safe_get_impl();
    impl.body.setDamping(linear_damping, impl.body.getAngularDamping());
}

float collider::get_linear_damping() const
{
    impl_data& impl = safe_get_impl(false);
    return impl.body.getLinearDamping();
}

void collider::set_angular_damping(float angular_damping)
{
    impl_data& impl = safe_get_impl();
    impl.body.setDamping(impl.body.getLinearDamping(), angular_damping);
}

float collider::get_angular_damping() const
{
    impl_data& impl = safe_get_impl(false);
    return impl.body.getAngularDamping();
}

void collider::set_category_flags(uint32_t category_flags)
{
    impl_data& impl = safe_get_impl();
    impl.category_flags = category_flags;
    impl.body.setCollisionFlags(get_collision_flags(impl.category_flags));
    update_activation_state();
    set_mass(impl.mass);
}

uint32_t collider::get_category_flags() const
{
    impl_data& impl = safe_get_impl(false);
    return impl.category_flags;
}

void collider::make_static()
{
    uint32_t category_flags = get_category_flags();
    category_flags &= ~(KINEMATIC|DYNAMIC);
    category_flags |= STATIC;
    set_category_flags(category_flags);
    set_category_mask(DYNAMIC);
}

void collider::make_dynamic()
{
    uint32_t category_flags = get_category_flags();
    category_flags &= ~(STATIC|KINEMATIC);
    category_flags |= DYNAMIC;
    set_category_flags(category_flags);
    set_category_mask(ALL);
}

void collider::make_kinematic()
{
    uint32_t category_flags = get_category_flags();
    category_flags &= ~(STATIC|DYNAMIC);
    category_flags |= KINEMATIC;
    set_category_flags(category_flags);
    set_category_mask(DYNAMIC);
}

void collider::set_category_mask(uint32_t category_mask)
{
    impl_data& impl = safe_get_impl();
    impl.category_mask = category_mask;
    impl.need_refresh = true;
}

uint32_t collider::get_category_mask() const
{
    impl_data& impl = safe_get_impl(false);
    return impl.category_mask;
}

void collider::set_shape(shape* s)
{
    impl_data& impl = safe_get_impl();
    impl.need_refresh = true;
    impl.s = s;
    impl.body.setCollisionShape(s ? s->get_bt_shape() : nullptr);
    set_mass(impl.mass);
}

shape* collider::get_shape() const
{
    impl_data& impl = safe_get_impl();
    return impl.s;
}

void collider::set_use_deactivation(bool use_deactivation)
{
    impl_data& impl = safe_get_impl();
    impl.use_deactivation = use_deactivation;
    update_activation_state();
}

bool collider::get_use_deactivation() const
{
    impl_data& impl = safe_get_impl();
    return impl.use_deactivation;
}

void collider::impulse(vec3 impulse)
{
    impl_data& impl = safe_get_impl();
    impl.body.applyCentralImpulse(glm_to_bt(impulse));
}

void collider::set_velocity(vec3 velocity)
{
    impl_data& impl = safe_get_impl();
    impl.body.setLinearVelocity(glm_to_bt(velocity));
}

vec3 collider::get_velocity() const
{
    impl_data& impl = safe_get_impl();
    return bt_to_glm(impl.body.getLinearVelocity());
}

collider& collider::operator=(const collider& other)
{
    if(!other.impl)
    {
        impl.reset();
        return *this;
    }

    impl_data& impl = safe_get_impl();
    impl_data& o = *other.impl;
    impl.s = o.s;
    impl.category_flags = o.category_flags;
    impl.category_mask = o.category_mask;
    impl.use_deactivation = o.use_deactivation;
    impl.mass = o.mass;
    impl.need_refresh = true;
    impl.motion_state = o.motion_state;
    impl.body.clearForces();
    impl.body.setMotionState(&impl.motion_state);
    impl.body.setMassProps(o.mass, o.body.getLocalInertia());
    impl.body.setLinearVelocity(o.body.getLinearVelocity());
    impl.body.setAngularVelocity(o.body.getAngularVelocity());
    impl.body.setFriction(o.body.getFriction());
    impl.body.setRestitution(o.body.getRestitution());
    impl.body.setRollingFriction(o.body.getRollingFriction());
    impl.body.setSpinningFriction(o.body.getSpinningFriction());
    impl.body.setDamping(o.body.getLinearDamping(), o.body.getAngularDamping());
    impl.body.setCollisionShape(o.body.getCollisionShape());
    impl.body.setCollisionFlags(o.body.getCollisionFlags());
    impl.body.forceActivationState(o.body.getActivationState());
    return *this;
}

collider& collider::operator=(collider&& other) noexcept
{
#ifdef DEBUG_PHYSICS
    if(other.impl && other.impl->currently_in_scene())
        throw std::runtime_error(
            "Attempted to move a collider that is in a scene. "
            "This would result in undefined behaviour."
        );
#endif
    impl = std::move(other.impl);
    return *this;
}

bool collider::valid() const
{
    return impl && impl->s != nullptr;
}

void collider::apply_scale(const transformable* n)
{
    if(!n) return;
    apply_scale(n->get_global_scaling());
}

void collider::apply_scale(vec3 scaling)
{
    impl_data& impl = safe_get_impl();
    if(impl.s)
    {
        impl.s->apply_scale(scaling);
        impl.need_refresh = true;
    }
}

collider::impl_data* collider::get_internal() const
{
    return &safe_get_impl(false);
}

collider::impl_data& collider::safe_get_impl(bool need_sync) const
{
    if(!impl) impl.reset(new impl_data());
    if(need_sync && impl->async_simulator)
        impl->async_simulator->finish_update();
    return *impl;
}

void collider::update_activation_state()
{
    impl_data& impl = safe_get_impl();

    int state = 0;
    if(impl.category_flags & collider::STATIC)
        state = 0;
    else if(impl.category_flags & collider::KINEMATIC || !impl.use_deactivation)
        state = DISABLE_DEACTIVATION;
    else
        state = ACTIVE_TAG;

    impl.body.forceActivationState(state);
    impl.need_refresh = true;
}

}

