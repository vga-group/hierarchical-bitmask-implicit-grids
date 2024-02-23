#include "collision_system.hh"
#include "skeleton_collider.hh"
#include "core/skeleton.hh"
#include "internal.cc"
#include "collider_internal.cc"

namespace rb::phys
{

collision_system::collision_system(context& ctx, scene& s, options opt)
: s(&s)
{
    collision_configuration.reset(new btDefaultCollisionConfiguration());
    pair_cache.reset(new btDbvtBroadphase());
    if(opt.multithreaded)
        dispatcher.reset(new btCollisionDispatcherMt(collision_configuration.get()));
    else
        dispatcher.reset(new btCollisionDispatcher(collision_configuration.get()));
    world.reset(new btCollisionWorld(
        dispatcher.get(),
        pair_cache.get(),
        collision_configuration.get()
    ));

    s.foreach([&](entity id, collider* c, skeleton_collider* skel){
        if(c) handle(s, add_component<collider>{id, c});
        if(skel) handle(s, add_component<skeleton_collider>{id, skel});
    });
    s.add_receiver(*this);
}

collision_system::~collision_system()
{
}

void collision_system::run()
{
    refresh_all();
    world->performDiscreteCollisionDetection();

    int num = dispatcher->getNumManifolds();
    for(int i = 0; i < num; ++i)
    {
        btPersistentManifold* man = dispatcher->getManifoldByIndexInternal(i);
        const btCollisionObject* o0 = man->getBody0();
        const btCollisionObject* o1 = man->getBody1();
        for(int j = 0; j < man->getNumContacts(); ++j)
        {
            const btManifoldPoint& point = man->getContactPoint(j);
            s->emit(collision{
                {(entity)o0->getUserIndex(), (entity)o1->getUserIndex()},
                {(unsigned)o0->getUserIndex2(), (unsigned)o1->getUserIndex2()},
                {bt_to_glm(point.getPositionWorldOnA()), bt_to_glm(point.getPositionWorldOnB())},
                {bt_to_glm(point.m_localPointA), bt_to_glm(point.m_localPointB)}
            });
        }
    }
}

void collision_system::set_debug_drawer(btIDebugDraw* drawer)
{
    world->setDebugDrawer(drawer);
}

void collision_system::debug_draw()
{
    world->debugDrawWorld();
}

std::optional<intersection> collision_system::cast_ray_closest_hit(
    const ray& r,
    float max_distance
) const
{
    btVector3 from = glm_to_bt(r.o);
    btVector3 to = glm_to_bt(r.o+normalize(r.dir)*max_distance);
    btCollisionWorld::ClosestRayResultCallback cb(from, to);
    world->rayTest(from, to, cb);
    if(cb.m_collisionObject)
        return std::optional<intersection>({
            bt_to_glm(cb.m_hitPointWorld),
            bt_to_glm(cb.m_hitNormalWorld),
            (entity)cb.m_collisionObject->getUserIndex(),
            (entity)cb.m_collisionObject->getUserIndex2()
        });
    else return std::optional<intersection>();
}

std::vector<intersection> collision_system::cast_ray_all_hits(
    const ray& r,
    float max_distance
) const
{
    btVector3 from = glm_to_bt(r.o);
    btVector3 to = glm_to_bt(r.o+normalize(r.dir)*max_distance);
    btCollisionWorld::AllHitsRayResultCallback cb(from, to);
    world->rayTest(from, to, cb);
    std::vector<intersection> res;
    res.reserve(cb.m_collisionObjects.size());
    for(int i = 0; i < cb.m_collisionObjects.size(); ++i)
    {
        res.push_back({
            bt_to_glm(cb.m_hitPointWorld[i]),
            bt_to_glm(cb.m_hitNormalWorld[i]),
            (entity)cb.m_collisionObjects[i]->getUserIndex(),
            (entity)cb.m_collisionObjects[i]->getUserIndex2()
        });
    }
    return res;
}

void collision_system::refresh_all()
{
    // Regular colliders
    s->foreach([&](entity id, transformable* t, collider& c){
        collider::impl_data* impl = c.get_internal();
        refresh_collider(t, nullptr, impl);
    });

    // Skeleton colliders
    s->foreach([&](entity id, transformable* t, skeleton& skel, skeleton_collider& c){
        vec3 scaling = t->get_global_scaling();
        if(scaling != c.get_current_scale())
            c.apply_scale(scaling);

        for(size_t i = 0; i < c.joints.size(); ++i)
        {
            joint_collider& jc = c.joints[i];
            if(!jc.col.valid())
                continue;

            transformable* jt = &skel.get_joint(i).node;
            collider::impl_data* impl = jc.col.get_internal();
            mat4 transform = jt->get_global_transform();
            refresh_collider(t, &transform, impl);
        }
    });
}

void collision_system::refresh_collider(
    transformable* base_transform,
    mat4* local_transform,
    collider::impl_data* impl
){
    if(impl->need_refresh)
    {
        world->removeCollisionObject(&impl->body);
        impl->s->get_bt_offset(impl->motion_state.m_centerOfMassOffset);
        world->addCollisionObject(&impl->body, impl->category_flags, impl->category_mask);
        impl->need_refresh = false;
    }

    // Only update if the transform has changed since the last time.
    uintptr_t revision = (uintptr_t)base_transform->update_cached_transform();
    bool outdated = (uintptr_t)impl->motion_state.m_userPointer != revision;
    if(base_transform && (outdated || local_transform))
    {
        impl->motion_state.m_userPointer = (void*)revision;

        mat4 transform = base_transform->get_global_transform();
        if(local_transform)
            transform = transform * (*local_transform);

        vec3 translation, scaling;
        quat orientation;
        decompose_matrix(transform, translation, scaling, orientation);

        impl->motion_state.m_graphicsWorldTrans =
            btTransform(glm_to_bt(orientation), glm_to_bt(translation));
        impl->body.saveKinematicState(1);
    }
}

void collision_system::handle(scene& s, const add_component<collider>& event)
{
    collider::impl_data* impl = event.data->get_internal();
    if(!impl->s) return;

    transformable* t = s.get<transformable>(event.id);
    add_collider(event.id, 0, t, nullptr, impl);
}

void collision_system::handle(scene& s, const remove_component<collider>& event)
{
    collider::impl_data* impl = event.data->get_internal();
    if(!impl->s) return;

    world->removeCollisionObject(&impl->body);
    impl->body.setUserPointer(nullptr);
}

void collision_system::handle(scene& s, const add_component<skeleton_collider>& event)
{
    transformable* t = s.get<transformable>(event.id);
    for(size_t i = 0; i < event.data->joints.size(); ++i)
    {
        joint_collider& jc = event.data->joints[i];
        if(!jc.col.valid())
            continue;

        collider::impl_data* impl = jc.col.get_internal();

        skeleton* skel = s.get<rb::skeleton>(event.id);
        transformable* jt = skel ? &skel->get_joint(i).node : nullptr;
        mat4 transform = jt ? jt->get_global_transform() : mat4(1);
        add_collider(event.id, i, t, &transform, impl);
    }
}

void collision_system::handle(scene& s, const remove_component<skeleton_collider>& event)
{
    for(joint_collider& jc: event.data->joints)
    {
        if(!jc.col.valid())
            continue;

        collider::impl_data* impl = jc.col.get_internal();
        world->removeCollisionObject(&impl->body);
        impl->body.setUserPointer(nullptr);
    }
}

void collision_system::add_collider(
    entity id,
    unsigned subindex,
    transformable* base_transform,
    mat4* local_transform,
    collider::impl_data* impl
){
    vec3 translation = vec3(0), scaling = vec3(0);
    quat orientation(1,0,0,0);

    if(base_transform)
    {
        impl->motion_state.m_userPointer = (void*)(uintptr_t)base_transform->update_cached_transform();
        mat4 transform = base_transform->get_global_transform();
        if(local_transform)
            transform = transform * (*local_transform);
        decompose_matrix(transform, translation, scaling, orientation);
    }

    impl->s->get_bt_offset(impl->motion_state.m_centerOfMassOffset);
    impl->motion_state.m_graphicsWorldTrans =
        btTransform(glm_to_bt(orientation), glm_to_bt(translation));
    impl->body.setMotionState(&impl->motion_state);

    world->addCollisionObject(&impl->body, impl->category_flags, impl->category_mask);

    impl->need_refresh = false;
    RB_CHECK(impl->body.getUserPointer() != nullptr,
        "Collider already exists in a simulator or collision system. "
        "Double-dipping is unsafe!"
    );
    impl->body.setUserPointer(this);
    impl->body.setUserIndex(id);
    impl->body.setUserIndex2(subindex);
}

}
