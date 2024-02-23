#ifndef RAYBASE_GFX_MATH_GLSL
#define RAYBASE_GFX_MATH_GLSL
#define M_PI  3.14159238265
#define SQRT2 1.41421356237
#define SQRT3 1.73205080756

vec2 lambert_azimuthal_equal_area_encode(vec3 normal)
{
    vec3 n = normalize(normal);
    return inversesqrt(2.0f+2.0f*n.z)*n.xy;
}

vec3 lambert_azimuthal_equal_area_decode(vec2 n2)
{
    n2 *= SQRT2;
    float d = dot(n2, n2);
    float f = sqrt(2.0f - d);
    return vec3(f*n2, 1.0f - d);
}

vec2 octahedral_encode(vec3 normal)
{
    vec3 na = abs(normal);
    normal /= na.x + na.y + na.z;
    return normal.z >= 0.0 ? normal.xy : (1.0 - abs(normal.yx)) * (step(0, normal.xy) * 2.0f - 1.0f);
}

vec3 octahedral_decode(vec2 encoded_normal)
{
    vec2 na = abs(encoded_normal);
    vec3 normal = vec3(encoded_normal.xy, 1.0 - na.x - na.y);
    normal.xy -= (step(0, normal.xy) * 2.0f - 1.0f) * clamp(-normal.z, 0.0f, 1.0f);
    return normalize(normal);
}

uint rgb_to_rgbe(vec3 color)
{
    ivec3 ex;
    frexp(color, ex);
    int e = max(ex.r, max(ex.g, ex.b));
    ivec4 icolor = clamp(ivec4(
        round(ldexp(color, ivec3(-e)) * 256.0f),
        e+128
    ), 0, 255);
    return icolor.r|(icolor.g<<8)|(icolor.b<<16)|(icolor.a<<24);
}

vec3 rgbe_to_rgb(uint rgbe)
{
    ivec4 icolor = ivec4(rgbe, rgbe>>8, rgbe>>16, rgbe>>24) & 0xFF;
    return ldexp(vec3(icolor) * (1.0f / 256.0f), ivec3(icolor.a-128));
}

uint rgb_to_r9g9b9e5(vec3 color)
{
    ivec3 ex;
    frexp(color, ex);
    int e = clamp(max(ex.r, max(ex.g, ex.b)), -16, 15);
    ivec3 icolor = clamp(ivec3(
        floor(ldexp(color, ivec3(-e)) * 512.0f)
    ), ivec3(0), ivec3(511));
    return icolor.r|(icolor.g<<9)|(icolor.b<<18)|((e+16)<<27);
}

vec3 r9g9b9e5_to_rgb(uint rgbe)
{
    ivec4 icolor = ivec4(rgbe, rgbe>>9, rgbe>>18, rgbe>>27) & 0x1FF;
    return ldexp(vec3(icolor) * (1.0f / 512.0f), ivec3(icolor.a-16));
}

uint morton_encode_3d(uvec3 x)
{
    x &= 0x000003ffu;
    x = (x ^ (x << 16u)) & 0xff0000ffu;
    x = (x ^ (x << 8u)) & 0x0300f00fu;
    x = (x ^ (x << 4u)) & 0x030c30c3u;
    x = (x ^ (x << 2u)) & 0x09249249u;
    return x.x + 2u * x.y + 4u * x.z;
}

uvec3 morton_decode_3d(uint m)
{
    uvec3 x = uvec3(m, m >> 1u, m >> 2u);
    x &= 0x09249249u;
    x = (x ^ (x >> 2u)) & 0x030c30c3u;
    x = (x ^ (x >> 4u)) & 0x0300f00fu;
    x = (x ^ (x >> 8u)) & 0xff0000ffu;
    x = (x ^ (x >> 16u)) & 0x000003ffu;
    return x;
}

uint morton_encode_2d(uvec2 x)
{
    x &= 0x0000ffffu;
    x = (x ^ (x << 8u)) & 0x00ff00ffu;
    x = (x ^ (x << 4u)) & 0x0f0f0f0fu;
    x = (x ^ (x << 2u)) & 0x33333333u;
    x = (x ^ (x << 1u)) & 0x55555555u;
    return x.x + 2u * x.y;
}

uvec2 morton_decode_2d(uint m)
{
    uvec2 x = uvec2(m, m >> 1u);
    x &= 0x55555555;
    x = (x ^ (x >> 1u)) & 0x33333333u;
    x = (x ^ (x >> 2u)) & 0x0f0f0f0fu;
    x = (x ^ (x >> 4u)) & 0x00ff00ffu;
    x = (x ^ (x >> 8u)) & 0x0000ffffu;
    return x;
}

float hyperbolize_depth(float depth, vec4 proj_info)
{
    if(proj_info.x < 0)
        depth = 1.0 / depth;
    return (depth-proj_info.y)/proj_info.x;
}

float linearize_depth(float depth, vec4 proj_info)
{
    float tmp = depth * proj_info.x + proj_info.y;
    // If proj_info.x > 0, the projection is orthographic. If less than 0,
    // it is perspective.
    return proj_info.x > 0 ? tmp : 1.0/tmp;
}

// Gives view-space position.
vec3 unproject_position_with_linear_depth(float linear_depth, vec2 uv, vec4 proj_info)
{
    return vec3((0.5f-uv) * proj_info.zw * linear_depth, linear_depth);
}

// Gives view-space position.
vec3 unproject_position(float depth, vec2 uv, vec4 proj_info)
{
    return unproject_position_with_linear_depth(
        linearize_depth(depth, proj_info),
        uv, proj_info
    );
}

// Return value: xy = uv, z = hyperbolic depth
vec3 project_position(vec3 pos, vec4 proj_info)
{
    float hdepth = hyperbolize_depth(pos.z, proj_info);
    return vec3(0.5-pos.xy / (proj_info.zw * pos.z), hdepth);
}

vec4 find_omni_pos(vec3 p, vec4 proj_info)
{
    vec3 ap = abs(p);

    vec3 face_pos = p;
    int face_index = 0;

    if(ap.x > ap.y && ap.x > ap.z) // +-X is depth
    {
        face_index = 0;
        face_pos = -face_pos.zyx;
    }
    else if(ap.y > ap.z) // +-Y is depth
    {
        face_index = 2;
        face_pos = -face_pos.xzy;
    }
    else // +-Z is depth
    {
        face_index = 4;
        face_pos = vec3(face_pos.x, -face_pos.yz);
    }

    // Check sign
    if(face_pos.z < 0)
    {
        face_index++;
        face_pos.xz = -face_pos.xz;
    }

    // Projection is simple due to forced 90 degree FOV and 1:1 aspect ratio
    return vec4(
        0.5f + 0.5f * face_pos.xy / face_pos.z,
        face_index,
        (-1.0/face_pos.z-proj_info.y)/proj_info.x
    );
}

vec3 unproject_omni_pos(vec2 uv, float depth, int face_index, vec4 proj_info)
{
    vec3 face_pos;
    face_pos.z = -1.0f / (depth * proj_info.x + proj_info.y);
    face_pos.xy = 2.0f * face_pos.z * (uv - 0.5f);

    if((face_index & 1) == 1)
    {
        --face_index;
        face_pos.xz = -face_pos.xz;
    }

    if(face_index == 0)
        face_pos = -face_pos.zyx;
    if(face_index == 2)
        face_pos = -face_pos.xzy;
    if(face_index == 4)
        face_pos = vec3(face_pos.x, -face_pos.yz);
    return face_pos;
}

vec3 unproject_cubemap_dir(vec2 uv, int face_index)
{
    vec3 face_pos;
    face_pos.z = 1.0f;
    face_pos.xy = 2.0f * face_pos.z * (uv - 0.5f);

    if((face_index & 1) == 1)
    {
        --face_index;
        if(face_index == 2)
            face_pos.yz = -face_pos.yz;
        else
            face_pos.xz = -face_pos.xz;
    }

    if(face_index == 0)
        face_pos = vec3(face_pos.z, -face_pos.yx);
    if(face_index == 2)
        face_pos = face_pos.xzy;
    if(face_index == 4)
        face_pos = vec3(face_pos.x, -face_pos.y, face_pos.z);
    return face_pos;
}

vec3 pixel_id_to_cubemap_direction(int pixel_id, vec2 off, ivec2 size)
{
    int face_size = size.x * size.y;
    int face = pixel_id / face_size;
    pixel_id %= face_size;
    ivec2 xy = ivec2(pixel_id % size.x, pixel_id / size.x);
    vec3 dir = vec3(
        2.0f * (vec2(xy) + off)/vec2(size) - 1.0f,
        1.0f
    );
    if(face >= 3) { dir = -dir; face -= 3; }
    switch(face)
    {
    case 0:
        return dir.xyz;
    case 1:
        return dir.zxy;
    case 2:
        return dir.yzx;
    }
}

int cubemap_direction_to_pixel_id(vec3 dir, ivec2 size)
{
    vec3 ap = abs(dir);
    int face_index = 0;

    if(ap.z > ap.x && ap.z > ap.y) // +-Z is depth
    {
        face_index = 0;
        dir = dir.xyz;
    }
    else if(ap.x > ap.y) // +-Y is depth
    {
        face_index = 1;
        dir = dir.yzx;
    }
    else // +-X is depth
    {
        face_index = 2;
        dir = dir.zxy;
    }
    if(dir.z < 0) face_index += 3;

    vec2 uv = vec2(dir)/dir.z;
    ivec2 xy = ivec2(floor((uv + 1.0f)*vec2(size)*0.5f));
    return xy.x + xy.y * size.x + face_index * size.x * size.y;
}


// Gives view-space view ray direction.
vec3 get_view_ray(vec2 uv, vec4 proj_info)
{
    return normalize(vec3((uv-0.5f) * proj_info.zw, -1));
}

// Same as get_view_ray without the normalization.
vec3 get_view_ray_unnormalized(vec2 uv, vec4 proj_info)
{
    return vec3((uv-0.5f) * proj_info.zw, -1);
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

// a and b must be unit vectors.
bool direction_equal(vec3 a, vec3 b, float epsilon)
{
    // Don't use dot() for this, its' inaccurate due to float precision :(
    // This naive approach is actually way better too, since it converges
    // towards zero, so it acts way better with floats.
    return distance(a, b) < epsilon;
}

//https://pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations
vec2 concentric_mapping(vec2 u)
{
    vec2 uOffset = 2.0f * u - 1.0f;
    vec2 abs_uOffset = abs(uOffset);

    if(all(lessThan(abs_uOffset, vec2(1e-4))))
        return vec2(0);

    vec2 r_theta = abs_uOffset.x > abs_uOffset.y ?
        vec2(uOffset.x, M_PI * 0.25 * uOffset.y / uOffset.x) :
        vec2(uOffset.y, M_PI * 0.5 - M_PI * 0.25 * uOffset.x / uOffset.y);
    return r_theta.x * vec2(cos(r_theta.y), sin(r_theta.y));
}

vec3 tangent_reflect(vec3 v)
{
    return vec3(v.xy, -v.z);
}

vec3 tangent_refract(vec3 v, float eta)
{
    float k = 1.0 - eta * eta * (1.0 - v.z * v.z);
    if(k < 0.0) return vec3(0.0f);
    else return eta * -v - (eta * -v.z + sqrt(k)) * vec3(0,0,1);
    return vec3(v.xy, -v.z);
}

// https://www.pcg-random.org/
uint pcg(inout uint seed)
{
    seed = seed * 747796405u + 2891336453u;
    seed = ((seed >> ((seed >> 28) + 4)) ^ seed) * 277803737u;
    seed ^= seed >> 22;
    return seed;
}

// http://www.jcgt.org/published/0009/03/02/
uvec3 pcg3d(inout uvec3 seed)
{
    seed = seed * 1664525u + 1013904223u;
    seed += seed.yzx * seed.zxy;
    seed ^= seed >> 16;
    seed += seed.yzx * seed.zxy;
    return seed;
}

uvec4 pcg4d(inout uvec4 seed)
{
    seed = seed * 1664525u + 1013904223u;
    seed += seed.yzxy * seed.wxyz;
    seed ^= seed >> 16;
    seed += seed.yzxy * seed.wxyz;
    return seed;
}

uvec4 pcg1to4(inout uint seed)
{
    uvec4 seed4 = seed+uvec4(0,1,2,3);
    pcg4d(seed4);
    seed = seed4.x;
    return seed4;
}

float generate_uniform_random(inout uint seed)
{
    return ldexp(float(pcg(seed)), -32);
}

vec4 generate_uniform_random(inout uvec4 seed)
{
    return ldexp(vec4(pcg4d(seed)), ivec4(-32));
}

uint find_nth_set_bit(uint v, uint n)
{
    uint c = 0;
    uvec4 parts = uvec4(v, v>>8u, v>>16u, v>>24u)&0xFFu;
    ivec4 bits = bitCount(parts);

    if(n >= bits.x + bits.y) { c += 16; n -= bits.x + bits.y; bits.x = bits.z; }
    if(n >= bits.x) { c += 8; n -= bits.x;}

    parts = (uvec4(v, v>>2u, v>>4u, v>>6u)>>c)&0x3u;
    bits = bitCount(parts);

    if(n >= bits.x + bits.y) { c += 4; n -= bits.x + bits.y; bits.x = bits.z;}
    if(n >= bits.x) { c += 2; n -= bits.x;}
    if(n >= ((v >> c)&1)) { c++; }

    return c;
}

uint find_nth_set_bit(uvec4 v, uint n)
{
    ivec4 count = bitCount(v);
    uint low = count.x;
    uint i = 0;
    if(n >= count.x + count.y) { n -= count.x + count.y; i = 2; low = count.z; }
    if(n >= low) { n -= low; i++; }
    return i * 32u + find_nth_set_bit(v[i], n);
}

uint find_bit_rank(uint v, uint b)
{
    return bitCount(v&((1<<b)-1));
}

uvec3 find_bit_rank(uvec3 v, uint b)
{
    return bitCount(v&((1<<b)-1));
}

mat3 orthogonalize(mat3 m)
{
    m[0] = normalize(m[0] - dot(m[0], m[2]) * m[2]);
    m[1] = cross(m[2], m[0]);
    return m;
}

vec3 quat_rotate(vec4 q, vec3 v)
{
    return 2.0f * cross(q.w * v + cross(v, q.xyz), q.xyz) + v;
}

vec4 quat_inverse(vec4 q)
{
    return vec4(-q.x, -q.y, -q.z, q.w);
}

vec3 dual_quat_transform(mat2x4 dq, vec3 v)
{
    vec3 t = 2.0f * (
        dq[0].w * dq[1].xyz -
        dq[1].w * dq[0].xyz +
        cross(dq[1].xyz, dq[0].xyz)
    );
    return quat_rotate(dq[0], v) + t;
}

mat3 cofactor(mat4 m)
{
    return mat3(
        dot(m[1].yzw, cross(m[2].yzw, m[3].yzw)),
        dot(m[1].xwz, cross(m[2].xwz, m[3].xwz)),
        dot(m[1].xyw, cross(m[2].xyw, m[3].xyw)),
        dot(m[0].ywz, cross(m[2].ywz, m[3].ywz)),
        dot(m[0].xzw, cross(m[2].xzw, m[3].xzw)),
        dot(m[0].xwy, cross(m[2].xwy, m[3].xwy)),
        dot(m[0].yzw, cross(m[1].yzw, m[3].yzw)),
        dot(m[0].xwz, cross(m[1].xwz, m[3].xwz)),
        dot(m[0].xyw, cross(m[1].xyw, m[3].xyw))
    );
}

float erf(float x)
{
    float x2 = x * x;
    float ax2 = 0.147 * x2;
    return sign(x) * sqrt(1-exp(-x2*((4.0f/M_PI)+ax2)/(1+ax2)));
}

float inv_erf(float x)
{
    float ln1x2 = log(1-x*x);
    const float a = 0.147;
    const float p = 2.0f/(M_PI*a);
    float k = p + ln1x2 * 0.5f;
    float k2 = k*k;
    return sign(x) * sqrt(sqrt(k2-ln1x2*(1.0f/a))-k);
}

float sample_gaussian(float u, float sigma, float epsilon)
{
    float k = u * 2.0f - 1.0f;
    k = clamp(k, -(1.0f-epsilon), 1.0f-epsilon);
    return sigma * 1.41421356f * inv_erf(k);
}

float sample_blackman_harris(float u)
{
    bool flip = u > 0.5;
    u = flip ? 1 - u : u;
    vec4 v = vec4(-0.46901354f, 0.93713735f, -0.14434936f, 0.02061377f) *
        pow(vec4(u), vec4(0.5f, 0.25, 0.125f, 0.0625f));
    float s = 0.31247268f * u + v.x + v.y + v.z + v.w;
    return flip ? 1 - s : s;
}

vec3 sample_sphere(vec2 u)
{
    float cos_theta = 2.0f * u.x - 1.0f;
    float sin_theta = sqrt(1.0f - cos_theta * cos_theta);
    float phi = u.y * 2.0f * M_PI;
    return vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}

vec3 sample_ball(vec3 u)
{
    vec3 d = sample_sphere(u.xy);
    return pow(u.z, 1.0f/3.0f) * d;
}

vec3 sample_cone(vec3 dir, float cos_theta_min, vec2 u)
{
    float cos_theta = mix(1.0f, cos_theta_min, u.x);
    float sin_theta = sqrt(1.0f - cos_theta * cos_theta);
    float phi = u.y * 2 * M_PI;
    return create_tangent_space(dir) * vec3(
        cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta
    );
}

vec2 sample_disk_intro(vec2 u)
{
    vec2 uo = 2.0f * u - 1.0f;
    vec2 inv_uo = 1.0f/uo;
    vec2 auo = abs(uo);

    // Handle corner case where inv_uo is inf
    if(all(lessThan(auo, vec2(1e-5))))
        return vec2(0);

    vec2 radius_theta = auo.x > auo.y ?
        vec2(u.x, M_PI/4 * (uo.y * inv_uo.x)) :
        vec2(u.y, M_PI/2 - M_PI/4 * (uo.x * inv_uo.y));
    return radius_theta;
}

vec2 sample_disk(vec2 u)
{
    vec2 radius_theta = sample_disk_intro(u);
    return (2.0f * radius_theta.x - 1.0f) * vec2(
        cos(radius_theta.y),
        sin(radius_theta.y)
    );
}

vec2 sample_ring(vec2 u, float min_dist, float width)
{
    vec2 radius_theta = sample_disk_intro(u);
    // TODO: May not have a uniform distribution! Should be fixed!
    radius_theta.x = (2.0f * radius_theta.x - 1.0f) * width;
    radius_theta.x += sign(radius_theta.x) * min_dist;
    return radius_theta.x * vec2(
        cos(radius_theta.y),
        sin(radius_theta.y)
    );
}

// A low-discrepancy sequence.
vec2 weyl(vec2 u, int i)
{
    return fract(u + vec2(i * ivec2(12664745, 9560333))/exp2(24));
}

vec2 sample_gaussian_weighted_disk(vec2 u, float sigma)
{
    vec2 radius_theta = sample_disk_intro(u);
    radius_theta.x = sample_gaussian(radius_theta.x, sigma, 1e-6f);
    return radius_theta.x * vec2(
        cos(radius_theta.y),
        sin(radius_theta.y)
    );
}

vec2 sample_blackman_harris_weighted_disk(vec2 u)
{
    vec2 radius_theta = sample_disk_intro(u);
    radius_theta.x = 2.0f * sample_blackman_harris(radius_theta.x) - 1.0f;
    return radius_theta.x * vec2(
        cos(radius_theta.y),
        sin(radius_theta.y)
    );
}

vec3 sample_triangle_area(vec2 u, vec3 a, vec3 b, vec3 c)
{
    u = u.x+u.y > 1 ? 1 - u : u;
    return a + (b - a) * u.x + (c - a) * u.y;
}

// Uses Householder reflection to simplify the problem into a 2x2 matrix
// determinant.
float accurate_determinant(mat3 mat)
{
    float div = 0.5f / (abs(mat[1].x) + 1.0f);
    vec3 h = mat[1] - vec3(mat[1].x > 0 ? -1 : 1, 0, 0);

    vec3 a0 = mat[0] - 2.0f * h * dot(h, mat[0]) * div;
    vec3 a2 = mat[2] - 2.0f * h * dot(h, mat[2]) * div;
    return abs(a0.y * a2.z - a2.y * a0.z);
}

// https://momentsingraphics.de/Siggraph2021.html
void sample_triangle_solid_angle(vec2 u, vec3 pos, vec3 a, vec3 b, vec3 c, out vec3 sample_dir, out float pdf)
{
    vec3 na = normalize(a-pos);
    vec3 nb = normalize(b-pos);
    vec3 nc = normalize(c-pos);

    float a_dot_b = dot(na, nb);
    float a_dot_c = dot(na, nc);
    float b_dot_c = dot(nb, nc);

    float G0 = accurate_determinant(mat3(na,nb,nc));
    float G1 = a_dot_c + b_dot_c;
    float G2 = 1.0f + a_dot_b;

    float solid_angle = 2.0f * atan(G0, G2 + G1);
    float part_solid_angle = u.x * solid_angle;
    float half_part_solid_angle = part_solid_angle * 0.5f;
    float sin_half_part_solid_angle = sin(half_part_solid_angle);

    vec3 r =
        (G0 * cos(half_part_solid_angle) - G1 * sin_half_part_solid_angle) * na +
        G2 * sin_half_part_solid_angle * nc;

    vec3 ntc = -na + 2.0f * dot(na, r) * r / dot(r, r);
    float stc = dot(nb, ntc);
    float s = 1.0f - u.y + stc * u.y;

    float tt = sqrt((1.0f - s*s)/(1.0f - stc * stc));

    sample_dir = normalize((s - tt * stc) * nb + tt * ntc);
    pdf = 1.0f / solid_angle;

}

float triangle_area(vec3 a, vec3 b, vec3 c)
{
    return 0.5 * length(cross(b - a, c - a));
}

float triangle_solid_angle(vec3 pos, vec3 a, vec3 b, vec3 c)
{
    vec3 na = normalize(a-pos);
    vec3 nb = normalize(b-pos);
    vec3 nc = normalize(c-pos);

    float a_dot_b = dot(na, nb);
    float a_dot_c = dot(na, nc);
    float b_dot_c = dot(nb, nc);

    return 2.0f * atan(
        accurate_determinant(mat3(na,nb,nc)),
        1.0f + a_dot_b + a_dot_c + b_dot_c
    );
}

float solid_angle_to_area_pdf(vec3 view, float dist, vec3 normal)
{
    return abs(dot(view, normal)) / (dist * dist);
}

float area_to_solid_angle_pdf(vec3 view, float dist, vec3 normal)
{
    return (dist * dist) / abs(dot(view, normal));
}

vec2 sample_regular_polygon(vec2 u, float angle, uint sides)
{
    float side = floor(u.x * sides);
    u.x = fract(u.x * sides);
    float side_radians = (2.0f*M_PI)/sides;
    float a1 = side_radians * side + angle;
    float a2 = side_radians * (side + 1) + angle;
    vec2 b = vec2(sin(a1), cos(a1));
    vec2 c = vec2(sin(a2), cos(a2));
    u = u.x+u.y > 1 ? 1 - u : u;
    return b * u.x + c * u.y;
}

// Perturbation must be between 0 and 1.
vec2 fibonacci_disk(int i, int count, vec2 perturbation)
{
    float theta = i * 2.399963229728653f + M_PI * 2.0f * perturbation.x;
    return sqrt((perturbation.y+i)/count)*vec2(cos(theta), sin(theta));
}

vec3 fibonacci_sphere(int i, int count, vec2 perturbation)
{
    float theta = (i + perturbation.x) * 0.3819660112501051f;
    return sample_sphere(vec2((perturbation.y+i)/float(count), theta));
}

vec3 fibonacci_cone(int i, int count, vec3 dir, float cos_theta_min, vec2 perturbation)
{
    float theta = (i + perturbation.x) * 0.3819660112501051f;
    return sample_cone(dir, cos_theta_min, vec2((perturbation.y+i)/float(count), theta));
}

// http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
float interleaved_gradient_noise(vec2 pixel, int frame)
{
    pixel += frame * 5.588238f;
    return fract(52.9829189f * fract(dot(vec2(0.06711056f, 0.00583715f), pixel)));
}

vec3 clamped_reflect(vec3 I, vec3 N)
{
    return I - 2.0f * max(dot(N, I), 0.0f) * N;
}

vec3 box_parallax(vec3 o, vec3 dir)
{
    vec3 t = (step(vec3(0), dir) * 2.0f - 1.0f - clamp(o, vec3(-1), vec3(1))) / dir;
    return o + dir * min(min(t.x, t.y), t.z);
}

vec3 sphere_parallax(vec3 o, vec3 dir)
{
    float length2 = dot(o, o);
    if(length2 > 1)
    {
        o = normalize(o);
        length2 = 1;
    }

    float a = dot(dir, dir);
    float b = dot(o, dir);
    return o - dir * (b - sqrt(b * b + a - a * length2));
}

vec2 sphere_intersection(
    vec3 pos, float radius, vec3 ray_origin, vec3 ray_dir
){
    vec3 rp = ray_origin - pos;
    float a = dot(ray_dir, ray_dir);
    float b = dot(rp, ray_dir);
    float c = dot(rp, rp) - radius * radius;
    float discriminant = b * b - a * c;
    if(discriminant < 0) return vec2(-1.0f);
    float sqrtD = sqrt(discriminant);
    return (vec2(-sqrtD, sqrtD) - b) / a;
}

float ray_plane_intersection(
    vec3 a, vec3 b, vec3 c,
    vec3 ray_origin, vec3 ray_dir
){
    vec4 plane = vec4(normalize(cross(b-a, c-a)), 0);
    plane.w = dot(a-ray_origin, plane.xyz);
    float dist = abs(plane.w / dot(plane.xyz, ray_dir));
    if(isnan(dist)) return 0;
    return dist;
}

vec3 triangle_barycentric_coords(vec3 p, vec3 a, vec3 b, vec3 c)
{
    vec3 a_to_b = b - a;
    float bb = dot(a_to_b, a_to_b);

    vec3 a_to_c = c - a;
    float bc = dot(a_to_b, a_to_c);
    float cc = dot(a_to_c, a_to_c);

    vec3 a_to_p = p - a;
    float pb = dot(a_to_p, a_to_b);
    float pc = dot(a_to_p, a_to_c);
    vec2 bary = vec2(
        (cc * pb - bc * pc),
        (bb * pc - bc * pb)
    ) / (bb * cc - bc * bc);

    return vec3(1.0f - bary.x - bary.y, bary.xy);
}

float texel_solid_angle(ivec2 p, ivec2 size)
{
    vec2 inv_size = 1.0f / vec2(size);
    vec2 uv = (2.0f * vec2(p) + 1.0f) * inv_size - 1.0f;

    vec4 range = uv.xyxy + vec4(-inv_size, inv_size);
    vec4 range2 = range * range;

    vec4 at = atan(range.xxzz * range.ywyw, sqrt(range2.xxzz + range2.ywyw + 1.0f));

    return at.x - at.y - at.z + at.w;
}

float cubemap_jacobian_determinant(vec3 dir)
{
    vec3 ad = abs(dir);
    float inv_d = max(ad.x, max(ad.y, ad.z)) * inversesqrt(dot(dir, dir));
    return inv_d * inv_d * inv_d * 24; // 6 faces, each face is mapped -1 to 1 (area is 4)
}

int weighted_selection(inout float u, vec4 weights)
{
    weights.yw += weights.xz;
    weights.zw += weights.yy;
    weights /= weights.w;
    int i = 0;
    if(u < weights.y) i = u < weights.x ? 0 : 1;
    else i = u < weights.z ? 2 : 3;

    float low = i > 0 ? weights[i-1] : 0;
    float high = weights[i];
    u = (u-low)/(high-low);
    return i;
}

struct aabb
{
    // vec4 for alignment reasons.
    vec4 bounds_min;
    vec4 bounds_max;
};

aabb aabb_union(aabb a, aabb b)
{
    return aabb(min(a.bounds_min, b.bounds_min), max(a.bounds_max, b.bounds_max));
}

vec3 closest_point_on_plane(vec3 p, vec4 plane)
{
    return p - (dot(plane.xyz, p) - plane.w) * plane.xyz;
}

vec3 closest_point_on_line(vec3 p, vec3 l0, vec3 l1)
{
    vec3 delta = l1 - l0;
    return l0 + clamp(dot(p-l0, delta)/dot(delta, delta), 0, 1) * delta;
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
    plane.w = dot(c1, plane.xyz);
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

vec3 cosine_hemisphere_sample(vec2 u)
{
    vec2 d = concentric_mapping(u);
    return vec3(d, sqrt(max(0, 1.0f - dot(d, d))));
}

float cosine_hemisphere_pdf(vec3 dir)
{
    return max(dir.z * (1.0/M_PI), 0.0f);
}

vec3 uniform_hemisphere_sample(vec2 u)
{
    vec3 d = sample_sphere(u);
    d.z = abs(d.z);
    return d;
}

float uniform_hemisphere_pdf(vec3 dir)
{
    return 1.0 / (2.0f * M_PI);
}

void init_pixel_ray_cone(
    vec4 proj_info,
    ivec2 p,
    ivec2 resolution,
    out float ray_cone_radius,
    out float ray_cone_angle
){
    if(proj_info.x < 0)
    { // Perspective
        vec2 step_size = proj_info.wz / resolution;
        vec2 a = abs(-proj_info.wz * 0.5f + step_size * vec2(p));
        vec2 b = abs(-proj_info.wz * 0.5f + step_size * vec2(p+1));

        vec2 low = min(a, b);
        vec2 high = max(a, b);

        vec2 angles = atan(high) - atan(low);

        ray_cone_radius = 0;
        ray_cone_angle = min(angles.x, angles.y);
    }
    else
    { // Ortho
        ray_cone_radius = (proj_info.z / resolution.x + proj_info.w / resolution.y) * 0.25f;
        ray_cone_angle = 0.0f;
    }
}

vec2 owen_scrambled_hammersley(uint i, uint N, uvec2 seed)
{
    uvec2 v = uvec2(bitfieldReverse(uint(ldexp(float(i)/float(N), 32))), i);
    v ^= v * 0x3d20adeau;
    v += seed;
    v *= (seed >> 16u) | 1u;
    v ^= v * 0x05526c56u;
    v ^= v * 0x53a22864u;
    return ldexp(vec2(bitfieldReverse(v)), ivec2(-32));
}

bool reconnection_condition(
    float dist,
    float min_distance,
    float new_roughness,
    float vertex_roughness,
    float min_roughness
){
    return step(min_distance, dist) * step(min_roughness, min(new_roughness, vertex_roughness)) > 0;
}

float reconnection_shift_jacobian(
    vec3 orig_pos,
    vec3 orig_vertex_pos,
    vec3 orig_vertex_normal,
    vec3 new_pos,
    vec3 new_vertex_pos,
    vec3 new_vertex_normal
){
    vec3 orig_to_vertex = orig_vertex_pos - orig_pos;
    vec3 new_to_vertex = new_vertex_pos - new_pos;
    float orig_to_vertex_len2 = dot(orig_to_vertex, orig_to_vertex);
    float new_to_vertex_len2 = dot(new_to_vertex, new_to_vertex);
    float orig_to_vertex_len = sqrt(orig_to_vertex_len2);
    float new_to_vertex_len = sqrt(new_to_vertex_len2);

    float jacobian = abs(dot(new_vertex_normal, new_to_vertex)) * orig_to_vertex_len2 * orig_to_vertex_len /
        (abs(dot(orig_vertex_normal, orig_to_vertex)) * new_to_vertex_len2 * new_to_vertex_len);

    // Extreme angles can break the jacobian. Those angles also should
    // not reflect light, so we should be able to just skip them.
    return isinf(jacobian) || isnan(jacobian) ? 0.0f : jacobian;
}

float reconnection_shift_half_jacobian(
    vec3 pos,
    vec4 vertex_pos,
    vec3 vertex_normal
){
    float jacobian = 1.0f;
    if(vertex_pos.w != 0)
    {
        vec3 to_vertex = vertex_pos.xyz - pos;
        float to_vertex_len2 = dot(to_vertex, to_vertex);
        float to_vertex_len = sqrt(to_vertex_len2);
        jacobian *= abs(dot(vertex_normal, to_vertex)) / (to_vertex_len2 * to_vertex_len);
    }
    return jacobian;
}

float hybrid_shift_half_jacobian(
    float v0_pdf,
    float v1_pdf,
    vec3 pos,
    vec4 vertex_pos,
    vec3 vertex_normal
){
    return v0_pdf * v1_pdf * reconnection_shift_half_jacobian(pos, vertex_pos, vertex_normal);
}

float edge_detect(
    vec3 normal1, vec3 pos1,
    vec3 normal2, vec3 pos2,
    float normal_sensitivity,
    float pos_sensitivity
){
    vec3 tangent = normalize(pos1-pos2);
    return
        clamp(1.0f-abs(dot(tangent, normal1))*pos_sensitivity, 0.0, 1.0) *
        clamp(1.0f-abs(dot(tangent, normal2))*pos_sensitivity, 0.0, 1.0) *
        pow(clamp(dot(normal1, normal2), 0.0f, 1.0f), normal_sensitivity);
}

float edge_detect(vec3 normal1, vec3 pos1, vec3 pos2, float pos_sensitivity)
{
    vec3 tangent = normalize(pos1-pos2);
    return clamp(1.0f-abs(dot(tangent, normal1))*pos_sensitivity, 0.0, 1.0);
}

// https://developer.download.nvidia.com/video/gputechconf/gtc/2020/presentations/s22699-fast-denoising-with-self-stabilizing-recurrent-blurs.pdf
float disocclusion_detect(vec3 normal1, vec3 pos1, vec3 pos2, float inv_max_plane_dist)
{
    return clamp(1.0f - abs(dot(normal1, pos2-pos1)) * inv_max_plane_dist, 0.0f, 1.0f);
}

float disocclusion_detect(vec3 normal1, vec3 pos1, vec3 normal2, vec3 pos2, float inv_max_plane_dist)
{
    vec3 delta = (pos2-pos1);
    return
        clamp(1.0f - abs(dot(normal1, delta)) * inv_max_plane_dist, 0.0f, 1.0f) *
        clamp(1.0f - abs(dot(normal2, delta)) * inv_max_plane_dist, 0.0f, 1.0f);
}

#endif
