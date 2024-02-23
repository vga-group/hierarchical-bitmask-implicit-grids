#ifndef RAYBASE_MATH_TCC
#define RAYBASE_MATH_TCC
#include <numeric>

namespace rb
{

template<length_t L, typename T, glm::qualifier Q>
T vecmax(const vec<L, T, Q>& v)
{
    T m = v[0];
    for(length_t i = 1; i < L; ++i) m = max(m, v[i]);
    return m;
}

template<length_t L, typename T, glm::qualifier Q>
T vecmin(const vec<L, T, Q>& v)
{
    T m = v[0];
    for(length_t i = 1; i < L; ++i) m = min(m, v[i]);
    return m;
}


template<typename T>
struct glm_vecsize_helper_struct {};

template<length_t L, typename T, glm::qualifier Q>
constexpr length_t glm_vecsize_helper(glm_vecsize_helper_struct<vec<L, T, Q>>)
{
    return L;
}

template<typename VecType>
constexpr unsigned glm_vecsize()
{
    return glm_vecsize_helper(glm_vecsize_helper_struct<VecType>{});
}

template<typename T, glm::qualifier Q>
vec<2, T, Q> vecsort(vec<2, T, Q> v)
{
    if(v[0] > v[1]) std::swap(v[0], v[1]);
    return v;
}

template<typename T, glm::qualifier Q>
vec<3, T, Q> vecsort(vec<3, T, Q> v)
{
    if(v[0] > v[2]) std::swap(v[0], v[2]);
    if(v[0] > v[1]) std::swap(v[0], v[1]);
    if(v[1] > v[2]) std::swap(v[1], v[2]);
    return v;
}

template<typename T, glm::qualifier Q>
vec<4, T, Q> vecsort(vec<4, T, Q> v)
{
    if(v[0] > v[2]) std::swap(v[0], v[2]);
    if(v[1] > v[3]) std::swap(v[1], v[3]);
    if(v[0] > v[1]) std::swap(v[0], v[1]);
    if(v[2] > v[3]) std::swap(v[2], v[3]);
    if(v[1] > v[2]) std::swap(v[1], v[2]);
    return v;
}

template<typename T>
T cubic_spline(T p1, T m1, T p2, T m2, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    float tmp = 2 * t3 - 3 * t2;
    return
        (tmp + 1) * p1 +
        (t3 - 2 * t2 + t) * m1 +
        (-tmp) * p2 +
        (t3 - t2) * m2;
}

template<typename T>
void wrap_convolve(
    const T* input_image,
    T* output_image,
    uvec2 size,
    const float* kernel,
    int kernel_size
){
    std::unique_ptr<T[]> temp_image(new T[size.x * size.y]);
    // Convolve horizontally input_image -> temp_image
    for(unsigned y = 0; y < size.y; ++y)
    for(unsigned x = 0; x < size.x; ++x)
    {
        T res = T(0);
        int i = int(x) - kernel_size/2;
        while(i < 0) i += size.x;
        for(int k = 0; k < kernel_size; ++k, ++i)
        {
            if(i >= size.x) i -= size.x;
            res += kernel[k] * input_image[y * size.x + i];
        }
        temp_image[y + x * size.y] = res;
    }
    // Convolve vertically temp_image -> output_image
    for(unsigned x = 0; x < size.x; ++x)
    for(unsigned y = 0; y < size.y; ++y)
    {
        T res = T(0);
        int i = int(y) - kernel_size/2;
        while(i < 0) i += size.y;
        for(int k = 0; k < kernel_size; ++k, ++i)
        {
            if(i >= size.y) i -= size.y;
            res += kernel[k] * temp_image[i + x * size.y];
        }
        output_image[y * size.x + x] = res;
    }
}

// https://gamedev.net/forums/topic/695570-divide-and-conquer-broadphase/5373251/
template<typename T, typename F1, typename F2>
void aabb_group_overlap_internal(
    uint32_t a_count,
    const T* a_entries,
    uint32_t* a_indices,
    uint32_t b_count,
    const T* b_entries,
    uint32_t* b_indices,
    F1&& get_aabb,
    F2&& report_overlap
){
    if(a_count == 0 || b_count == 0) return;
    if(a_count == 1)
    {
        aabb a_aabb = get_aabb(a_entries[a_indices[0]]);
        for(uint32_t j = 0; j < b_count; ++j)
        {
            aabb b_aabb = get_aabb(b_entries[b_indices[j]]);
            if(aabb_overlap(a_aabb, b_aabb))
                report_overlap(a_entries[a_indices[0]], b_entries[b_indices[j]]);
        }
    }
    else if(b_count == 1)
    {
        aabb b_aabb = get_aabb(b_entries[b_indices[0]]);
        for(uint32_t i = 0; i < a_count; ++i)
        {
            aabb a_aabb = get_aabb(a_entries[a_indices[i]]);
            if(aabb_overlap(a_aabb, b_aabb))
                report_overlap(a_entries[a_indices[i]], b_entries[b_indices[0]]);
        }
    }
    else
    {
        aabb group_aabb = {vec3(FLT_MAX), vec3(-FLT_MAX)};
        for(uint32_t i = 0; i < a_count; ++i)
        {
            aabb bb = get_aabb(a_entries[a_indices[i]]);
            group_aabb.min = min(group_aabb.min, bb.min);
            group_aabb.max = max(group_aabb.max, bb.max);
        }
        for(uint32_t j = 0; j < b_count; ++j)
        {
            aabb bb = get_aabb(b_entries[b_indices[j]]);
            group_aabb.min = min(group_aabb.min, bb.min);
            group_aabb.max = max(group_aabb.max, bb.max);
        }

        vec3 size = group_aabb.max - group_aabb.min;
        // TODO: Better split heuristic
        int axis = 2;
        if(size.x > size.y && size.x > size.z) axis = 0;
        else if(size.y > size.z) axis = 1;
        float split = (group_aabb.min[axis] + group_aabb.max[axis]) * 0.5f;

        uint32_t* a_indices_end = std::partition(
            a_indices, a_indices+a_count,
            [&](uint32_t index) { return get_aabb(a_entries[index]).min[axis] <= split; }
        );
        uint32_t* b_indices_end = std::partition(
            b_indices, b_indices+b_count,
            [&](uint32_t index) { return get_aabb(b_entries[index]).min[axis] <= split; }
        );
        if(a_indices_end == a_indices+a_count && b_indices_end == b_indices+b_count)
        {
            for(uint32_t i = 0; i < a_count; ++i)
            {
                aabb a_aabb = get_aabb(a_entries[a_indices[i]]);
                for(uint32_t j = 0; j < b_count; ++j)
                {
                    aabb b_aabb = get_aabb(b_entries[b_indices[j]]);
                    if(aabb_overlap(a_aabb, b_aabb))
                        report_overlap(a_entries[a_indices[i]], b_entries[b_indices[j]]);
                }
            }
            return;
        }
        else
        {
            aabb_group_overlap_internal(
                a_indices_end-a_indices,
                a_entries,
                a_indices,
                b_indices_end-b_indices,
                b_entries,
                b_indices,
                std::forward<F1>(get_aabb),
                std::forward<F2>(report_overlap)
            );
        }

        a_indices_end = std::partition(
            a_indices, a_indices+a_count,
            [&](uint32_t index) { return get_aabb(a_entries[index]).max[axis] >= split; }
        );
        b_indices_end = std::partition(
            b_indices, b_indices+b_count,
            [&](uint32_t index) { return get_aabb(b_entries[index]).max[axis] >= split; }
        );
        if(a_indices_end == a_indices+a_count && b_indices_end == b_indices+b_count)
        {
            for(uint32_t i = 0; i < a_count; ++i)
            {
                aabb a_aabb = get_aabb(a_entries[a_indices[i]]);
                for(uint32_t j = 0; j < b_count; ++j)
                {
                    aabb b_aabb = get_aabb(b_entries[b_indices[j]]);
                    if(aabb_overlap(a_aabb, b_aabb))
                        report_overlap(a_entries[a_indices[i]], b_entries[b_indices[j]]);
                }
            }
        }
        else
        {
            aabb_group_overlap_internal(
                a_indices_end-a_indices,
                a_entries,
                a_indices,
                b_indices_end-b_indices,
                b_entries,
                b_indices,
                std::forward<F1>(get_aabb),
                std::forward<F2>(report_overlap)
            );
        }
    }
}

template<typename T, typename F1, typename F2>
void aabb_group_overlap(
    const std::vector<T>& a,
    const std::vector<T>& b,
    F1&& get_aabb,
    F2&& report_overlap,
    uint32_t* scratch
){
    uint32_t* local_scratch = nullptr;
    if(!scratch)
    {
        local_scratch = new uint32_t[a.size() + b.size()];
        scratch = local_scratch;
    }
    uint32_t* a_indices = scratch;
    uint32_t* b_indices = scratch + a.size();
    std::iota(a_indices, a_indices+a.size(), 0);
    std::iota(b_indices, b_indices+b.size(), 0);
    aabb_group_overlap_internal(
        a.size(), a.data(), a_indices,
        b.size(), b.data(), b_indices,
        std::forward<F1>(get_aabb),
        std::forward<F2>(report_overlap)
    );
    if(local_scratch) delete [] local_scratch;
}

template<typename Key>
std::vector<Key> set_intersection(
    const std::vector<Key>& a,
    const std::vector<Key>& b
){
    std::vector<Key> intersection;
    size_t i = 0;
    size_t j = 0;
    while(i < a.size() && j < b.size())
    {
        if(a[i] == b[j])
        {
            intersection.push_back(a[i]);
            while(a[i] == b[j] && i < a.size()) ++i;
            if(i >= a.size()) break;
            while(b[j] < a[i] && j < b.size()) ++j;
        }
        else if(a[i] < b[j]) i++;
        else j++;
    }
    return intersection;
}

template <class T>
void hash_combine(std::size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

template<typename A, glm::length_t L, typename T, glm::qualifier Q>
void save(A& a, const glm::vec<L, T, Q>& t)
{
    const T* ptr = glm::value_ptr(t);
    for(glm::length_t i = 0; i < L; ++i)
        a.serialize(ptr[i]);
}

template<typename A, glm::length_t C, glm::length_t R, typename T>
void save(A& a, const glm::mat<C, R, T, glm::packed_highp>& t)
{
    const T* ptr = glm::value_ptr(t);
    for(glm::length_t i = 0; i < C*R; ++i)
        a.serialize(ptr[i]);
}

template<typename A, glm::length_t C, glm::length_t R, typename T>
void save(A& a, const glm::mat<C, R, T, glm::aligned_highp>& t)
{
    glm::mat<C, R, T, glm::packed_highp> temp = t;
    save(a, temp);
}

template<typename A, typename T, glm::qualifier Q>
void save(A& a, const glm::qua<T, Q>& t)
{
    const T* ptr = glm::value_ptr(t);
    for(glm::length_t i = 0; i < 4; ++i)
        a.serialize(ptr[i]);
}

template<typename A, length_t L, typename T, glm::qualifier Q>
bool load(A& a, glm::vec<L, T, Q>& t)
{
    T* ptr = glm::value_ptr(t);
    bool result = true;
    for(glm::length_t i = 0; i < L; ++i)
        result &= a.serialize(ptr[i]);
    return result;
}

template<typename A, glm::length_t C, glm::length_t R, typename T>
bool load(A& a, glm::mat<C, R, T, glm::packed_highp>& t)
{
    T* ptr = glm::value_ptr(t);
    bool result = true;
    for(glm::length_t i = 0; i < C*R; ++i)
        result &= a.serialize(ptr[i]);
    return result;
}

template<typename A, glm::length_t C, glm::length_t R, typename T>
bool load(A& a, glm::mat<C, R, T, glm::aligned_highp>& t)
{
    glm::mat<C, R, T, glm::packed_highp> temp;
    bool ret = load(a, temp);
    t = temp;
    return ret;
}

template<typename A, typename T, glm::qualifier Q>
bool load(A& a, glm::qua<T, Q>& t)
{
    T* ptr = glm::value_ptr(t);
    bool result = true;
    for(glm::length_t i = 0; i < 4; ++i)
        result &= load(a, ptr[i]);
    return result;
}

}

#endif
