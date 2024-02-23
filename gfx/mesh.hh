#ifndef RAYBASE_GFX_MESH_HH
#define RAYBASE_GFX_MESH_HH
#include "primitive.hh"
#include "material.hh"
#include "core/skeleton.hh"
#include "core/animation.hh"
#include <vector>
#include <optional>

namespace rb::gfx
{

struct vertex_group
{
    const primitive* source;
    std::vector<const primitive*> morph_targets;
    std::unique_ptr<primitive> animation_primitive;

    const primitive* get_primitive() const;
};

// In raybase, mesh does not contain materials. class model is the one with the
// material data too. This one only stores a list of primitives, the
// acceleration structure and animation state.
class mesh: public animation_controller<mesh>
{
friend class animation_controller<mesh>;
public:
    mesh(device& dev);

    void add_vertex_group(
        const primitive* mesh,
        std::vector<const primitive*>&& morph_targets = {}
    );
    void clear_vertex_groups();

    void set_skeleton(skeleton* skel);
    skeleton* get_skeleton();
    void clear_skeleton();

    bool is_animated() const;

    bool is_animation_dirty() const;
    void mark_animation_refreshed();

    void set_morph_target_weights(
        argvec<float> morph_target_weights
    );
    const std::vector<float>& get_morph_target_weights() const;

    using animation_pool = std::unordered_map<
        std::string /*name*/,
        variable_animation<std::vector<float>> /*weights*/
    >;

    void set_morph_target_animation_pool(const animation_pool* pool);

    // Returns false if there is no bounding box available.
    bool get_bounding_box(aabb& bounding_box) const;
    bool has_any_bounding_boxes() const;

    size_t group_count() const;
    vertex_group& operator[](size_t i);
    const vertex_group& operator[](size_t i) const;

    using iterator = std::vector<vertex_group>::iterator;
    using const_iterator = std::vector<vertex_group>::const_iterator;

    iterator begin();
    const_iterator begin() const;
    const_iterator cbegin() const;

    iterator end();
    const_iterator end() const;
    const_iterator cend() const;

private:
    time_ticks set_animation(const std::string& name);
    void apply_animation(time_ticks time);

    void create_animation_primitives();

    std::vector<vertex_group> groups;

    device* dev;
    const animation_pool* pool;
    const variable_animation<std::vector<float>>* morph_target_weights_anim;
    std::vector<float> morph_target_weights;
    bool morph_targets_dirty;
    skeleton* skel;
};

// Starts the animation on EVERY object in the scene. So it's pretty
// uncontrollable. Good for previewing animated models, bad for actual games.
void play_all_mesh_animations(
    scene& ctx,
    const std::string& name = "",
    bool loop = true
);

void update_mesh_animations(scene& ctx, time_ticks delta);

}

#endif
