#ifndef RAYBASE_PHYS_COLLISION_HH
#define RAYBASE_PHYS_COLLISION_HH
#include "core/ecs.hh"
#include "core/math.hh"

namespace rb::phys
{

struct collision
{
    entity id[2];
    unsigned subindex[2];
    vec3 world_pos[2];
    vec3 local_pos[2];
};

struct intersection
{
    vec3 pos;
    vec3 normal;
    entity id;
    unsigned subindex; // i.e. joint index for skeleton colliders!
};

// You can register this system along with a simulator or collision_system in
// order to capture the collision events and put them in a queue, if they're
// easier to consume that way in your use case.
class collision_queue: public receiver<collision>
{
public:
    std::vector<collision>::iterator begin();
    std::vector<collision>::iterator end();
    void clear();

protected:
    void handle(scene& s, const collision& event) override;

private:
    std::vector<collision> unhandled_collisions;
};

}

#endif
