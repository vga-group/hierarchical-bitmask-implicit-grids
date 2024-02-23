#include "light.hh"

namespace rb::gfx
{

void point_light::set_cutoff_radius(float cutoff_radius)
{
    cutoff_brightness = vecmax(color)/(cutoff_radius*cutoff_radius);
}

float point_light::get_cutoff_radius() const
{
    if(cutoff_brightness <= 0.0f) return INFINITY;
    vec3 radius2 = color/cutoff_brightness;
    return sqrt(max(max(radius2.x, radius2.y), radius2.z));
}

void spotlight::set_inner_angle(float inner_angle, float ratio)
{
    if(inner_angle <= 0) falloff_exponent = 1.0f;
    else
    {
        float inner = cos(glm::radians(inner_angle));
        float outer = cos(glm::radians(cutoff_angle));
        falloff_exponent = log(ratio)/log(max(1.0f-inner, 0.0f)/(1.0f-outer));
    }
}

}
