// Don't include this file from any header, only include it from *.cc files
// within phys/
#ifndef RAYBASE_PHYS_COLLIDER_INTERNAL_HH
#define RAYBASE_PHYS_COLLIDER_INTERNAL_HH
#include "collider.hh"
#include "shape.hh"
#include "internal.cc"

namespace rb::phys
{

class simulator;
struct collider::impl_data
{
    // The rigid body may be in a scene that is asynchronous, meaning that the
    // state of the rigid body may be undefined if update is running. Because of
    // that, this object holds a reference to the relevant scene that is used
    // for synchronization.
    shape* s = nullptr;
    uint32_t category_flags = DYNAMIC;
    uint32_t category_mask = ALL;

    bool use_deactivation = true;

    float mass = 0.0f;
    bool need_refresh = false;

    simulator* async_simulator = nullptr;
    btDefaultMotionState motion_state;
    btRigidBody body;

    btRigidBody::btRigidBodyConstructionInfo create_info()
    {
        btRigidBody::btRigidBodyConstructionInfo info{
            mass, &motion_state, nullptr
        };
        info.m_rollingFriction = 0.01f;
        info.m_spinningFriction = 0.01f;
        return info;
    }

    impl_data(): s(nullptr), body(create_info())
    {
        body.setCollisionShape(s ? s->get_bt_shape() : nullptr);
    }
    impl_data(impl_data&& other) = delete;
    impl_data(const impl_data& other) = delete;
};

}

#endif
