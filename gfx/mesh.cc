#include "mesh.hh"
#include "model.hh"
#include "core/stack_set.hh"
#include "core/stack_allocator.hh"

namespace rb::gfx
{

mesh::mesh(device& dev):
    dev(&dev), pool(nullptr), morph_target_weights_anim(nullptr),
    morph_targets_dirty(false), skel(nullptr)
{
}

const primitive* vertex_group::get_primitive() const
{
    return animation_primitive ? animation_primitive.get() : source;
}

void mesh::add_vertex_group(
    const primitive* mesh,
    std::vector<const primitive*>&& morph_targets
){
    groups.emplace_back(vertex_group{
        mesh,
        std::move(morph_targets)
    });
    create_animation_primitives();
}

void mesh::clear_vertex_groups()
{
    groups.clear();
}

void mesh::set_skeleton(skeleton* skel)
{
    this->skel = skel;
    if(skel)
        create_animation_primitives();
}

skeleton* mesh::get_skeleton()
{
    return skel;
}

void mesh::clear_skeleton()
{
    skel = nullptr;
}

bool mesh::is_animated() const
{
    if(skel) return true;
    for(const vertex_group& vg: groups)
        if(vg.morph_targets.size() != 0 || vg.source->has_attribute(primitive::JOINTS))
            return true;
    return false;
}

bool mesh::is_animation_dirty() const
{
    if(morph_target_weights.size() != 0 && morph_targets_dirty)
        return true;
    if(skel)
        return skel->is_dirty();
    return false;
}

void mesh::mark_animation_refreshed()
{
    morph_targets_dirty = false;
}

void mesh::set_morph_target_weights(
    argvec<float> morph_target_weights
){
    this->morph_target_weights.resize(morph_target_weights.size());

    if(!std::equal(
        morph_target_weights.begin(),
        morph_target_weights.end(),
        this->morph_target_weights.begin()
    )) morph_targets_dirty = true;

    std::copy(
        morph_target_weights.begin(),
        morph_target_weights.end(),
        this->morph_target_weights.begin()
    );
}

const std::vector<float>& mesh::get_morph_target_weights() const
{
    return morph_target_weights;
}

void mesh::set_morph_target_animation_pool(
    const animation_pool* pool
){
    this->pool = pool;
}

bool mesh::get_bounding_box(aabb& bounding_box) const
{
    if(groups.size() == 0) return false;

    aabb range{vec3(FLT_MAX), vec3(-FLT_MAX)};

    for(const auto& group: groups)
    {
        const primitive* mesh = group.get_primitive();
        if(!mesh) continue;

        aabb tmp;
        if(!mesh->get_bounding_box(tmp))
            return false;
        range.min = min(range.min, tmp.min);
        range.max = max(range.max, tmp.max);
    }
    bounding_box = range;
    return true;
}

bool mesh::has_any_bounding_boxes() const
{
    if(groups.size() == 0) return false;

    for(const auto& group: groups)
    {
        const primitive* mesh = group.get_primitive();
        if(mesh && mesh->has_bounding_box())
            return true;
    }
    return false;
}

size_t mesh::group_count() const
{
    return groups.size();
}

vertex_group& mesh::operator[](size_t i)
{
    return groups[i];
}

const vertex_group& mesh::operator[](size_t i) const
{
    return groups[i];
}

mesh::iterator mesh::begin()
{
    return groups.begin();
}

mesh::const_iterator mesh::begin() const
{
    return groups.begin();
}

mesh::const_iterator mesh::cbegin() const
{
    return groups.cbegin();
}

mesh::iterator mesh::end()
{
    return groups.end();
}

mesh::const_iterator mesh::end() const
{
    return groups.end();
}

mesh::const_iterator mesh::cend() const
{
    return groups.cend();
}

time_ticks mesh::set_animation(const std::string& name)
{
    time_ticks loop_time = 0;
    if(pool)
    {
        auto it = pool->find(name);

        // Use "" as shorthand for "any" animation. Mostly useful for just
        // testing stuff around.
        if(name.size() == 0 && it == pool->end())
            it = pool->begin();

        if(it != pool->end())
        {
            morph_target_weights_anim = &it->second;
            loop_time = max(loop_time, morph_target_weights_anim->get_loop_time());
        }
    }

    if(skel)
        loop_time = max(loop_time, skel->set_animation(name));

    return loop_time;
}

void mesh::apply_animation(time_ticks time)
{
    if(skel)
        skel->apply_animation(time);

    if(!morph_target_weights_anim)
        return;

    morph_target_weights_anim->get(time, morph_target_weights);
    morph_targets_dirty = true;
}

void mesh::create_animation_primitives()
{
    bool has_skeleton = skel;
    for(vertex_group& vg: groups)
    {
        if((has_skeleton || vg.morph_targets.size() != 0) && !vg.animation_primitive)
            vg.animation_primitive.reset(new primitive(vg.source));
    }
}

void play_all_mesh_animations(
    scene& ctx,
    const std::string& name,
    bool loop
){
    // There cannot be more reachable meshes than there are models in the scene,
    // because each model can reference only one mesh.
    stack_set<mesh*> reachable_meshes(ctx.count<model>());

    ctx([&](model& m){
        mesh* me = m.m;
        if(me && me->is_animated() && reachable_meshes.insert(me))
            me->play(name, loop);
    });
}

void update_mesh_animations(scene& ctx, time_ticks delta)
{
    // There cannot be more reachable meshes than there are models in the scene,
    // because each model can reference only one mesh.
    stack_set<mesh*> reachable_meshes(ctx.count<model>());

    ctx([&](model& m){
        mesh* me = m.m;
        if(me && me->is_playing() && reachable_meshes.insert(me))
            me->update(delta);
    });
}

}

