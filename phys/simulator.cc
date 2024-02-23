#include "simulator.hh"
#include "core/error.hh"
#include "core/skeleton.hh"
#include "internal.cc"
#include "collision.hh"
#include "skeleton_collider.hh"
#include "collider_internal.cc"

namespace rb::phys
{

simulator::simulator(context& ctx, scene& s, options opt):
    pool(&ctx.get_thread_pool()),
    s(&s),
    opt(opt),
    pending_async_update(false)
{
    collision_configuration.reset(new btDefaultCollisionConfiguration());
    pair_cache.reset(new btDbvtBroadphase());
    if(opt.multithreaded)
    {
        dispatcher.reset(
            new btCollisionDispatcherMt(collision_configuration.get())
        );
        constraint_solver_pool.reset(new btConstraintSolverPoolMt(
            pool->get_thread_count()
        ));
        constraint_solver.reset(new btSequentialImpulseConstraintSolverMt());
        world.reset(new btDiscreteDynamicsWorldMt(
            dispatcher.get(),
            pair_cache.get(),
            constraint_solver_pool.get(),
            constraint_solver.get(),
            collision_configuration.get()
        ));
    }
    else
    {
        dispatcher.reset(
            new btCollisionDispatcher(collision_configuration.get())
        );
        constraint_solver.reset(new btSequentialImpulseConstraintSolver());
        world.reset(new btDiscreteDynamicsWorld(
            dispatcher.get(),
            pair_cache.get(),
            constraint_solver.get(),
            collision_configuration.get()
        ));
    }

    s.foreach([&](entity id, collider* col, constraint* con, skeleton_collider* skel){
        if(col) handle(s, add_component<collider>{id, col});
        if(skel) handle(s, add_component<skeleton_collider>{id, skel});
    });
    s.add_receiver(*this);
}

simulator::~simulator()
{
    finish_update();
    world.reset();
    constraint_solver.reset();
}

void simulator::set_gravity(vec3 g)
{
    auto* dyn = static_cast<btDiscreteDynamicsWorld*>(world.get());
    dyn->setGravity(glm_to_bt(g));
}

vec3 simulator::get_gravity() const
{
    const auto* dyn = static_cast<const btDiscreteDynamicsWorld*>(world.get());
    return bt_to_glm(dyn->getGravity());
}

void simulator::set_time_step(time_ticks time_step)
{
    opt.time_step = time_step;
}

time_ticks simulator::get_time_step() const
{
    return opt.time_step;
}

void simulator::update(time_ticks delta_time)
{
    std::unique_lock lk(async_mutex);
    finish_update();
    pre_update_impl();
    step_simulation(delta_time);
    post_update_impl();
}

void simulator::start_update(time_ticks delta_time, uint32_t priority)
{
    std::unique_lock lk(async_mutex);
    if(pending_async_update)
        finish_update();

    pre_update_impl();
    pending_ticket = pool->add_task([&, delta_time=delta_time](){step_simulation(delta_time);}, priority);
    pending_async_update = true;
}

void simulator::finish_update()
{
    std::unique_lock lk(async_mutex);
    if(pending_async_update)
    {
        pending_ticket.wait();
        post_update_impl();
        pending_async_update = false;
    }
}

std::optional<intersection> simulator::cast_ray_closest_hit(
    const ray& r,
    float max_distance
){
    std::unique_lock lk(async_mutex);
    finish_update();
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

std::vector<intersection> simulator::cast_ray_all_hits(
    const ray& r,
    float max_distance
){
    std::unique_lock lk(async_mutex);
    finish_update();
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

void simulator::set_debug_drawer(btIDebugDraw* drawer)
{
    world->setDebugDrawer(drawer);
}

void simulator::debug_draw()
{
    world->debugDrawWorld();
}

void simulator::handle(scene& s, const add_component<collider>& event)
{
    std::unique_lock lk(async_mutex);
    finish_update();

    collider::impl_data* impl = event.data->get_internal();
    if(!impl->s) return;

    transformable* t = s.get<transformable>(event.id);
    add_collider(event.id, 0, t, nullptr, impl);
}

void simulator::handle(scene& s, const remove_component<collider>& event)
{
    std::unique_lock lk(async_mutex);
    finish_update();

    collider::impl_data* impl = event.data->get_internal();
    if(!impl->s) return;

    world->removeRigidBody(&impl->body);
    impl->async_simulator = nullptr;
    impl->body.setUserPointer(nullptr);
}

void simulator::handle(scene& s, const add_component<constraint>& event) {}

void simulator::handle(scene& s, const remove_component<constraint>& event)
{
    std::unique_lock lk(async_mutex);
    finish_update();

    if(event.data->instance)
    {
        world->removeConstraint(event.data->instance.get());
        event.data->instance.reset();
    }
}

void simulator::handle(scene& s, const add_component<skeleton_collider>& event)
{
    std::unique_lock lk(async_mutex);
    finish_update();

    transformable* t = s.get<transformable>(event.id);
    skeleton* skel = s.get<rb::skeleton>(event.id);
    for(size_t i = 0; i < event.data->joints.size(); ++i)
    {
        joint_collider& jc = event.data->joints[i];
        if(!jc.col.valid())
            continue;

        collider::impl_data* impl = jc.col.get_internal();

        transformable* jt = skel ? &skel->get_joint(i).node : nullptr;
        mat4 transform = jt ? jt->get_global_transform() : mat4(1);
        add_collider(event.id, i, t, &transform, impl);
    }
}

void simulator::handle(scene& s, const remove_component<skeleton_collider>& event)
{
    std::unique_lock lk(async_mutex);
    finish_update();

    for(joint_collider& jc: event.data->joints)
    {
        if(!jc.col.valid())
            continue;

        collider::impl_data* impl = jc.col.get_internal();
        world->removeRigidBody(&impl->body);
        impl->async_simulator = nullptr;
        impl->body.setUserPointer(nullptr);
    }

    for(constraint& c: event.data->constraints)
    {
        if(c.instance)
        {
            world->removeConstraint(c.instance.get());
            c.instance.reset();
        }
    }
}

void simulator::pre_update_impl()
{
    scene_state_to_world();

    // Check constraint validity & whether they need refreshes
    s->foreach([&](entity id, constraint& c){
        collider* ca = s->get<collider>(c.colliders[0]);
        collider* cb = s->get<collider>(c.colliders[1]);
        if(ca && cb)
        { // Potentially valid, check that it's up-to-date.
            RB_CHECK(ca == cb, "Constraint is from collider to itself");
            if(c.instance)
            {
                collider::impl_data* impl_a = ca->get_internal();
                collider::impl_data* impl_b = cb->get_internal();
                if(
                    &c.instance->getRigidBodyA() != &impl_a->body ||
                    &c.instance->getRigidBodyB() != &impl_b->body ||
                    c.needs_refresh
                ){
                    world->removeConstraint(c.instance.get());
                    c.instance.reset();
                }
            }
            if(!c.instance)
            {
                if(c.create_bt_constraint_instance(*s))
                    world->addConstraint(c.instance.get(), c.p.disable_collisions);
            }
        }
        else if(c.instance)
        { // Not valid, needs to be removed.
            world->removeConstraint(c.instance.get());
            c.instance.reset();
        }
    });

    s->foreach([&](entity id, skeleton_collider& skel_col){
        for(constraint& c: skel_col.constraints)
        {
            collider& ca = skel_col.joints[c.colliders[0]].col;
            collider& cb = skel_col.joints[c.colliders[1]].col;
            uint32_t category = ca.get_category_flags() & cb.get_category_flags();
            RB_CHECK(c.colliders[0] == c.colliders[1], "Skeleton constraint is from joint collider to itself");
            if(category & (phys::collider::STATIC|phys::collider::KINEMATIC))
            { // There should not be constraints between static things.
                if(c.instance)
                {
                    world->removeConstraint(c.instance.get());
                    c.instance.reset();
                }
            }
            else
            { // Any other combo is OK.
                if(c.instance)
                {
                    collider::impl_data* impl_a = ca.get_internal();
                    collider::impl_data* impl_b = cb.get_internal();
                    if(
                        &c.instance->getRigidBodyA() != &impl_a->body ||
                        &c.instance->getRigidBodyB() != &impl_b->body ||
                        c.needs_refresh
                    ){
                        world->removeConstraint(c.instance.get());
                        c.instance.reset();
                    }
                }
                if(!c.instance)
                {
                    if(c.create_bt_constraint_instance(skel_col))
                        world->addConstraint(c.instance.get(), c.p.disable_collisions);
                }
            }
        }
    });
}

void simulator::post_update_impl()
{
    world_to_scene_state();

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
                {(entity)o0->getUserIndex2(), (entity)o1->getUserIndex2()},
                {bt_to_glm(point.getPositionWorldOnA()), bt_to_glm(point.getPositionWorldOnB())},
                {bt_to_glm(point.m_localPointA), bt_to_glm(point.m_localPointB)}
            });
        }
    }
}

void simulator::step_simulation(time_ticks delta_time)
{
    float dt_sec = delta_time*0.000001f;
    float time_step_sec = opt.time_step*0.000001f;

    world->stepSimulation(
        dt_sec,
        opt.time_step <= 0 ? 0 : 4,
        opt.time_step <= 0 ? dt_sec : time_step_sec
    );
}

void simulator::scene_state_to_world()
{
    s->foreach([&](entity id, transformable* t, collider& c){
        refresh_collider(t, nullptr, c);
    });

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
            mat4 transform = jt->get_global_transform();
            refresh_collider(t, &transform, jc.col);
        }
    });
}

void simulator::world_to_scene_state()
{
    s->foreach([&](entity id, transformable* t, collider& c){
        if((c.get_category_flags() & (collider::KINEMATIC|collider::STATIC)))
            return;

        collider::impl_data* impl = c.get_internal();

        if(t && impl->body.isActive())
        {
            btTransform trans = impl->motion_state.m_graphicsWorldTrans;

            t->set_global_orientation(bt_to_glm(trans.getRotation()));
            t->set_global_position(bt_to_glm(trans.getOrigin()));
        }
    });

    s->foreach([&](entity id, transformable* t, skeleton& skel, skeleton_collider& c){
        mat4 inverse_parent = inverse(t->get_global_transform());
        for(size_t i = 0; i < c.joints.size(); ++i)
        {
            joint_collider& jc = c.joints[i];
            if(!jc.col.valid() || (jc.col.get_category_flags() & (collider::KINEMATIC|collider::STATIC)))
                continue;

            collider::impl_data* impl = jc.col.get_internal();
            if(t && impl->body.isActive())
            {
                btTransform bt_trans = impl->motion_state.m_graphicsWorldTrans;
                mat4 transform;
                bt_trans.getOpenGLMatrix(glm::value_ptr(transform));
                transform = inverse_parent * transform;

                transformable& jt = skel.get_joint(i).node;
                jt.set_global_orientation(get_matrix_orientation(transform));
                jt.set_global_position(get_matrix_translation(transform));
            }
        }
    });
}

void simulator::add_collider(
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
    impl->async_simulator = this;

    world->addRigidBody(&impl->body, impl->category_flags, impl->category_mask);

    impl->need_refresh = false;
    RB_CHECK(impl->body.getUserPointer() != nullptr,
        "Collider already exists in a simulator or collision system. "
        "Double-dipping is unsafe!"
    );
    impl->body.setUserPointer(this);
    impl->body.setUserIndex(id);
    impl->body.setUserIndex2(subindex);
}

void simulator::refresh_collider(
    transformable* base_transform,
    mat4* local_transform,
    collider& c
){
    collider::impl_data* impl = c.get_internal();
    if(impl->need_refresh)
    {
        world->removeRigidBody(&impl->body);
        impl->s->get_bt_offset(impl->motion_state.m_centerOfMassOffset);
        world->addRigidBody(&impl->body, impl->category_flags, impl->category_mask);
        impl->need_refresh = false;
    }

    // Only update kinematics if the transform has changed since the last time.
    uintptr_t revision = (uintptr_t)base_transform->update_cached_transform();
    bool outdated = (uintptr_t)impl->motion_state.m_userPointer != revision;
    if(
        base_transform && (c.get_category_flags() & (collider::KINEMATIC|collider::STATIC)) &&
        (outdated || local_transform)
    ){
        impl->motion_state.m_userPointer = (void*)revision;

        mat4 transform = base_transform->get_global_transform();
        if(local_transform)
            transform = transform * (*local_transform);

        vec3 translation, scaling;
        quat orientation;
        decompose_matrix(transform, translation, scaling, orientation);

        impl->motion_state.m_graphicsWorldTrans =
            btTransform(glm_to_bt(orientation), glm_to_bt(translation));
    }
}

}
