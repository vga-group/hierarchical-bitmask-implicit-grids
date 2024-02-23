#ifndef RAYBASE_EXTRA_GAME_OBJECT_HH
#define RAYBASE_EXTRA_GAME_OBJECT_HH
#include "core/ecs.hh"

namespace rb
{
    class transformable;
    class skeleton;
    class rigid_animated;
};

namespace rb::gfx
{
    class camera;
    class directional_light;
    class point_light;
    class spotlight;
    class model;
};

namespace rb::phys
{
    class collider;
    struct skeleton_collider;
};

namespace rb::extra
{

// This exists for convenience with the ECS stuff. It can be annoying to fetch
// everything one-by-one, so you can use this struct to fetch and store at least
// most of it at once.
struct game_object
{
    entity id = INVALID_ENTITY;
    rb::transformable* transform = nullptr;
    rb::gfx::camera* camera = nullptr;
    rb::gfx::directional_light* directional_light = nullptr;
    rb::gfx::point_light* point_light = nullptr;
    rb::gfx::spotlight* spotlight = nullptr;
    rb::gfx::model* model = nullptr;
    rb::skeleton* skeleton = nullptr;
    rb::phys::collider* collider = nullptr;
    rb::phys::skeleton_collider* skeleton_collider = nullptr;
    rb::rigid_animated* rigid_animation = nullptr;
};

// Attempts to find a "game object" by name. On failure, 'id' is set to
// INVALID_ENTITY. On success, all pointers are either null or filled in with
// a pointer to the related component.
game_object get_game_object(scene& s, const std::string& name);
game_object get_game_object(scene& s, entity id);
template<typename... Args>
game_object add_game_object(scene& s, Args&&... args)
{
    return get_game_object(s, s.add(std::forward<Args>(args)...));
}

}

#endif
