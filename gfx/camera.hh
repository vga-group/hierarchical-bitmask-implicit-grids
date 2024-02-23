#ifndef RAYBASE_CAMERA_HH
#define RAYBASE_CAMERA_HH
#include "core/transformable.hh"

namespace rb::gfx
{

class camera
{
public:
    camera();

    void perspective(float fov, float aspect, float near, float far = INFINITY);
    void ortho(float aspect);
    void ortho(float aspect, float near, float far);
    void ortho(float left, float right, float bottom, float top);
    void ortho(
        float left, float right, float bottom, float top,
        float near, float far
    );
    mat4 get_projection() const;
    vec4 get_projection_info() const;
    vec2 get_pixels_per_unit(uvec2 target_size) const;

    frustum get_frustum() const;
    // Returns frustum in global coordinates. You'll want to move this to the
    // object's space for AABB testing.
    frustum get_global_frustum(const transformable& t) const;

    float get_near() const;
    float get_far() const;
    vec2 get_range() const;

    void set_aspect(float aspect);
    float get_aspect() const;

    void set_fov(float fov);
    float get_fov() const;

    // Returns view-projection matrix.
    mat4 get_view_projection(const transformable& t) const;

    // vec2(0, 0) is bottom left, vec2(1, 1) is top right. The rays are in view
    // space. The ray length is such that (origin + direction).z == far.
    // near_mul can be used to adjust the starting point of the ray, 1.0f starts
    // from near plane, 0.0f starts from camera.
    ray get_view_ray(vec2 uv, float near_mul = 1.0f) const;

    ray get_global_view_ray(const transformable& t, vec2 uv = vec2(0.5)) const;

    void set_focus(
        float focus_distance,
        float f_stop,
        float aperture_angle = 0,
        float sensor_size = 0.036
    );
    vec3 get_focus_info() const;

private:
    void update_projection();

    mat4 projection;
    float fov, near, far, aspect;
    frustum f;

    vec3 focus;
    vec4 proj_info;
};

struct film_filter
{
    enum
    {
        POINT = 0,
        BOX = 1,
        GAUSSIAN = 2,
        BLACKMAN_HARRIS = 3
    } type = BLACKMAN_HARRIS;

    union {
        float radius = 1.5f;
        float sigma; // Gaussian only
    };
};

struct aperture
{
    inline static constexpr unsigned PINHOLE = 0;
    inline static constexpr unsigned DISK = 1;

    // 0 = no depth-of-field
    // 1 = perfect disk
    // 2 = (undefined, reserved for later use)
    // 3 onwards = number of sides in aperture.
    unsigned shape = PINHOLE;
};

}

#endif
