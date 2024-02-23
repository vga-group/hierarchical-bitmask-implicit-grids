#ifndef RAYBASE_GFX_LIGHT_HH
#define RAYBASE_GFX_LIGHT_HH
#include "core/transformable.hh"

namespace rb::gfx
{

// If you want the light to track the camera, DON'T set the camera as the
// parent! That would rotate the light and not update cascades! Call
// track_camera() instead!
struct directional_light
{
    vec3 color = vec3(1.0);
    float angular_radius = 0.263f;
};

struct point_light
{
    vec3 color = vec3(1.0);
    float radius = 0.1f;
    float cutoff_brightness = 5.0f / 256.0f;

    void set_cutoff_radius(float cutoff_radius);
    float get_cutoff_radius() const;
};

struct spotlight: public point_light
{
    float cutoff_angle = 30;
    float falloff_exponent = 1;

    // Approximates falloff exponent from the inner angle representation.
    void set_inner_angle(float inner_angle, float ratio = 1.f/255.f);
};

// This one is only as the final fallback when there are no other indirect
// lighting methods available! If you put envmaps and lightmaps etc. in your
// scene, don't expect the ambient light to have any effect!
struct ambient_light
{
    vec3 color = vec3(0.1);
};

}

#endif
