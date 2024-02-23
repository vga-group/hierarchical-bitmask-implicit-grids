#ifndef RAYBASE_ANIMATION_HH
#define RAYBASE_ANIMATION_HH
#include "transformable.hh"
#include "ecs.hh"
#include "types.hh"
#include <vector>
#include <unordered_map>

namespace rb
{

namespace easing
{
    inline double linear(double t)
    {
        return t;
    }
    inline double ease(double t)
    {
        return cubic_bezier({0.25, 0.1}, {0.25, 1.0}, t);
    }
    inline double ease_in(double t)
    {
        return cubic_bezier({0.42, 0.0}, {1.0, 1.0}, t);
    }
    inline double ease_out(double t)
    {
        return cubic_bezier({0.0, 0.0}, {0.58, 1.0}, t);
    }
    inline double ease_in_out(double t)
    {
        return cubic_bezier({0.42, 0.0}, {0.58, 1.0}, t);
    }
}

// You can define custom mixers for your time if the required operators
// aren't available or don't work. They should have the exact same function
// as this, just linearly mix between the begin and end values. Easing is
// done by a separate easing function that is applied to 't' before passing
// it to this.
template<typename T>
struct numeric_mixer
{
    void operator()(T& output, const T& begin, const T& end, double t) const
    {
        output = begin * (1.0 - t) + end * t;
    }
};

template<glm::length_t L, typename T, glm::qualifier Q>
struct numeric_mixer<glm::vec<L, T, Q>>
{
    using V = glm::vec<L, T, Q>;
    void operator()(V& output, const V& begin, const V& end, double t) const
    {
        output = begin * T(1.0 - t) + end * T(t);
    }
};

template<typename T, glm::qualifier Q>
struct numeric_mixer<glm::qua<T, Q>>
{
    using V = glm::qua<T, Q>;
    void operator()(V& output, const V& begin, const V& end, double t) const
    {
        output = glm::slerp(begin, end, T(t));
    }
};

template<typename T>
struct numeric_mixer<std::vector<T>>
{
    using V = std::vector<T>;
    void operator()(V& output, const V& begin, const V& end, double t) const
    {
        output.resize(begin.size());
        for(size_t i = 0; i < begin.size(); ++i)
            numeric_mixer<T>()(output[i], begin[i], end[i], t);
    }
};

template<typename T>
struct animation_sample
{
    time_ticks timestamp;
    T data;
    // These are only used if the interpolation is CUBICSPLINE.
    T in_tangent;
    T out_tangent;
};

enum interpolation
{
    LINEAR = 0,
    STEP,
    CUBICSPLINE,
    SMOOTHSTEP
};

template<typename T>
void interpolate(
    time_ticks time,
    const std::vector<animation_sample<T>>& data,
    T& output,
    interpolation interp
);

template<typename T>
class variable_animation
{
public:
    variable_animation(
        interpolation interp = LINEAR,
        std::vector<animation_sample<T>>&& samples = {}
    );

    T operator[](time_ticks time) const;
    void get(time_ticks time, T& into) const;
    time_ticks get_loop_time() const;
    size_t size() const;

private:
    interpolation interp;
    std::vector<animation_sample<T>> samples;
};

class rigid_animation
{
public:
    rigid_animation();

    void set_position(
        interpolation position_interpolation,
        std::vector<animation_sample<vec3>>&& position
    );

    void set_scaling(
        interpolation scaling_interpolation,
        std::vector<animation_sample<vec3>>&& scaling
    );

    void set_orientation(
        interpolation orientation_interpolation,
        std::vector<animation_sample<quat>>&& orientation
    );

    void set_transform(
        interpolation interpolation,
        const std::vector<animation_sample<mat4>>& transform
    );

    void apply(transformable& node, time_ticks time) const;
    time_ticks get_loop_time() const;

private:
    variable_animation<vec3> position;
    variable_animation<vec3> scaling;
    variable_animation<quat> orientation;
};

using rigid_animation_pool = std::unordered_map<
    std::string /*name*/,
    rigid_animation
>;

// You can use this to provide the animation functions to your class, as long as
// you implement the following member functions:
//   time_ticks set_animation(const std::string& name);
//   void apply_animation(time_ticks time);
template<typename Derived>
class animation_controller
{
public:
    animation_controller();

    // Starts playing the queued animation at the next loop point, or
    // immediately if there are no playing animations. Returns a reference to
    // this object for chaining purposes.
    animation_controller& queue(const std::string& name, bool loop = false);
    void play(const std::string& name, bool loop = false);
    void pause(bool paused = true);
    // The animation can be unpaused and still not play simply when there is
    // no animation in the queue left to play.
    bool is_playing() const;
    bool is_paused() const;
    // Drops queued animations and ends the looping of the current animation.
    void finish();
    // Drops queued animations and instantly stops current animation as well.
    void stop();
    const std::string& get_playing_animation_name() const;
    time_ticks get_animation_time() const;
    size_t get_animation_state_hash() const;

    void update(time_ticks dt);

private:
    struct animation_step
    {
        animation_step(const std::string& name, bool loop);
        std::string name;
        size_t name_hash;
        bool loop;
    };
    std::vector<animation_step> animation_queue;
    time_ticks timer;
    time_ticks loop_time;
    bool paused;
};

class animation_updater;

struct rigid_animated: public animation_controller<rigid_animated>
{
friend class animation_controller<rigid_animated>;
public:
    rigid_animated(const rigid_animation_pool* pool = nullptr);

    const rigid_animation_pool* pool;
    const rigid_animation* cur_anim;

protected:
    time_ticks set_animation(const std::string& name);
    void apply_animation(time_ticks time);
};

void play_all_rigid_animations(
    scene& ctx,
    const std::string& name = "",
    bool loop = true
);

void update_rigid_animations(scene& ctx, time_ticks delta);

}

#include "animation.tcc"
#endif
