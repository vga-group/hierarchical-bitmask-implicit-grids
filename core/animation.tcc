#ifndef RAYBASE_ANIMATION_TCC
#define RAYBASE_ANIMATION_TCC
#include "animation.hh"
#include <algorithm>

namespace rb
{

template<typename T>
void interpolate_cubic_spline(
    const animation_sample<T>& prev,
    const animation_sample<T>& cur,
    T& output,
    float ratio,
    float scale
){
    output = cubic_spline(
        prev.data,
        prev.out_tangent * scale,
        cur.data,
        cur.in_tangent * scale,
        ratio
    );
}

template<typename T>
void interpolate_cubic_spline(
    const animation_sample<std::vector<T>>& prev,
    const animation_sample<std::vector<T>>& cur,
    std::vector<T>& output,
    float ratio,
    float scale
){
    output.resize(prev.data.size());
    for(size_t i = 0; i < prev.data.size(); ++i)
        output[i] = cubic_spline(
            prev.data[i],
            prev.out_tangent[i] * scale,
            cur.data[i],
            cur.in_tangent[i] * scale,
            ratio
        );
}

template<typename T>
void interpolate(
    time_ticks time,
    const std::vector<animation_sample<T>>& data,
    T& output,
    interpolation interp
){
    auto it = std::upper_bound(
        data.begin(), data.end(), time,
        [](time_ticks time, const animation_sample<T>& s){
            return time < s.timestamp;
        }
    );
    if(it == data.end())
    {
        output = data.back().data;
        return;
    }
    if(it == data.begin())
    {
        output = data.front().data;
        return;
    }

    auto prev = it-1;
    float frame_ticks = it->timestamp-prev->timestamp;
    float ratio = (time-prev->timestamp)/frame_ticks;
    switch(interp)
    {
    default:
    case LINEAR:
        numeric_mixer<T>()(output, prev->data, it->data, ratio);
        return;
    case STEP:
        output = prev->data;
        return;
    case CUBICSPLINE:
        {
            // Scale factor has to use seconds unfortunately.
            float scale = frame_ticks * 0.000001f;
            interpolate_cubic_spline(*prev, *it, output, ratio, scale);
        }
        return;
    case SMOOTHSTEP:
        numeric_mixer<T>()(output, prev->data, it->data, smoothstep(0.0f, 1.0f, ratio));
        return;
    }
}

template<typename T>
variable_animation<T>::variable_animation(
    interpolation interp,
    std::vector<animation_sample<T>>&& samples
):  interp(interp), samples(std::move(samples))
{}

template<typename T>
T variable_animation<T>::operator[](time_ticks time) const
{
    T temp;
    get(time, temp);
    return temp;
}

template<typename T>
void variable_animation<T>::get(time_ticks time, T& into) const
{
   interpolate<T>(time, samples, into, interp);
}

template<typename T>
time_ticks variable_animation<T>::get_loop_time() const
{
    return samples.back().timestamp;
}

template<typename T>
size_t variable_animation<T>::size() const
{
    return samples.size();
}

template<typename Derived>
animation_controller<Derived>::animation_controller()
: timer(0), loop_time(0), paused(false)
{
}

template<typename Derived>
animation_controller<Derived>& animation_controller<Derived>::queue(
    const std::string& name, bool loop
){
    animation_queue.emplace_back(name, loop);
    if(animation_queue.size() == 1)
    {
        timer = 0;
        loop_time = static_cast<Derived*>(this)->set_animation(name);
    }
    return *this;
}

template<typename Derived>
void animation_controller<Derived>::play(const std::string& name, bool loop)
{
    animation_queue.clear();
    animation_queue.emplace_back(name, loop);
    timer = 0;
    loop_time = static_cast<Derived*>(this)->set_animation(name);
}

template<typename Derived>
void animation_controller<Derived>::pause(bool paused)
{
    this->paused = paused;
}

template<typename Derived>
bool animation_controller<Derived>::is_playing() const
{
    return !animation_queue.empty() && !paused;
}

template<typename Derived>
bool animation_controller<Derived>::is_paused() const
{
    return this->paused;
}

template<typename Derived>
void animation_controller<Derived>::finish()
{
    if(animation_queue.size() != 0)
    {
        animation_queue.resize(1);
        animation_queue.front().loop = false;
    }
}

template<typename Derived>
void animation_controller<Derived>::stop()
{
    animation_queue.clear();
    timer = 0;
    loop_time = 0;
}

template<typename Derived>
const std::string&
animation_controller<Derived>::get_playing_animation_name() const
{
    static const std::string empty_dummy("");
    if(animation_queue.size() == 0) return empty_dummy;
    return animation_queue.front().name;
}

template<typename Derived>
time_ticks animation_controller<Derived>::get_animation_time() const
{
    return timer;
}

template<typename Derived>
size_t animation_controller<Derived>::get_animation_state_hash() const
{
    size_t seed = animation_queue.empty() ? 0 : animation_queue.front().name_hash;
    hash_combine(seed, timer);
    return seed;
}

template<typename Derived>
void animation_controller<Derived>::update(time_ticks dt)
{
    if(!is_playing()) return;

    timer += dt;

    animation_step& cur_step = animation_queue.front();

    // If we have a waiting animation, check if the timer rolled over the
    // looping point.
    if(animation_queue.size() > 1)
    {
        if(timer >= loop_time)
        {
            timer -= loop_time;
            animation_queue.erase(animation_queue.begin());
            loop_time = static_cast<Derived*>(this)->set_animation(
                animation_queue.front().name
            );
        }
    }
    // If there's nothing waiting, then keep looping.
    else if(cur_step.loop)
    {
        if(loop_time != 0)
            timer %= loop_time;
    }
    // If we're past the end of a non-looping animation, stop animating.
    else if(timer >= loop_time)
    {
        animation_queue.erase(animation_queue.begin());
        loop_time = 0;
        timer = 0;
        return;
    }

    static_cast<Derived*>(this)->apply_animation(timer);
}

template<typename Derived>
animation_controller<Derived>::animation_step::animation_step(const std::string& name, bool loop)
: name(name), name_hash(std::hash<std::string>()(name)), loop(loop)
{
}

}

#endif
