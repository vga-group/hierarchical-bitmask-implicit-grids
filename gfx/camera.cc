#include "camera.hh"

namespace rb::gfx
{

camera::camera()
:   fov(0.0f), near(0.0f), far(1.0f), aspect(1.0f),
    proj_info(0), focus(1, 0, 0)
{
}

void camera::perspective(float fov, float aspect, float near, float far)
{
    this->fov = fov;
    this->aspect = aspect;
    this->near = near;
    this->far = far;
    update_projection();
}

void camera::ortho(float aspect)
{
    ortho(-aspect, aspect, -1.0f, 1.0f);
}

void camera::ortho(float aspect, float near, float far)
{
    ortho(-aspect, aspect, -1.0f, 1.0f, near, far);
}

void camera::ortho(float left, float right, float bottom, float top)
{
    ortho(left, right, bottom, top, 0.0f, 1.0f);
}

void camera::ortho(
    float left, float right, float bottom, float top,
    float near, float far
){
    this->fov = 0.0f;
    this->near = near;
    this->far = far;
    aspect = (right-left)/(top-bottom);
    f.planes[0] = vec4(0, -1, 0, top);
    f.planes[1] = vec4(0, 1, 0, -bottom);
    f.planes[2] = vec4(1, 0, 0, -left);
    f.planes[3] = vec4(-1, 0, 0, right);
    f.planes[4] = vec4(0, 0, 1, far);
    f.planes[5] = vec4(0, 0, -1, -near);
    update_projection();
}

mat4 camera::get_projection() const
{
    return projection;
}

vec4 camera::get_projection_info() const
{
    return proj_info;
}

vec2 camera::get_pixels_per_unit(uvec2 target_size) const
{
    vec3 top(1,1,-1);
    vec3 bottom(-1,-1,-1);
    vec4 top_projection = projection * vec4(top, 1);
    top_projection /= top_projection.w;
    vec4 bottom_projection = projection * vec4(bottom, 1);
    bottom_projection /= bottom_projection.w;

    return 0.5f * vec2(
        top_projection.x - bottom_projection.x,
        top_projection.y - bottom_projection.y
    ) * vec2(target_size);
}

frustum camera::get_frustum() const
{
    return f;
}

frustum camera::get_global_frustum(const transformable& t) const
{
    return t.get_global_transform() * f;
}

float camera::get_near() const
{
    return near;
}

float camera::get_far() const
{
    return far;
}

vec2 camera::get_range() const
{
    return vec2(-near, -far);
}

void camera::set_aspect(float aspect)
{
    this->aspect = aspect;
    update_projection();
}

float camera::get_aspect() const
{
    return aspect;
}

void camera::set_fov(float fov)
{
    this->fov = fov;
    update_projection();
}

float camera::get_fov() const
{
    return fov;
}

mat4 camera::get_view_projection(const transformable& t) const
{
    return projection * glm::inverse(t.get_global_transform());
}

ray camera::get_view_ray(vec2 uv, float near_mul) const
{
    ray r;

    float use_near = -near_mul * near;

    if(proj_info.x > 0.0f) // Ortho
    {
        r.o = vec3(
            mix(-f.planes[2].w, f.planes[3].w, uv.x),
            mix(-f.planes[1].w, f.planes[0].w, uv.y),
            -use_near
        );
        r.dir = vec3(0, 0, use_near-far);
    }
    else // Perspective
    {
        vec3 dir = vec3((0.5f-uv) * vec2(proj_info.z, proj_info.w), 1.0f);
        r.o = dir * -use_near;
        r.dir = (dir * -far) - r.o;
    }

    return r;
}

ray camera::get_global_view_ray(const transformable& t, vec2 uv) const
{
    return t.get_global_transform() * get_view_ray(uv);
}

void camera::set_focus(
    float focus_distance,
    float f_stop,
    float aperture_angle,
    float sensor_size
){
    focus = vec3(
        focus_distance,
        f_stop == 0.0f ? 0.0f : sensor_size/f_stop,
        glm::radians(aperture_angle)
    );
}

vec3 camera::get_focus_info() const
{
    return vec3(focus.x, focus.y/(2.0f * tan(glm::radians(fov)*0.5f)), focus.z);
}

void camera::update_projection()
{
    if(fov == 0.0f)
    { // Ortho
        float top = f.planes[0].w;
        float bottom = -f.planes[1].w;
        float height = top-bottom;
        float left = -f.planes[2].w;
        float right = f.planes[3].w;
        float h_mid = (left+right)*0.5f;
        float width = height * aspect;
        left = h_mid - width*0.5f;
        right = h_mid + width*0.5f;

        projection = reverse_z_ortho(left, right, bottom, top, near, far);
        proj_info = vec4(far - near, -far, right-left, top - bottom);

        f.planes[0] = vec4(0, -1, 0, top);
        f.planes[1] = vec4(0, 1, 0, -bottom);
        f.planes[2] = vec4(1, 0, 0, -left);
        f.planes[3] = vec4(-1, 0, 0, right);
        f.planes[4] = vec4(0, 0, 1, far);
        f.planes[5] = vec4(0, 0, -1, -near);
    }
    else
    { // Perspective
        float rad_fov = glm::radians(fov);
        proj_info.w = 2*tan(rad_fov/2.0f);
        proj_info.z = proj_info.w*aspect;

        if(far == INFINITY)
        {
            projection = reverse_z_perspective(rad_fov, aspect, near);
            proj_info.x = -1.0/near;
            proj_info.y = 0;
        }
        else
        {
            // Giving the flipped near and far flip the depth range.
            projection = reverse_z_perspective(rad_fov, aspect, near, far);
            proj_info.x = 1.0/far - 1.0/near;
            proj_info.y = -1.0/far;
        }

        float s = sin(rad_fov); float c = cos(rad_fov);
        f.planes[0] = vec4(0, -c, -s, 0);
        f.planes[1] = vec4(0, c, -s, 0);
        f.planes[2] = vec4(normalize(vec3(c, 0, -s*aspect)), 0);
        f.planes[3] = vec4(normalize(vec3(-c, 0, -s*aspect)), 0);
        f.planes[4] = vec4(0, 0, 1.0f, far == INFINITY ? FLT_MAX : far);
        f.planes[5] = vec4(0, 0, -1.0f, -near);
    }
}

}
