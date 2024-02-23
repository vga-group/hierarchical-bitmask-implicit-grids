#include "game_object.hh"
#include "gltf.hh"
#include "core/animation.hh"
#include "core/skeleton.hh"
#include "gfx/model.hh"
#include "gfx/light.hh"
#include "gfx/camera.hh"
#include "phys/skeleton_collider.hh"
#include "phys/collider.hh"
#include "core/filesystem.hh"
#include "core/ecs.hh"

namespace rb::extra
{

game_object get_game_object(scene& s, const std::string& name)
{
    rb::entity id = s.find<rb::extra::gltf_node>(name);
    if(id == INVALID_ENTITY) return {};
    return get_game_object(s, id);
}

game_object get_game_object(scene& s, entity id)
{
    game_object o;
    o.id = id;
    o.transform = s.get<rb::transformable>(id);
    o.camera = s.get<rb::gfx::camera>(id);
    o.directional_light = s.get<rb::gfx::directional_light>(id);
    o.point_light = s.get<rb::gfx::point_light>(id);
    o.spotlight = s.get<rb::gfx::spotlight>(id);
    o.model = s.get<rb::gfx::model>(id);
    o.skeleton = s.get<rb::skeleton>(id);
    o.collider = s.get<rb::phys::collider>(id);
    o.skeleton_collider = s.get<rb::phys::skeleton_collider>(id);
    o.rigid_animation = s.get<rb::rigid_animated>(id);
    return o;
}

}
