#include "collision.hh"

namespace rb::phys
{

std::vector<collision>::iterator collision_queue::begin()
{
    return unhandled_collisions.begin();
}

std::vector<collision>::iterator collision_queue::end()
{
    return unhandled_collisions.end();
}

void collision_queue::clear()
{
    unhandled_collisions.clear();
}

void collision_queue::handle(scene& s, const collision& event)
{
    unhandled_collisions.push_back(event);
}

}
