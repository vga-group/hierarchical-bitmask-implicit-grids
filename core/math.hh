#ifndef RAYBASE_MATH_HH
#define RAYBASE_MATH_HH
#define GLM_ENABLE_EXPERIMENTAL
// Makes GLM angles predictable
#define GLM_FORCE_RADIANS
#define GLM_FORCE_INTRINSICS
#define GLM_FORCE_SSE2
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define _USE_MATH_DEFINES
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/integer.hpp>
#include <string>
#include <vector>
#include <complex>
#include <memory>

#if GLM_VERSION != 998
#pragma warning "This program was written to use GLM 0.9.9.8. " \
    GLM_VERSION_MESSAGE
#endif

namespace rb
{

// Because of our SSE forcing, the default vec2, vec3 and vec4 types are
// aligned to 4. Practically, this means that vec3 is too large. If for memory
// reasons packed vectors are desired, use the following types.
using pvec4 = glm::vec<4, float, glm::packed_highp>;
using pvec3 = glm::vec<3, float, glm::packed_highp>;
using pvec2 = glm::vec<2, float, glm::packed_highp>;
using pivec4 = glm::vec<4, int, glm::packed_highp>;
using pivec3 = glm::vec<3, int, glm::packed_highp>;
using pivec2 = glm::vec<2, int, glm::packed_highp>;
using puvec4 = glm::vec<4, unsigned, glm::packed_highp>;
using puvec3 = glm::vec<3, unsigned, glm::packed_highp>;
using puvec2 = glm::vec<2, unsigned, glm::packed_highp>;
using pi16vec4 = glm::vec<4, int16_t, glm::packed_highp>;
using pi16vec3 = glm::vec<3, int16_t, glm::packed_highp>;
using pi16vec2 = glm::vec<2, int16_t, glm::packed_highp>;
using pu16vec4 = glm::vec<4, uint16_t, glm::packed_highp>;
using pu16vec3 = glm::vec<3, uint16_t, glm::packed_highp>;
using pu16vec2 = glm::vec<2, uint16_t, glm::packed_highp>;
using pmat4 = glm::mat<4, 4, float, glm::packed_highp>;
using pmat3 = glm::mat<3, 3, float, glm::packed_highp>;
using pmat2 = glm::mat<2, 2, float, glm::packed_highp>;

using namespace glm;

template<length_t L, typename T, glm::qualifier Q>
T vecmax(const vec<L, T, Q>& v);

template<length_t L, typename T, glm::qualifier Q>
T vecmin(const vec<L, T, Q>& v);

template<typename VecType>
constexpr unsigned glm_vecsize();

template<typename T, glm::qualifier Q>
vec<2, T, Q> vecsort(vec<2, T, Q> v);

template<typename T, glm::qualifier Q>
vec<3, T, Q> vecsort(vec<3, T, Q> v);

template<typename T, glm::qualifier Q>
vec<4, T, Q> vecsort(vec<4, T, Q> v);

bool point_in_rect(vec2 p, vec2 o, vec2 sz);
bool point_in_triangle(vec2 a, vec2 b, vec2 c, vec2 p);
vec3 barycentric(vec2 a, vec2 b, vec2 c, vec2 p);
float signed_area(vec2 a, vec2 b, vec2 c);

vec3 hsv_to_rgb(vec3 hsv);
float circle_sequence(unsigned n);
vec3 generate_color(int32_t index, float saturation = 1.0f, float value = 1.0f);

// Doesn't work correctly with shearing.
void decompose_matrix(
    const mat4& transform,
    vec3& translation,
    vec3& scaling,
    quat& orientation
);

vec3 get_matrix_translation(const mat4& transform);
vec3 get_matrix_scaling(const mat4& transform);
quat get_matrix_orientation(const mat4& transform);
mat4 remove_matrix_scaling(const mat4& transform);

quat rotate_towards(
    quat orig,
    quat dest,
    float angle_limit
);

quat quat_lookat(
    vec3 dir,
    vec3 up,
    vec3 forward = vec3(0,0,-1)
);

bool solve_quadratic(float a, float b, float c, float& x0, float& x1);
void solve_cubic_roots(
    double a, double b, double c, double d,
    std::complex<double>& r1,
    std::complex<double>& r2,
    std::complex<double>& r3
);

double cubic_bezier(dvec2 p1, dvec2 p2, double t);
template<typename T>
T cubic_spline(T p1, T m1, T p2, T m2, float t);

struct sphere
{
    vec3 origin;
    float radius;
};

bool intersect_sphere(
    vec3 pos,
    vec3 dir,
    const sphere& s,
    float& t0,
    float& t1
);

unsigned ilog2(unsigned n);
bool is_power_of_two(unsigned n);
unsigned next_power_of_two(unsigned n);
uint32_t align_up_to(uint32_t n, uint32_t align);
unsigned factorize(unsigned n);

// Computes a modelview matrix for a quad such that it completely covers the
// surface area of a sphere. use_near_radius determines whether the resulting
// depth value is picked from the near edge of the sphere or the furthest edge.
mat4 sphere_projection_quad_matrix(
    const sphere& s,
    float near,
    float far,
    bool use_near_radius = false,
    float big = 1e3
);

// glm::ortho, but with reversed Z
mat4 reverse_z_ortho(
    float left, float right,
    float bottom, float top,
    float near, float far
);

mat4 reverse_z_perspective(float fov, float aspect, float near);
mat4 reverse_z_perspective(float fov, float aspect, float near, float far);

// Note that this function is very slow. Please save your generated samples.
// If samples already contain some values, they're assumed to be generated by
// a previous call to this function with the same r.
// Circular version
void mitchell_best_candidate(
    std::vector<vec2>& samples,
    float r,
    unsigned candidate_count,
    unsigned count
);

// Rectangular version
void mitchell_best_candidate(
    std::vector<vec2>& samples,
    float w,
    float h,
    unsigned candidate_count,
    unsigned count
);

// Spherical version
void mitchell_best_candidate(
    std::vector<vec3>& samples,
    float r,
    unsigned candidate_count,
    unsigned count
);

std::vector<vec2> grid_samples(
    unsigned w,
    unsigned h,
    float step
);

std::vector<float> generate_gaussian_kernel(
    int radius,
    float sigma
);

template<typename T>
void wrap_convolve(
    const T* input_image,
    T* output_image,
    uvec2 size,
    const float* kernel,
    int kernel_size
);

// This method is surprisingly fast (~30 milliseconds for 256x256 vec4 lookup
// texture), so you can render them during loading time. The quality _is worse
// than void-and-cluster_, but the speed is no contest and this is still fairly
// good in practice.
template<typename T>
std::vector<T> generate_blue_noise(
    uvec2 size,
    uint32_t seed = 0,
    int gauss_radius=3,
    float gauss_sigma=1.1,
    int iterations=5
);

vec3 pitch_yaw_to_vec(float pitch, double yaw);

uvec2 string_to_resolution(const std::string& str);

unsigned calculate_mipmap_count(uvec2 size);

// axis-aligned bounding box
struct aabb
{
    vec3 min;
    vec3 max;
};

struct frustum
{
    vec4 planes[6];
};

// Assumes affine transform!
struct frustum operator*(const mat4& mat, const struct frustum& f);

// These functions have some false negatives, so they sometimes fail to cull
// even when it would be possible.
bool obb_frustum_cull(
    const aabb& box,
    const mat4& transform,
    const struct frustum& f
);

bool aabb_frustum_cull(const aabb& box, const struct frustum& f);
bool sphere_frustum_cull(const sphere& s, const struct frustum& f);

aabb aabb_from_obb(const aabb& box, const mat4& transform);
bool aabb_overlap(const aabb& a, const aabb& b);
bool aabb_contains(const aabb& a, const vec3& p);
float aabb_distance(const aabb& a, const vec3& p);
float aabb_overlap_volume(const aabb& a, const aabb& b);

// With magic (BVH), this function should pretty much be O(NlogN+MlogM) (naive
// solution would be O(NM)).
// report_overlap(T a, T b) may report duplicates.
// get_aabb(T a) should return the AABB for its parameter.
// scratch_memory should either be NULL or point to an array of
// uint32_t[a.size()+b.size()].
template<typename T, typename F1, typename F2>
void aabb_group_overlap(
    const std::vector<T>& a,
    const std::vector<T>& b,
    F1&& get_aabb,
    F2&& report_overlap,
    uint32_t* scratch_memory = nullptr
);

unsigned ravel_tex_coord(uvec3 p, uvec3 size);

struct ray
{
    vec3 o;
    vec3 dir;
};

ray operator*(const mat4& mat, const ray& r);
vec2 ray_aabb_intersect(const aabb& a, const ray& r);

// This function simply checks if the given matrix causes the winding order
// of triangles in a model to flip.
bool flipped_winding_order(const mat3& transform);

vec2 screen_to_relative_coord(uvec2 size, vec2 p);
vec2 relative_to_screen_coord(uvec2 size, vec2 p);
vec2 screen_to_relative_size(uvec2 size, vec2 p);
vec2 relative_to_screen_size(uvec2 size, vec2 p);

uint32_t rgb_to_rgbe(vec3 color);
vec3 rgbe_to_rgb(uint32_t rgbe);

vec2 octahedral_encode(vec3 normal);
vec3 octahedral_decode(vec2 encoded_normal);

uint32_t morton_encode(uvec3 x);
uvec3 morton_decode(uint32_t m);

// https://www.pcg-random.org/
uint32_t pcg(uint32_t& seed);

// http://www.jcgt.org/published/0009/03/02/
uvec3 pcg3d(uvec3& seed);
uvec4 pcg4d(uvec4& seed);

// Similar to std::set_intersection, but also does de-duplication!
// The input vectors must be in ascending order! (sort with radix_sort, for
// example)
template<typename Key>
std::vector<Key> set_intersection(
    const std::vector<Key>& a,
    const std::vector<Key>& b
);

vec3 closest_point_on_plane(vec3 p, vec4 plane);
vec3 closest_point_on_line(vec3 p, vec3 l0, vec3 l1);
bool point_in_triangle(vec3 p, vec3 c1, vec3 c2, vec3 c3);
vec3 closest_point_on_triangle(vec3 p, vec3 c1, vec3 c2, vec3 c3);
vec3 triangle_barycentrics(vec3 p, vec3 c1, vec3 c2, vec3 c3);

struct tetrahedron
{
    vec3 corners[4];
};

aabb aabb_from_tetrahedron(const tetrahedron& tt);
bool point_in_tetrahedron(const tetrahedron& tt, vec3 p);
vec4 tetrahedron_barycentrics(const tetrahedron& tt, vec3 p);
vec3 closest_point_on_tetrahedron(const tetrahedron& tt, vec3 p);
vec3 tetrahedron_circumcenter(const tetrahedron& tt);

vec3 create_tangent(vec3 normal);
mat3 create_tangent_space(vec3 normal);

template <class T>
void hash_combine(std::size_t& seed, const T& v);

template<typename A, glm::length_t L, typename T, glm::qualifier Q>
void save(A& a, const glm::vec<L, T, Q>& t);

template<typename A, glm::length_t C, glm::length_t R, typename T>
void save(A& a, const glm::mat<C, R, T, glm::packed_highp>& t);

template<typename A, glm::length_t C, glm::length_t R, typename T>
void save(A& a, const glm::mat<C, R, T, glm::aligned_highp>& t);

template<typename A, typename T, glm::qualifier Q>
void save(A& a, const glm::qua<T, Q>& t);

template<typename A, glm::length_t L, typename T, glm::qualifier Q>
bool load(A& a, glm::vec<L, T, Q>& t);

template<typename A, glm::length_t C, glm::length_t R, typename T>
bool load(A& a, glm::mat<C, R, T, glm::packed_highp>& t);

template<typename A, glm::length_t C, glm::length_t R, typename T>
bool load(A& a, glm::mat<C, R, T, glm::aligned_highp>& t);

template<typename A, typename T, glm::qualifier Q>
bool load(A& a, glm::qua<T, Q>& t);

}

#include "math.tcc"
#endif
