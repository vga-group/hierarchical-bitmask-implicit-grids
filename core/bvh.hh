#ifndef RAYBASE_BVH_HH
#define RAYBASE_BVH_HH
#include "math.hh"
#include <vector>

namespace rb
{


enum class bvh_heuristic
{
    EQUAL_COUNT,
    MIDDLE,
    AREA_WEIGHTED,
    SURFACE_AREA_HEURISTIC
};

template<typename T>
class bvh
{
public:
    void clear();
    void add(aabb bounding_box, const T& t, uint16_t mask = 0xFFFF);
    void add(aabb bounding_box, T&& t, uint16_t mask = 0xFFFF);
    // You'll need to call refit() or build() to have the BVH reflect the changes
    // made in this update!
    // bool on_leaf(aabb& bounding_box, T& t, uint16_t& mask)
    // on_leaf must return true if AABB or mask was changed.
    template<typename F>
    void update(F&& on_leaf);
    size_t size() const;

    void build(bvh_heuristic bh = bvh_heuristic::AREA_WEIGHTED);
    void rebuild(
        bvh_heuristic bh = bvh_heuristic::AREA_WEIGHTED,
        float rebuild_trigger_relative_delta = 0.01
    );
    void refit();

    template<typename F>
    void query(vec3 point, F&& on_overlap, uint16_t mask = 0xFFFF) const;

    template<typename F>
    void query(ray r, F&& on_intersect, uint16_t mask = 0xFFFF) const;

    template<typename F>
    void query(aabb bounding_box, F&& on_overlap, uint16_t mask = 0xFFFF) const;

    template<typename F, typename U>
    void query(const bvh<U>& other, F&& on_overlap, uint16_t mask = 0xFFFF, uint16_t mask_other = 0xFFFF) const;

    template<typename F>
    void foreach(F&& callback, uint16_t mask = 0xFFFF) const;

private:
    struct node
    {
        alignas(16) pvec3 center;
        uint32_t child_offset;
        alignas(16) pvec3 radius;
        int8_t axis;
        uint8_t status; // 0: up to date, 1: outdated (LEAF ONLY!)
        uint16_t mask; // Used for culling
    };
    static_assert(sizeof(node) == 32, "BVH node size must be 32!");

    bool refit_subtree(uint32_t index, float& refit_delta);

    template<typename F>
    void point_traverse(uint32_t index, vec3 point, uint16_t mask, F&& on_overlap) const;

    template<typename F>
    bool ray_traverse(uint32_t index, vec3 o, vec3 inv_dir, uint16_t mask, F&& on_intersect) const;

    template<typename F>
    void aabb_traverse(uint32_t index, aabb bounding_box, uint16_t mask, F&& on_overlap) const;

    template<typename F, typename U>
    void bvh_traverse(uint32_t index_self, uint32_t index_other, const bvh<U>& other, uint16_t mask, uint16_t mask_other, F&& on_overlap) const;

    void build_recursive_equal(node* node_data, size_t node_count);
    void build_recursive_middle(node* node_data, size_t node_count);
    void build_recursive_area_weighted(node* node_data, size_t node_count);
    void build_recursive_sah(node* node_data, size_t node_count);

    float accumulated_refit_delta = 0;
    std::vector<T> data;
    std::vector<node> build_nodes;
    std::vector<node> nodes;
};

}

#include "bvh.tcc"

#endif
