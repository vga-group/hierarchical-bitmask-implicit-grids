#include "animation.hh"

namespace rb
{

rigid_animation::rigid_animation()
{
}

void rigid_animation::set_position(
    interpolation position_interpolation,
    std::vector<animation_sample<vec3>>&& position
){
    this->position = variable_animation(
        position_interpolation,
        std::move(position)
    );
}

void rigid_animation::set_scaling(
    interpolation scaling_interpolation,
    std::vector<animation_sample<vec3>>&& scaling
){
    this->scaling = variable_animation(
        scaling_interpolation,
        std::move(scaling)
    );
}

void rigid_animation::set_orientation(
    interpolation orientation_interpolation,
    std::vector<animation_sample<quat>>&& orientation
){
    this->orientation = variable_animation(
        orientation_interpolation,
        std::move(orientation)
    );
}

void rigid_animation::set_transform(
    interpolation interp,
    const std::vector<animation_sample<mat4>>& transform
){
    std::vector<animation_sample<vec3>> position_data(transform.size());
    std::vector<animation_sample<vec3>> scaling_data(transform.size());
    std::vector<animation_sample<quat>> orientation_data(transform.size());

    for(size_t i = 0; i < transform.size(); ++i)
    {
        const animation_sample<mat4>& t = transform[i];
        animation_sample<vec3>& p = position_data[i];
        animation_sample<vec3>& s = scaling_data[i];
        animation_sample<quat>& o = orientation_data[i];
        p.timestamp = s.timestamp = o.timestamp = t.timestamp;
        decompose_matrix(t.data, p.data, s.data, o.data);
        if(interp == CUBICSPLINE)
        {
            decompose_matrix(
                t.in_tangent, p.in_tangent, s.in_tangent, o.in_tangent
            );
            decompose_matrix(
                t.out_tangent, p.out_tangent, s.out_tangent, o.out_tangent
            );
        }
    }
    set_position(interp, std::move(position_data));
    set_scaling(interp, std::move(scaling_data));
    set_orientation(interp, std::move(orientation_data));
}

void rigid_animation::apply(transformable& node, time_ticks time) const
{
    if(position.size())
        node.set_position(position[time]);
    if(scaling.size())
        node.set_scaling(scaling[time]);
    if(orientation.size())
        node.set_orientation(normalize(orientation[time]));
}

time_ticks rigid_animation::get_loop_time() const
{
    time_ticks loop_time = 0;
    if(position.size())
        loop_time = std::max(position.get_loop_time(), loop_time);
    if(scaling.size())
        loop_time = std::max(scaling.get_loop_time(), loop_time);
    if(orientation.size())
        loop_time = std::max(orientation.get_loop_time(), loop_time);
    return loop_time;
}

rigid_animated::rigid_animated(const rigid_animation_pool* pool)
: pool(pool), cur_anim(nullptr)
{
}

time_ticks rigid_animated::set_animation(const std::string& name)
{
    time_ticks loop_time = 0;
    if(pool)
    {
        auto it = pool->find(name);

        // Use "" as shorthand for "any" animation. Mostly useful for just
        // testing stuff around.
        if(name.size() == 0 && it == pool->end())
            it = pool->begin();

        if(it != pool->end())
        {
            cur_anim = &it->second;
            loop_time = max(loop_time, cur_anim->get_loop_time());
        }
    }

    return loop_time;
}

void rigid_animated::apply_animation(time_ticks)
{
    // Do nothing here, we apply the animation manually in animation_updater.
}

void play_all_rigid_animations(
    scene& ctx,
    const std::string& name,
    bool loop
){
    ctx([&](transformable& t, rigid_animated& a){
        a.play(name, loop);
    });
}

void update_rigid_animations(scene& ctx, time_ticks delta)
{
    ctx([&](transformable& t, rigid_animated& a){
        a.update(delta);
        if(a.is_playing())
        {
            if(a.cur_anim) a.cur_anim->apply(t, a.get_animation_time());
        }
    });
}

}
