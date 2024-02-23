#ifndef RAYBASE_BVH_TCC
#define RAYBASE_BVH_TCC
#include "bvh.hh"
#include "log.hh"

namespace rb
{

template<typename T>
void bvh<T>::clear()
{
    accumulated_refit_delta = 0;
    data.clear();
    build_nodes.clear();
    nodes.clear();
}

template<typename T>
void bvh<T>::add(aabb bounding_box, const T& t, uint16_t mask)
{
    nodes.clear();
    vec3 center = (bounding_box.min + bounding_box.max) * 0.5f;
    vec3 radius = bounding_box.max - center;
    build_nodes.emplace_back(node{center, uint32_t(data.size()), radius, -1, 1, mask});
    data.emplace_back(t);
}

template<typename T>
void bvh<T>::add(aabb bounding_box, T&& t, uint16_t mask)
{
    nodes.clear();
    vec3 center = (bounding_box.min + bounding_box.max) * 0.5f;
    vec3 radius = bounding_box.max - center;
    build_nodes.emplace_back(node{center, uint32_t(data.size()), radius, -1, 1, mask});
    data.emplace_back(std::move(t));
}

template<typename T>
template<typename F>
void bvh<T>::update(F&& on_leaf)
{
    if(nodes.size() == 0)
    {
        for(size_t i = 0; i < build_nodes.size(); ++i)
        {
            node& n = build_nodes[i];
            aabb bounding_box = {
                n.center - n.radius,
                n.center + n.radius
            };
            if(on_leaf(bounding_box, data[n.child_offset], n.mask))
            {
                n.center = (bounding_box.max + bounding_box.min)*0.5f;
                n.radius = pvec3(bounding_box.max) - n.center;
                n.status = 1;
            }
        }
    }
    else
    {
        bool changed = false;
        for(size_t i = 0; i < nodes.size(); ++i)
        {
            if(nodes[i].axis < 0)
            {
                node& n = nodes[i];
                aabb bounding_box = {
                    n.center - n.radius,
                    n.center + n.radius
                };
                if(on_leaf(bounding_box, data[n.child_offset], n.mask))
                {
                    n.center = (bounding_box.max + bounding_box.min)*0.5f;
                    n.radius = pvec3(bounding_box.max) - n.center;
                    n.status = 1;
                    changed = true;
                }
            }
        }
        if(changed)
            nodes[0].status = 1;
    }
}

template<typename T>
size_t bvh<T>::size() const
{
    return data.size();
}

template<typename T>
void bvh<T>::build(bvh_heuristic bh)
{
    nodes.clear();
    nodes.reserve(build_nodes.size() * 2);
    accumulated_refit_delta = 0;
    switch(bh)
    {
    case bvh_heuristic::EQUAL_COUNT:
        build_recursive_equal(build_nodes.data(), build_nodes.size());
        break;
    case bvh_heuristic::MIDDLE:
        build_recursive_middle(build_nodes.data(), build_nodes.size());
        break;
    case bvh_heuristic::AREA_WEIGHTED:
        build_recursive_area_weighted(build_nodes.data(), build_nodes.size());
        break;
    case bvh_heuristic::SURFACE_AREA_HEURISTIC:
        build_recursive_sah(build_nodes.data(), build_nodes.size());
        break;
    }
}

template<typename T>
void bvh<T>::rebuild(
    bvh_heuristic bh,
    float rebuild_trigger_relative_delta
){
    if(nodes.size() == 0)
    {
        //RB_LOG("Full BVH rebuild due to never being built before");
        build(bh);
    }
    else if(accumulated_refit_delta > rebuild_trigger_relative_delta)
    {
        //RB_LOG("Full BVH rebuild due to deterioration: ", accumulated_refit_delta, " > ", rebuild_trigger_relative_delta);
        build_nodes.clear();
        for(size_t i = 0; i < nodes.size(); ++i)
        {
            if(nodes[i].axis < 0)
                build_nodes.push_back(nodes[i]);
        }
        build(bh);
    }
    else
    {
        //RB_LOG("BVH refit due to delta ", accumulated_refit_delta, " < ", rebuild_trigger_relative_delta);
        refit();
    }
}

template<typename T>
void bvh<T>::refit()
{
    if(nodes.size() == 0 || nodes[0].status == 0) return;

    float refit_delta = 0.0f;
    refit_subtree(0, refit_delta);
    accumulated_refit_delta += refit_delta;
}

template<typename T>
void bvh<T>::build_recursive_equal(node* node_data, size_t node_count)
{
    if(node_count == 0) return;
    if(node_count == 1)
    {
        node new_node = node_data[0];
        new_node.status = 0;
        nodes.push_back(new_node);
        return;
    }

    aabb group_aabb = {vec3(FLT_MAX), vec3(-FLT_MAX)};
    uint16_t mask = 0;
    for(uint32_t i = 0; i < node_count; ++i)
    {
        node& n = node_data[i];
        group_aabb.min = min(group_aabb.min, vec3(n.center) - vec3(n.radius));
        group_aabb.max = max(group_aabb.max, vec3(n.center) + vec3(n.radius));
        mask |= n.mask;
    }

    vec3 size = group_aabb.max - group_aabb.min;
    int axis = 2;
    if(size.x > size.y && size.x > size.z) axis = 0;
    else if(size.y > size.z) axis = 1;

    uint32_t own_index = nodes.size();
    nodes.push_back({
        (group_aabb.max + group_aabb.min) * 0.5f,
        own_index,
        size * 0.5f,
        int8_t(axis),
        0,
        mask
    });

    size_t split_count = node_count/2;
    std::nth_element(
        node_data,
        node_data+split_count,
        node_data+node_count,
        [&](const node& a, const node& b) {
            return a.center[axis] < b.center[axis];
        }
    );

    build_recursive_equal(node_data, split_count);
    nodes[own_index].child_offset = nodes.size();
    build_recursive_equal(node_data+split_count, node_count-split_count);
}

template<typename T>
void bvh<T>::build_recursive_middle(node* node_data, size_t node_count)
{
    if(node_count == 0) return;
    if(node_count == 1)
    {
        node new_node = node_data[0];
        new_node.status = 0;
        nodes.push_back(new_node);
        return;
    }

    aabb group_aabb = {vec3(FLT_MAX), vec3(-FLT_MAX)};
    uint16_t mask = 0;
    for(uint32_t i = 0; i < node_count; ++i)
    {
        node& n = node_data[i];
        group_aabb.min = min(group_aabb.min, vec3(n.center) - vec3(n.radius));
        group_aabb.max = max(group_aabb.max, vec3(n.center) + vec3(n.radius));
        mask |= n.mask;
    }

    vec3 size = group_aabb.max - group_aabb.min;
    int axis = 2;
    if(size.x > size.y && size.x > size.z) axis = 0;
    else if(size.y > size.z) axis = 1;

    vec3 center = (group_aabb.max + group_aabb.min) * 0.5f;
    float split = center[axis];

    uint32_t own_index = nodes.size();
    nodes.push_back({center, own_index, size * 0.5f, int8_t(axis), 0, mask});

    size_t split_count = std::partition(
        node_data, node_data+node_count,
        [&](const node& n) {return n.center[axis] < split;}
    ) - node_data;

    if(split_count == 0 || split_count == node_count)
        split_count = node_count/2;

    build_recursive_middle(node_data, split_count);
    nodes[own_index].child_offset = nodes.size();
    build_recursive_middle(node_data+split_count, node_count-split_count);
}

template<typename T>
void bvh<T>::build_recursive_area_weighted(node* node_data, size_t node_count)
{
    if(node_count == 0) return;
    if(node_count == 1)
    {
        node new_node = node_data[0];
        new_node.status = 0;
        nodes.push_back(new_node);
        return;
    }

    aabb group_aabb = {vec3(FLT_MAX), vec3(-FLT_MAX)};
    vec3 center = vec3(0);
    vec3 center_weight = vec3(0);
    uint16_t mask = 0;
    for(uint32_t i = 0; i < node_count; ++i)
    {
        node& n = node_data[i];
        center += vec3(n.center) * vec3(n.radius);
        center_weight += vec3(n.radius);
        group_aabb.min = min(group_aabb.min, vec3(n.center) - vec3(n.radius));
        group_aabb.max = max(group_aabb.max, vec3(n.center) + vec3(n.radius));
        mask |= n.mask;
    }
    center /= center_weight;

    vec3 size = group_aabb.max - group_aabb.min;
    int axis = 2;
    if(size.x > size.y && size.x > size.z) axis = 0;
    else if(size.y > size.z) axis = 1;

    float split = center[axis];

    uint32_t own_index = nodes.size();
    nodes.push_back({
        (group_aabb.max + group_aabb.min) * 0.5f, own_index, size * 0.5f,
        int8_t(axis), 0, mask
    });

    size_t split_count = std::partition(
        node_data, node_data+node_count,
        [&](const node& n) {return n.center[axis] < split;}
    ) - node_data;

    if(split_count == 0 || split_count == node_count)
        split_count = node_count/2;

    build_recursive_area_weighted(node_data, split_count);
    nodes[own_index].child_offset = nodes.size();
    build_recursive_area_weighted(node_data+split_count, node_count-split_count);
}

template<typename T>
void bvh<T>::build_recursive_sah(node* node_data, size_t node_count)
{
    if(node_count <= 4)
    {
        build_recursive_equal(node_data, node_count);
        return;
    }

    aabb group_aabb = {vec3(FLT_MAX), vec3(-FLT_MAX)};
    uint16_t mask = 0;
    for(uint32_t i = 0; i < node_count; ++i)
    {
        node& n  = node_data[i];
        group_aabb.min = min(group_aabb.min, vec3(n.center) - vec3(n.radius));
        group_aabb.max = max(group_aabb.max, vec3(n.center) + vec3(n.radius));
        mask |= n.mask;
    }

    vec3 size = group_aabb.max - group_aabb.min;
    int axis = 2;
    if(size.x > size.y && size.x > size.z) axis = 0;
    else if(size.y > size.z) axis = 1;

    // Loosely following PBRT - some performance optimizations have been done
    constexpr int bucket_count = 12;
    struct bucket_info
    {
        aabb bounds = {vec3(FLT_MAX), vec3(-FLT_MAX)};
        int count = 0;
    };
    bucket_info buckets[bucket_count];
    vec3 inv_size = vec3(bucket_count) / size;
    for(uint32_t i = 0; i < node_count; ++i)
    {
        node& n  = node_data[i];
        int bucket_index = (n.center[axis] - group_aabb.min[axis]) * inv_size[axis];
        bucket_index = clamp(bucket_index, 0, bucket_count-1);
        bucket_info& bucket = buckets[bucket_index];
        bucket.count++;
        bucket.bounds = {
            min(bucket.bounds.min, vec3(n.center)-vec3(n.radius)),
            max(bucket.bounds.min, vec3(n.center)+vec3(n.radius))
        };
    }

    bucket_info bucket_ascending[bucket_count];
    bucket_info bucket_descending[bucket_count];
    bucket_info prev_ascending;
    bucket_info prev_descending;
    for(int i = 0; i < bucket_count; ++i)
    {
        bucket_ascending[i].bounds.min = min(
            buckets[i].bounds.min, prev_ascending.bounds.min);
        bucket_ascending[i].bounds.max = max(
            buckets[i].bounds.max, prev_ascending.bounds.max);
        bucket_ascending[i].count = prev_ascending.count + buckets[i].count;
        prev_ascending = bucket_ascending[i];

        int j = bucket_count-1-i;
        bucket_descending[j].bounds.min = min(
            buckets[j].bounds.min, prev_descending.bounds.min);
        bucket_descending[j].bounds.max = max(
            buckets[j].bounds.max, prev_descending.bounds.max);
        bucket_descending[j].count = prev_descending.count + buckets[j].count;
        prev_descending = bucket_descending[j];
    }

    float min_cost = FLT_MAX;
    int min_cost_split = 0;
    for(int i = 0; i < bucket_count - 1; ++i)
    {
        aabb bounds0 = bucket_ascending[i].bounds;
        aabb bounds1 = bucket_descending[i+1].bounds;
        int count0 = bucket_ascending[i].count;
        int count1 = bucket_descending[i+1].count;
        vec3 size0 = bounds0.max - bounds0.min;
        vec3 size1 = bounds1.max - bounds1.min;

        float area0 = size0.x * size0.y + size0.z * size0.x + size0.y * size0.z;
        float area1 = size1.x * size1.y + size1.z * size1.x + size1.y * size1.z;
        float cost = count0 * area0 + count1 * area1;
        if(cost < min_cost)
        {
            min_cost = cost;
            min_cost_split = i;
        }
    }

    float split = float(min_cost_split+1)/bucket_count * size[axis] +
        group_aabb.min[axis];

    uint32_t own_index = nodes.size();
    nodes.push_back({
        (group_aabb.max + group_aabb.min) * 0.5f, own_index, size * 0.5f,
        int8_t(axis), 0, mask
    });

    size_t split_count = std::partition(
        node_data, node_data+node_count,
        [&](const node& n) {return n.center[axis] < split;}
    ) - node_data;

    if(split_count == 0 || split_count == node_count)
        split_count = node_count/2;

    build_recursive_sah(node_data, split_count);
    nodes[own_index].child_offset = nodes.size();
    build_recursive_sah(node_data+split_count, node_count-split_count);
}

template<typename T>
template<typename F>
void bvh<T>::query(vec3 point, F&& on_overlap, uint16_t mask) const
{
    point_traverse(0, point, mask, std::forward<F>(on_overlap));
}

template<typename T>
template<typename F>
void bvh<T>::query(ray r, F&& on_intersect, uint16_t mask) const
{
    ray_traverse(0, r.o, 1.0f/r.dir, mask, std::forward<F>(on_intersect));
}

template<typename T>
template<typename F>
void bvh<T>::query(aabb box, F&& on_overlap, uint16_t mask) const
{
    aabb_traverse(0, box, mask, std::forward<F>(on_overlap));
}

template<typename T>
template<typename F, typename U>
void bvh<T>::query(const bvh<U>& other, F&& on_overlap, uint16_t mask, uint16_t mask_other) const
{
    bvh_traverse(0, 0, other, mask, mask_other, std::forward<F>(on_overlap));
}

template<typename T>
template<typename F>
void bvh<T>::foreach(F&& callback, uint16_t mask) const
{
    for(const node& n: nodes)
    {
        if((n.mask & mask) != 0 && n.axis < 0)
            callback(data[n.child_offset], aabb{n.center - n.radius, n.center + n.radius});
    }
}

template<typename T>
bool bvh<T>::refit_subtree(uint32_t index, float& refit_delta)
{
    if(index >= nodes.size()) return false;

    node& n = nodes[index];
    if(n.axis >= 0)
    {
        n.status = 0;
        node old_a = nodes[index+1];
        bool need_refit = refit_subtree(index+1, refit_delta);

        node old_b = nodes[n.child_offset];
        need_refit |= refit_subtree(n.child_offset, refit_delta);
        refit_delta *= 0.5f;

        if(need_refit)
        {
            node& a = nodes[index+1];
            node& b = nodes[n.child_offset];
            aabb a_old_aabb = {old_a.center - old_a.radius, old_a.center + old_a.radius};
            aabb b_old_aabb = {old_b.center - old_b.radius, old_b.center + old_b.radius};
            aabb a_new_aabb = {a.center - a.radius, a.center + a.radius};
            aabb b_new_aabb = {b.center - b.radius, b.center + b.radius};
            vec3 min_corner = min(a_new_aabb.min, b_new_aabb.min);
            vec3 max_corner = max(a_new_aabb.max, b_new_aabb.max);
            vec3 old_radius = n.radius;

            n.center = (min_corner + max_corner) * 0.5f;
            n.radius = pvec3(max_corner) - n.center;
            n.mask = a.mask | b.mask;

            float old_volume = 8 * old_radius.x * old_radius.y * old_radius.z;
            float new_volume = 8 * n.radius.x * n.radius.y * n.radius.z;
            float old_overlap = aabb_overlap_volume(a_old_aabb, b_old_aabb);
            float new_overlap = aabb_overlap_volume(a_new_aabb, b_new_aabb);
            float relative_delta_overlap = new_overlap/new_volume - old_overlap/old_volume;

            refit_delta += relative_delta_overlap;
        }
        return need_refit;
    }
    else if(n.status)
    {
        n.status = 0;
        return true;
    }
    return false;
}

template<typename T>
template<typename F>
void bvh<T>::point_traverse(uint32_t index, vec3 point, uint16_t mask, F&& on_overlap) const
{
    if(index >= nodes.size()) return;

    const node& n = nodes[index];
    if((n.mask & mask) != 0 && all(lessThanEqual(abs(point-vec3(n.center)), vec3(n.radius))))
    {
        if(n.axis < 0)
            on_overlap(data[n.child_offset]);
        else
        {
            point_traverse(index+1, point, mask, std::forward<F>(on_overlap));
            point_traverse(n.child_offset, point, mask, std::forward<F>(on_overlap));
        }
    }
}

template<typename T>
template<typename F>
bool bvh<T>::ray_traverse(uint32_t index, vec3 o, vec3 inv_dir, uint16_t mask, F&& on_intersect) const
{
    if(index >= nodes.size()) return false;

    const node& no = nodes[index];
    if((no.mask & mask) == 0) return false;

    vec3 n = inv_dir * (o-no.center);
    vec3 k = abs(inv_dir) * no.radius;
    vec3 t1 = -n - k;
    vec3 t2 = -n + k;
    float near = max(max(t1.x, t1.y), t1.z);
    float far = min(min(t2.x, t2.y), t2.z);
    if(near < far && far > 0.0f)
    {
        if(no.axis < 0)
        {
            if(on_intersect(data[no.child_offset]))
                return true;
        }
        else if(inv_dir[no.axis] < 0)
        {
            if(ray_traverse(no.child_offset, o, inv_dir, mask, std::forward<F>(on_intersect)))
                return true;
            if(ray_traverse(index+1, o, inv_dir, mask, std::forward<F>(on_intersect)))
                return true;
        }
        else
        {
            if(ray_traverse(index+1, o, inv_dir, mask, std::forward<F>(on_intersect)))
                return true;
            if(ray_traverse(no.child_offset, o, inv_dir, mask, std::forward<F>(on_intersect)))
                return true;
        }
    }
    return false;
}

template<typename T>
template<typename F>
void bvh<T>::aabb_traverse(uint32_t index, aabb box, uint16_t mask, F&& on_overlap) const
{
    if(index >= nodes.size()) return;

    const node& n = nodes[index];
    aabb node_box = {n.center - n.radius, n.center + n.radius};
    if((n.mask & mask) != 0 && aabb_overlap(node_box, box))
    {
        if(n.axis < 0)
            on_overlap(data[n.child_offset]);
        else
        {
            aabb_traverse(index+1, box, mask, std::forward<F>(on_overlap));
            aabb_traverse(n.child_offset, box, mask, std::forward<F>(on_overlap));
        }
    }
}

template<typename T>
template<typename F, typename U>
void bvh<T>::bvh_traverse(uint32_t index_self, uint32_t index_other, const bvh<U>& other, uint16_t mask, uint16_t mask_other, F&& on_overlap) const
{
    if(index_self >= nodes.size() || index_other >= other.nodes.size()) return;

    const node& a = nodes[index_self];
    const node& b = other.nodes[index_other];
    if((a.mask & mask) == 0 || (b.mask & mask) == 0) return;

    if(all(lessThanEqual(max(a.center - a.radius, b.center - b.radius), min(a.center + a.radius, b.center + b.radius))))
    { // Top AABBs overlap.
        if(a.axis < 0 && b.axis < 0)
        {
            on_overlap(data[a.child_offset], other.data[b.child_offset]);
        }
        else if(a.axis < 0)
        {
            bvh_traverse(index_self, index_other+1, other, mask, mask_other, std::forward<F>(on_overlap));
            bvh_traverse(index_self, b.child_offset, other, mask, mask_other, std::forward<F>(on_overlap));
        }
        else if(b.axis < 0)
        {
            bvh_traverse(index_self+1, index_other, other, mask, mask_other, std::forward<F>(on_overlap));
            bvh_traverse(a.child_offset, index_other, other, mask, mask_other, std::forward<F>(on_overlap));
        }
        else
        {
            bvh_traverse(index_self+1, index_other+1, other, mask, mask_other, std::forward<F>(on_overlap));
            bvh_traverse(index_self+1, b.child_offset, other, mask, mask_other, std::forward<F>(on_overlap));
            bvh_traverse(a.child_offset, b.child_offset, other, mask, mask_other, std::forward<F>(on_overlap));
            bvh_traverse(a.child_offset, index_other+1, other, mask, mask_other, std::forward<F>(on_overlap));
        }
    }
}

}

#endif
