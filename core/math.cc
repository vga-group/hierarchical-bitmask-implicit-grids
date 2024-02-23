#include "math.hh"
#include "sort.hh"
#include <sstream>

namespace
{
using namespace rb;

vec2 circle_projection_range(vec2 dir, float r, float p, float big)
{
    float d2 = dot(dir, dir);
    float r2 = r * r;

    if(d2 <= r2) { return vec2(-1, 1) * big; }

    float len = sqrt(d2 - r2);
    vec2 n = dir / dir.y;

    vec2 h(-n.y, n.x);
    h *= r / len;

    vec2 up = n + h;
    float top = up.x / fabs(up.y) * p;

    vec2 down = n - h;
    float bottom = down.x / fabs(down.y) * p;

    if(dir.x > 0 && dir.y <= r)
    {
        bottom = big;
        if(dir.y <= 0) top = -top;
    }

    if(dir.x < 0 && dir.y <= r)
    {
        top = -big;
        if(dir.y <= 0) bottom = -bottom;
    }

    return vec2(top, bottom);
}

}

namespace rb
{

bool point_in_rect(vec2 p, vec2 o, vec2 sz)
{
    return o.x <= p.x && p.x <= o.x + sz.x && o.y <= p.y && p.y <= o.y + sz.y;
}

bool point_in_triangle(vec2 a, vec2 b, vec2 c, vec2 p)
{
    vec3 bc = barycentric(a, b, c, p);
    return bc.x >= 0 && bc.y >= 0 && bc.x + bc.y <= 1;
}

vec3 barycentric(vec2 a, vec2 b, vec2 c, vec2 p)
{
    float inv_area = 0.5f/signed_area(a, b, c);
    float s = (a.y*c.x - a.x*c.y - p.x*(a.y - c.y) + p.y*(a.x - c.x))*inv_area;
    float t = (a.x*b.y - a.y*b.x + p.x*(a.y - b.y) - p.y*(a.x - b.x))*inv_area;
    float u = 1 - s - t;
    return vec3(s, t, u);
}

float signed_area(vec2 a, vec2 b, vec2 c)
{
    return 0.5f * (
        a.x*b.y - b.x*a.y +
        b.x*c.y - c.x*b.y +
        c.x*a.y - a.x*c.y
    );
}

vec3 hsv_to_rgb(vec3 hsv)
{
    vec3 c = vec3(5,3,1) + hsv.x/60.0f;
    vec3 k = vec3(fmod(c.x, 6.0f), fmod(c.y, 6.0f), fmod(c.z, 6.0f));
    return hsv.z - hsv.z*hsv.y*clamp(min(k, 4.0f-k), vec3(0.0f), vec3(1.0f));
}

float circle_sequence(unsigned n)
{
    unsigned denom = n + 1;
    denom--;
    denom |= denom >> 1;
    denom |= denom >> 2;
    denom |= denom >> 4;
    denom |= denom >> 8;
    denom |= denom >> 16;
    denom++;
    unsigned num = 1 + (n - denom/2)*2;
    return num/(float)denom;
}

vec3 generate_color(int32_t index, float saturation, float value)
{
    return hsv_to_rgb(vec3(
        360*circle_sequence(index),
        saturation,
        value
    ));
}

void decompose_matrix(
    const glm::mat4& transform,
    glm::vec3& translation,
    glm::vec3& scaling,
    glm::quat& orientation
){
    translation = transform[3];
    scaling = glm::vec3(
        glm::length(transform[0]),
        glm::length(transform[1]),
        glm::length(transform[2])
    );
    orientation = glm::quat(glm::mat4(
        transform[0]/scaling.x,
        transform[1]/scaling.y,
        transform[2]/scaling.z,
        glm::vec4(0,0,0,1)
    ));
}

glm::vec3 get_matrix_translation(const glm::mat4& transform)
{
    return transform[3];
}

glm::vec3 get_matrix_scaling(const glm::mat4& transform)
{
    return glm::vec3(
        glm::length(transform[0]),
        glm::length(transform[1]),
        glm::length(transform[2])
    );
}

glm::quat get_matrix_orientation(const glm::mat4& transform)
{
    return glm::quat(glm::mat4(
        glm::normalize(transform[0]),
        glm::normalize(transform[1]),
        glm::normalize(transform[2]),
        glm::vec4(0,0,0,1)
    ));
}

mat4 remove_matrix_scaling(const mat4& transform)
{
    mat4 r;
    // Remove scaling
    for(int i = 0; i < 3; ++i)
        r[i] = vec4(normalize(vec3(transform[i])), transform[i].w);
    r[3] = transform[3];
    return r;
}

glm::quat rotate_towards(
    glm::quat orig,
    glm::quat dest,
    float angle_limit
){
    angle_limit = glm::radians(angle_limit);

    float cos_theta = dot(orig, dest);
    if(cos_theta > 0.999999f)
    {
        return dest;
    }

    if(cos_theta < 0)
    {
        orig = orig * -1.0f;
        cos_theta *= -1.0f;
    }

    float theta = acos(cos_theta);
    if(theta < angle_limit) return dest;
    return glm::mix(orig, dest, angle_limit/theta);
}

glm::quat quat_lookat(
    glm::vec3 dir,
    glm::vec3 up,
    glm::vec3 forward
){
    dir = glm::normalize(dir);
    up = glm::normalize(up);
    forward = glm::normalize(forward);

    glm::quat towards = glm::rotation(
        forward,
        glm::vec3(0,0,-1)
    );
    return glm::quatLookAt(dir, up) * towards;
}

bool solve_quadratic(float a, float b, float c, float& x0, float& x1)
{
    float D = b * b - 4 * a * c;
    float sD = sqrt(D) * sign(a);
    float denom = -0.5f/a;
    x0 = (b + sD) * denom;
    x1 = (b - sD) * denom;
    return !std::isnan(sD);
}

void solve_cubic_roots(
    double a, double b, double c, double d,
    std::complex<double>& r1,
    std::complex<double>& r2,
    std::complex<double>& r3
){
    double d1 = 2*b*b*b - 9*a*b*c + 27*a*a*d;
    double d2 = b*b - 3*a*c;
    auto d3 = sqrt(std::complex<double>(d1*d1 - 4*d2*d2*d2));

    double k = 1/(3*a);

    auto p1 = std::pow(0.5*(d1+d3), 1/3.0f);
    auto p2 = std::pow(0.5*(d1-d3), 1/3.0f);

    std::complex<double> c1(0.5, 0.5*sqrt(3));
    std::complex<double> c2(0.5, -0.5*sqrt(3));

    r1 = k*(-b - p1 - p2).real();
    r2 = k*(-b + c1*p1 + c2*p2);
    r3 = k*(-b + c2*p1 + c1*p2);
}

double cubic_bezier(dvec2 p1, dvec2 p2, double t)
{
    // x = (1-t)^3*P0 + 3*(1-t)^2*t*P1 + 3*(1-t)*t^2*P2 + t^3*P3
    //   = (3*P1 - 3*P2 + 1)*t^3 + (-6*P1 + 3*P2)*t^2 + (3*P1)*t
    //   when P0=(0,0) and P3=(1,1)

    std::complex<double> r1;
    std::complex<double> r2;
    std::complex<double> r3;
    solve_cubic_roots(
        3.*p1.x-3.*p2.x+1., 3.*p2.x-6.*p1.x, 3.*p1.x, -t,
        r1, r2, r3
    );

    double xt = r1.real();
    double best = 0;
    if(r1.real() < 0) best = -r1.real();
    else if(r1.real() > 1) best = r1.real()-1;

    if(abs(r2.imag()) < 0.00001)
    {
        double cost = 0;
        if(r2.real() < 0) cost = -r2.real();
        else if(r2.real() > 1) cost = r2.real()-1;
        if(cost < best)
        {
            best = cost;
            xt = r2.real();
        }
    }

    if(abs(r3.imag()) < 0.00001)
    {
        double cost = 0;
        if(r3.real() < 0) cost = -r3.real();
        else if(r3.real() > 1) cost = r3.real()-1;
        if(cost < best)
        {
            best = cost;
            xt = r3.real();
        }
    }

    return (3.*p1.y-3.*p2.y+1.)*xt*xt*xt
        + (3.*p2.y-6.*p1.y)*xt*xt
        + (3.*p1.y)*xt;
}

bool intersect_sphere(
    vec3 pos,
    vec3 dir,
    const sphere& s,
    float& t0,
    float& t1
){
    vec3 L = pos - s.origin;
    float a = dot(dir, dir);
    float b = 2*dot(dir, L);
    float c = dot(L, L) - s.radius * s.radius;

    if(!solve_quadratic(a, b, c, t0, t1)) return false;
    if(t1 < 0) return false;
    if(t0 < 0) t0 = 0;

    return true;
}

unsigned ilog2(unsigned n)
{
    return findMSB(n);
}

bool is_power_of_two(unsigned n)
{
    return (n & (n - 1)) == 0;
}

unsigned next_power_of_two(unsigned n)
{
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}

uint32_t align_up_to(uint32_t n, uint32_t align)
{
    return (n+(align-1))/align*align;
}

unsigned factorize(unsigned n)
{
    // Divisible by two
    if((n&1)==0) return 2;

    unsigned last = floor(sqrt(n));
    for(unsigned i = 3; i <= last; ++i)
        if((n % i) == 0) return i;

    return 0;
}

mat4 sphere_projection_quad_matrix(
    const sphere& s,
    float near,
    float far,
    bool use_near_radius,
    float big
){
    float d = -s.origin.z;

    if(use_near_radius) d = glm::max(d - s.radius, near);
    else d = glm::min(d + s.radius, far);

    vec2 w = circle_projection_range(vec2(s.origin.x, -s.origin.z), s.radius, d, big);
    vec2 h = circle_projection_range(vec2(s.origin.y, -s.origin.z), s.radius, d, big);

    glm::vec2 center = glm::vec2(w.x + w.y, h.x + h.y) / 2.0f;
    glm::vec2 scale = glm::vec2(fabs(w.y - w.x), fabs(h.y - h.x)) / 2.0f;

    return glm::translate(vec3(center, -d)) * glm::scale(vec3(scale, 0));
}

mat4 reverse_z_ortho(
    float left, float right,
    float bottom, float top,
    float near, float far
){
    mat4 res(1);
    res[0][0] = 2.0f / (right - left);
    res[1][1] = 2.0f / (top - bottom);
    res[2][2] = 1.0f / (far - near);
    res[3][0] = -(right + left) / (right - left);
    res[3][1] = -(top + bottom) / (top - bottom);
    res[3][2] = far / (far - near);
    return res;
}

mat4 reverse_z_perspective(float fov, float aspect, float near)
{
    float htan = tan(fov * 0.5);

    mat4 res(0);
    res[0][0] = 1.0f / (aspect * htan);
    res[1][1] = 1.0f / htan;
    res[2][3] = -1.0f;
    res[3][2] = near;
    return res;
}

mat4 reverse_z_perspective(float fov, float aspect, float near, float far)
{
    float htan = tan(fov * 0.5);

    mat4 res(0);
    res[0][0] = 1.0f / (aspect * htan);
    res[1][1] = 1.0f / htan;
    res[2][2] = -near / (near - far);
    res[2][3] = - 1.0f;
    res[3][2] = (far * near) / (far - near);
    return res;
}

template<typename T, class F>
void mitchell_best_candidate(
    std::vector<T>& samples,
    F&& sample_generator,
    unsigned candidate_count,
    unsigned count
){
    if(count < samples.size()) return;

    samples.reserve(count);
    count -= samples.size();

    while(count--)
    {
        T farthest = T(0);
        float farthest_distance = 0;

        for(unsigned i = 0; i < candidate_count; ++i)
        {
            T candidate = sample_generator();
            float candidate_distance = INFINITY;

            for(const T& sample: samples)
            {
                float dist = glm::distance(candidate, sample);
                if(dist < candidate_distance) candidate_distance = dist;
            }

            if(candidate_distance > farthest_distance)
            {
                farthest_distance = candidate_distance;
                farthest = candidate;
            }
        }

        samples.push_back(farthest);
    }
}

void mitchell_best_candidate(
    std::vector<vec2>& samples,
    float r,
    unsigned candidate_count,
    unsigned count
){
    mitchell_best_candidate(
        samples,
        [r](){return glm::diskRand(r);},
        candidate_count,
        count
    );
}

void mitchell_best_candidate(
    std::vector<vec2>& samples,
    float w,
    float h,
    unsigned candidate_count,
    unsigned count
){
    mitchell_best_candidate(
        samples,
        [w, h](){
            return glm::linearRand(glm::vec2(-w/2, -h/2), glm::vec2(w/2, h/2));
        },
        candidate_count,
        count
    );
}

void mitchell_best_candidate(
    std::vector<vec3>& samples,
    float r,
    unsigned candidate_count,
    unsigned count
){
    mitchell_best_candidate(
        samples,
        [r](){return glm::ballRand(r);},
        candidate_count,
        count
    );
}

std::vector<vec2> grid_samples(
    unsigned w,
    unsigned h,
    float step
){
    std::vector<vec2> samples;
    samples.resize(w*h);

    glm::vec2 start(
        (w-1)/-2.0f,
        (h-1)/-2.0f
    );

    for(unsigned i = 0; i < h; ++i)
        for(unsigned j = 0; j < w; ++j)
            samples[i*w+j] = start + vec2(i, j) * step;

    return samples;
}

std::vector<float> generate_gaussian_kernel(
    int radius,
    float sigma
){
    std::vector<float> result;
    result.reserve(radius * 2 + 1);

    for(int i = -radius; i <= radius; ++i)
    {
        float f = i/sigma;
        float weight = exp(-f*f/2.0f)/(sigma * sqrt(2*M_PI));
        result.push_back(weight);
    }
    return result;
}

vec3 pitch_yaw_to_vec(float pitch, double yaw)
{
    pitch = glm::radians(pitch);
    yaw = glm::radians(yaw);
    float c = cos(pitch);
    return vec3(c * cos(yaw), sin(pitch), c * sin(-yaw));
}

uvec2 string_to_resolution(const std::string& str)
{
    std::stringstream ss(str);
    uvec2 res;
    ss >> res.x;
    ss.ignore(1);
    ss >> res.y;
    if(!ss) return uvec2(640, 360);
    return res;
}

unsigned calculate_mipmap_count(uvec2 size)
{
    return (unsigned)std::floor(std::log2(std::max(size.x, size.y)))+1u;
}

struct frustum operator*(const mat4& mat, const struct frustum& f)
{
    struct frustum res = f;
    mat4 m = glm::transpose(glm::affineInverse(mat));
    for(vec4& p: res.planes) p = m * p;
    return res;
}

bool obb_frustum_intersection(
    const aabb& box,
    const mat4& transform,
    const struct frustum& f
){
    struct frustum tf = f;
    mat4 m = glm::transpose(transform);
    for(vec4& p: tf.planes) p = m * p;
    return aabb_frustum_cull(box, tf);
}

bool aabb_frustum_cull(const aabb& box, const struct frustum& f)
{
    for(vec4 p: f.planes)
    {
        if(
            dot(p, vec4(box.min, 1.0f)) < 0 &&
            dot(p, vec4(box.min.x, box.min.y, box.max.z, 1.0f)) < 0 &&
            dot(p, vec4(box.min.x, box.max.y, box.min.z, 1.0f)) < 0 &&
            dot(p, vec4(box.min.x, box.max.y, box.max.z, 1.0f)) < 0 &&
            dot(p, vec4(box.max.x, box.min.y, box.min.z, 1.0f)) < 0 &&
            dot(p, vec4(box.max.x, box.min.y, box.max.z, 1.0f)) < 0 &&
            dot(p, vec4(box.max.x, box.max.y, box.min.z, 1.0f)) < 0 &&
            dot(p, vec4(box.max, 1.0f)) < 0
        ) return false;
    }

    return true;
}

bool sphere_frustum_cull(const sphere& s, const struct frustum& f)
{
    for(vec4 p: f.planes)
    {
        if(dot(p, vec4(s.origin, 1.0f)) < -s.radius)
            return false;
    }

    return true;
}

aabb aabb_from_obb(const aabb& box, const mat4& transform)
{ // https://zeux.io/2010/10/17/aabb-from-obb-with-component-wise-abs/
    vec3 center = (box.min + box.max) * 0.5f;
    vec3 extent = (box.max - box.min) * 0.5f;

    mat4 abs_transform = mat4(
        abs(transform[0]),
        abs(transform[1]),
        abs(transform[2]),
        abs(transform[3])
    );

    center = transform * vec4(center, 1);
    extent = abs_transform * vec4(extent, 0);
    return aabb{center-extent, center+extent};
}

bool aabb_overlap(const aabb& a, const aabb& b)
{
    return all(lessThanEqual(max(a.min, b.min), min(a.max, b.max)));
}

bool aabb_contains(const aabb& a, const vec3& p)
{
    return all(greaterThanEqual(p, a.min)) && all(lessThanEqual(p, a.max));
}

float aabb_distance(const aabb& a, const vec3& p)
{
    vec3 c = a.min * 0.5f + a.max * 0.5f;
    vec3 b = a.max - c;
    vec3 q = abs(p - c) - b;
    return length(max(q, vec3(0.0))) + min(max(q.x,max(q.y,q.z)),0.0f);
}

float aabb_overlap_volume(const aabb& a, const aabb& b)
{
    aabb intersection = {max(a.min, b.min), min(a.max, b.max)};
    if(all(lessThanEqual(intersection.min, intersection.max)))
    {
        vec3 size = intersection.max-intersection.min;
        return size.x * size.y * size.z;
    }
    return 0.0f;
}

vec2 ray_aabb_intersection(const aabb& a, const ray& r)
{ // https://iquilezles.org/articles/intersectors/
    vec3 m = 1.0f/r.dir;
    vec3 n = m*(r.o-(a.max+a.min)*0.5f);
    vec3 k = abs(m)*(a.max - a.min);
    vec3 t1 = -n - k;
    vec3 t2 = -n + k;
    float near = max(max(t1.x, t1.y), t1.z);
    float far = min(min(t2.x, t2.y), t2.z);
    if(near > far || far < 0.0f)
        return vec2(-1.0f);
    return vec2(near, far);
}

unsigned ravel_tex_coord(uvec3 p, uvec3 size)
{
    return p.z * size.x * size.y + p.y * size.x + p.x;
}

ray operator*(const mat4& mat, const ray& r)
{
    ray res;
    res.o = mat * vec4(r.o, 1.0f);
    res.dir = inverseTranspose(mat) * vec4(r.dir, 0);
    return res;
}

bool flipped_winding_order(const mat3& transform)
{
    return determinant(transform) < 0;
}

vec2 screen_to_relative_coord(uvec2 size, vec2 p)
{
    p /= size;
    p -= vec2(0.5f, 0.5f);
    p *= vec2(2.0f*(size.x/(float)size.y), -2.0f);
    return p;
}

vec2 relative_to_screen_coord(uvec2 size, vec2 p)
{
    p *= vec2(0.5f*(size.y/(float)size.x), -0.5f);
    p += vec2(0.5f, 0.5f);
    p *= size;
    return p;
}

vec2 screen_to_relative_size(uvec2 size, vec2 p)
{
    return p * vec2(2.0f*(size.x/(float)size.y), -2.0f) / vec2(size);
}

vec2 relative_to_screen_size(uvec2 size, vec2 p)
{
    return p * vec2(0.5f*(size.y/(float)size.x), -0.5f) * vec2(size);
}

uint32_t rgb_to_rgbe(vec3 color)
{
    ivec3 exp;
    frexp(color, exp);
    int e = max(exp.r, max(exp.g, exp.b));
    ivec4 icolor = clamp(ivec4(
        round(ldexp(color, ivec3(-e)) * 256.0f),
        e+128
    ), 0, 255);
    return icolor.r|(icolor.g<<8)|(icolor.b<<16)|(icolor.a<<24);
}

vec3 rgbe_to_rgb(uint32_t rgbe)
{
    ivec4 icolor = ivec4(rgbe, rgbe>>8, rgbe>>16, rgbe>>24) & 0xFF;
    return ldexp(vec3(icolor) * (1.0f / 256.0f), ivec3(icolor.a-128));
}

vec2 octahedral_encode(vec3 normal)
{
    vec3 na = abs(normal);
    normal /= na.x + na.y + na.z;
    return normal.z >= 0.0f ?
        vec2(normal.x, normal.y) :
        (1.0f - abs(vec2(normal.y, normal.x))) * (step(vec2(0), vec2(normal.x, normal.y)) * 2.0f - 1.0f);
}

vec3 octahedral_decode(vec2 encoded_normal)
{
    vec2 na = abs(encoded_normal);
    vec3 normal = vec3(encoded_normal.x, encoded_normal.y, 1.0 - na.x - na.y);
    vec2 d = (step(vec2(0), vec2(normal.x, normal.y)) * 2.0f - 1.0f) * clamp(-normal.z, 0.0f, 1.0f);
    normal.x -= d.x;
    normal.y -= d.y;
    return normalize(normal);
}

uint32_t morton_encode(uvec3 x)
{
    x &= 0x000003ffu;
    x = (x ^ (x << 16u)) & 0xff0000ffu;
    x = (x ^ (x << 8u)) & 0x0300f00fu;
    x = (x ^ (x << 4u)) & 0x030c30c3u;
    x = (x ^ (x << 2u)) & 0x09249249u;
    return x.x + 2u * x.y + 4u * x.z;
}

uvec3 morton_decode(uint32_t m)
{
    uvec3 x = uvec3(m, m >> 1u, m >> 2u);
    x &= 0x09249249u;
    x = (x ^ (x >> 2u)) & 0x030c30c3u;
    x = (x ^ (x >> 4u)) & 0x0300f00fu;
    x = (x ^ (x >> 8u)) & 0xff0000ffu;
    x = (x ^ (x >> 16u)) & 0x000003ffu;
    return x;
}

uint32_t pcg(uint32_t& seed)
{
    seed = seed * 747796405u + 2891336453u;
    seed = ((seed >> ((seed >> 28) + 4)) ^ seed) * 277803737u;
    seed ^= seed >> 22;
    return seed;
}

uvec3 pcg3d(uvec3& seed)
{
    seed = seed * 1664525u + 1013904223u;
    seed += uvec3(seed.y, seed.z, seed.x) * uvec3(seed.z, seed.x, seed.y);
    seed ^= seed >> 16u;
    seed += uvec3(seed.y, seed.z, seed.x) * uvec3(seed.z, seed.x, seed.y);
    return seed;
}

uvec4 pcg4d(uvec4& seed)
{
    seed = seed * 1664525u + 1013904223u;
    seed += uvec4(seed.y, seed.z, seed.x, seed.y) * uvec4(seed.w, seed.x, seed.y, seed.z);
    seed ^= seed >> 16u;
    seed += uvec4(seed.y, seed.z, seed.x, seed.y) * uvec4(seed.w, seed.x, seed.y, seed.z);
    return seed;
}

template<typename T>
std::vector<T> generate_blue_noise(
    uvec2 size,
    uint32_t seed,
    int gauss_radius,
    float gauss_sigma,
    int iterations
){
    std::vector<T> bn(size.x * size.y);
    std::vector<T> blurred(size.x * size.y);
    std::vector<uint32_t> indices(size.x * size.y);
    std::vector<float> kernel = generate_gaussian_kernel(gauss_radius, gauss_sigma);

    // Generate white noise
    for(unsigned i = 0; i < size.x * size.y; ++i)
    {
        if constexpr(std::is_same_v<T, float>)
            bn[i] = std::ldexp(float(pcg(seed)), -32);
        else if constexpr(std::is_same_v<T, vec2>)
            bn[i] = ldexp(vec2(pcg(seed), pcg(seed)), ivec2(-32));
        else if constexpr(std::is_same_v<T, vec3>)
            bn[i] = ldexp(vec3(pcg(seed), pcg(seed), pcg(seed)), ivec3(-32));
        else if constexpr(std::is_same_v<T, vec4>)
            bn[i] = ldexp(vec4(pcg(seed), pcg(seed), pcg(seed), pcg(seed)), ivec4(-32));
    }

    for(int i = 0; i < iterations; ++i)
    {
        wrap_convolve(bn.data(), blurred.data(), size, kernel.data(), kernel.size());
        for(unsigned j = 0; j < size.x * size.y; ++j)
            bn[j] -= blurred[j];
        if constexpr(std::is_same_v<T, float>)
        {
            radix_inverse_argsort(bn.size(), bn.data(), indices.data(), [&](T key){
                return key;
            });
            for(unsigned j = 0; j < size.x * size.y; ++j)
                bn[j] = float(indices[j])/(size.x*size.y-1);
        }
        else
        {
            for(int k = 0; k < T::length(); ++k)
            {
                radix_inverse_argsort(bn.size(), bn.data(), indices.data(), [k](T key){
                    return key[k];
                });
                for(unsigned j = 0; j < size.x * size.y; ++j)
                    bn[j][k] = float(indices[j])/(size.x*size.y-1);
            }
        }
    }
    return bn;
}

template std::vector<float> generate_blue_noise(
    uvec2 size, uint32_t seed, int gauss_radius, float gauss_sigma,
    int iterations
);
template std::vector<vec2> generate_blue_noise(
    uvec2 size, uint32_t seed, int gauss_radius, float gauss_sigma,
    int iterations
);
template std::vector<vec3> generate_blue_noise(
    uvec2 size, uint32_t seed, int gauss_radius, float gauss_sigma,
    int iterations
);
template std::vector<vec4> generate_blue_noise(
    uvec2 size, uint32_t seed, int gauss_radius, float gauss_sigma,
    int iterations
);

vec3 closest_point_on_plane(vec3 p, vec4 plane)
{
    return p - (dot(vec3(plane), p) - plane.w) * vec3(plane);
}

vec3 closest_point_on_line(vec3 p, vec3 l0, vec3 l1)
{
    vec3 delta = l1 - l0;
    return l0 + clamp(dot(p-l0, delta)/dot(delta, delta), 0.0f, 1.0f) * delta;
}

bool point_in_triangle(vec3 p, vec3 c1, vec3 c2, vec3 c3)
{
    vec3 u = cross(c2-p, c3-p);
    vec3 v = cross(c3-p, c1-p);
    vec3 w = cross(c1-p, c2-p);
    return dot(u, v) >= 0.0f && dot(u, w) >= 0.0f;
}

vec3 closest_point_on_triangle(vec3 p, vec3 c1, vec3 c2, vec3 c3)
{
    // Put point on the triangle plane first
    vec4 plane = vec4(normalize(cross(c1-c2, c1-c3)), 0);
    plane.w = dot(c1, vec3(plane));
    p = closest_point_on_plane(p, plane);

    if(point_in_triangle(p, c1, c2, c3))
        return p;

    vec3 p1 = closest_point_on_line(p, c1, c2);
    vec3 p2 = closest_point_on_line(p, c2, c3);
    vec3 p3 = closest_point_on_line(p, c1, c3);

    float d1 = distance(p, p1);
    float d2 = distance(p, p2);
    float d3 = distance(p, p3);

    if(d1 < d2 && d1 < d3)
        return p1;
    else if(d2 < d3)
        return p2;
    return p3;
}

vec3 triangle_barycentrics(vec3 p, vec3 c1, vec3 c2, vec3 c3)
{
    vec3 v0 = c2 - c1;
    vec3 v1 = c3 - c1;
    vec3 v2 = p - c1;
    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    vec3 bc = vec3(
        0,
        (d11 * d20 - d01 * d21) / denom,
        (d00 * d21 - d01 * d20) / denom
    );
    bc.x = 1.0f - bc.y - bc.z;
    return bc;
}

aabb aabb_from_tetrahedron(const tetrahedron& tt)
{
    aabb bounding = {tt.corners[0], tt.corners[0]};
    for(int i = 1; i < 4; ++i)
    {
        bounding.min = min(tt.corners[i], bounding.min);
        bounding.max = max(tt.corners[i], bounding.max);
    }
    return bounding;
}

bool point_in_tetrahedron(const tetrahedron& tt, vec3 p)
{
    vec3 normals[4] = {
        cross(tt.corners[1]-tt.corners[0], tt.corners[2]-tt.corners[0]),
        cross(tt.corners[2]-tt.corners[1], tt.corners[3]-tt.corners[1]),
        cross(tt.corners[3]-tt.corners[2], tt.corners[0]-tt.corners[2]),
        cross(tt.corners[0]-tt.corners[3], tt.corners[1]-tt.corners[3])
    };
    vec4 d1 = vec4(
        dot(normals[0], tt.corners[3]-tt.corners[0]),
        dot(normals[1], tt.corners[0]-tt.corners[1]),
        dot(normals[2], tt.corners[1]-tt.corners[2]),
        dot(normals[3], tt.corners[2]-tt.corners[3])
    );
    vec4 d2 = vec4(
        dot(normals[0], p-tt.corners[0]),
        dot(normals[1], p-tt.corners[1]),
        dot(normals[2], p-tt.corners[2]),
        dot(normals[3], p-tt.corners[3])
    );
    return all(equal(sign(d1),sign(d2)));
}

vec4 tetrahedron_barycentrics(const tetrahedron& tt, vec3 p)
{
    return inverse(mat4(
        vec4(tt.corners[0], 1),
        vec4(tt.corners[1], 1),
        vec4(tt.corners[2], 1),
        vec4(tt.corners[3], 1)
    )) * vec4(p, 1);
}

vec3 closest_point_on_tetrahedron(const tetrahedron& tt, vec3 p)
{
    if(point_in_tetrahedron(tt, p))
        return p;

    vec3 p1 = closest_point_on_triangle(p, tt.corners[0], tt.corners[1], tt.corners[2]);
    vec3 p2 = closest_point_on_triangle(p, tt.corners[0], tt.corners[1], tt.corners[3]);
    vec3 p3 = closest_point_on_triangle(p, tt.corners[0], tt.corners[2], tt.corners[3]);
    vec3 p4 = closest_point_on_triangle(p, tt.corners[1], tt.corners[2], tt.corners[3]);

    float d1 = distance(p, p1);
    float d2 = distance(p, p2);
    float d3 = distance(p, p3);
    float d4 = distance(p, p4);

    float min_d = min(min(d1, d2), min(d3, d4));
    if(min_d == d1) return p1;
    if(min_d == d2) return p2;
    if(min_d == d3) return p3;
    return p4;
}

vec3 tetrahedron_circumcenter(const tetrahedron& tt)
{
    vec3 b = tt.corners[1] - tt.corners[0];
    vec3 c = tt.corners[2] - tt.corners[0];
    vec3 d = tt.corners[3] - tt.corners[0];

    return tt.corners[0] + 0.5f * (
        dot(b, b) * cross(d, c) +
        dot(c, c) * cross(b, d) +
        dot(d, d) * cross(c, b)
    ) / dot(b, cross(d, c));
}

vec3 create_tangent(vec3 normal)
{
    vec3 major;
    if(abs(normal.x) < 0.57735026918962576451) major = vec3(1,0,0);
    else if(abs(normal.y) < 0.57735026918962576451) major = vec3(0,1,0);
    else major = vec3(0,0,1);

    vec3 tangent = normalize(cross(normal, major));
    return tangent;
}

mat3 create_tangent_space(vec3 normal)
{
    vec3 tangent = create_tangent(normal);
    vec3 bitangent = cross(normal, tangent);

    return mat3(tangent, bitangent, normal);
}

}
