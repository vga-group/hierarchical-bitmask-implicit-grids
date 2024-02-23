#ifndef RAYBASE_GFX_MATERIAL_GLSL
#define RAYBASE_GFX_MATERIAL_GLSL
#include "math.glsl"
#include "color.glsl"
#include "material_data.glsl"

#define MATERIAL_FEATURE_ALBEDO (1<<0)
#define MATERIAL_FEATURE_METALLIC_ROUGHNESS (1<<1)
#define MATERIAL_FEATURE_EMISSION (1<<2)
#define MATERIAL_FEATURE_NORMAL_MAP (1<<3)
#define MATERIAL_FEATURE_TRANSMISSION (1<<4)
#define MATERIAL_FEATURE_ATTENUATION (1<<5)
#define MATERIAL_FEATURE_CLEARCOAT (1<<6)
#define MATERIAL_FEATURE_SHEEN (1<<7)
#define MATERIAL_FEATURE_SUBSURFACE (1<<8)
#define MATERIAL_FEATURE_ANISOTROPY (1<<9)
#define MATERIAL_FEATURE_MULTISCATTER (1<<10)
#define MATERIAL_FEATURE_ENVMAP_SPECULAR (1<<11)
#define MATERIAL_FEATURE_ENVMAP_PARALLAX (1<<12)
#define MATERIAL_FEATURE_LIGHTMAP (1<<13)
#define MATERIAL_FEATURE_SH_PROBES (1<<14)

#define ZERO_ROUGHNESS_LIMIT 0.001f

#define HAS_MATERIAL(feature) \
    ((RB_SUPPORTED_MATERIAL_FEATURES & MATERIAL_FEATURE_##feature) != 0)

// Normals should still be in tangent space, otherwise this function will cause
// broken normals!
material blend_material(in material below, in material above)
{
    float coverage = above.albedo.a;
    material mat;

    mat.albedo.rgb = mix(below.albedo.rgb, above.albedo.rgb, coverage);
    mat.albedo.a = clamp(
        below.albedo.a + above.albedo.a - below.albedo.a * above.albedo.a,
        0.0f, 1.0f
    );
    if(HAS_MATERIAL(METALLIC_ROUGHNESS))
    {
        mat.metallic = mix(below.metallic, above.metallic, coverage);
        mat.roughness = mix(below.roughness, above.roughness, coverage);
    }
    if(HAS_MATERIAL(EMISSION))
    {
        mat.emission = mix(below.emission, above.emission, coverage);
    }
    if(HAS_MATERIAL(NORMAL_MAP))
    {
        mat.normal = normalize(vec3(
            below.normal.xy + above.normal.xy * coverage,
            below.normal.z * mix(1.0f, abs(above.normal.z), coverage)
        ));
    }
    if(HAS_MATERIAL(TRANSMISSION))
    {
        mat.transmission = mix(below.transmission, above.transmission, coverage);
        mat.translucency = mix(below.translucency, above.translucency, coverage);
    }
    if(HAS_MATERIAL(ATTENUATION))
    {
        mat.attenuation = below.attenuation;
    }
    if(HAS_MATERIAL(CLEARCOAT))
    {
        mat.clearcoat = mix(below.clearcoat, above.clearcoat, coverage);
        mat.clearcoat_roughness = mix(below.clearcoat_roughness, above.clearcoat_roughness, coverage);
    }
    if(HAS_MATERIAL(SHEEN))
    {
        mat.sheen_color = mix(below.sheen_color, above.sheen_color, coverage);
        mat.sheen_roughness = mix(below.sheen_roughness, above.sheen_roughness, coverage);
    }
    if(HAS_MATERIAL(ANISOTROPY))
    {
        mat.anisotropy = mix(below.anisotropy, above.anisotropy, coverage);
        mat.anisotropy_rotation = mix(below.anisotropy_rotation, above.anisotropy_rotation, coverage);
    }
    mat.eta = mix(below.eta, above.eta, coverage);
    mat.f0 = mix(below.f0, above.f0, coverage);
    return mat;
}

vec2 get_anisotropic_roughness(float roughness, float anisotropy)
{
    float aspect = sqrt(1.0f - anisotropy * 0.9f);
    return vec2(roughness) * vec2(1.0f/aspect, aspect);
}

// All shading calculations are done in tangent space!

// =============================================================================
// Fresnel terms (F)
// =============================================================================

// Fast basic fresnel, doesn't handle refraction correctly.
float fresnel_schlick(float v_dot_h, float f0)
{
    if(f0 <= 0) return 0.0f; // Allows for lambertian materials with f0 = 0
    return f0 + (1.0f - f0) * pow(max(1.0f - v_dot_h, 0.0f), 5.0f);
}

// Extensive fresnel that also handles refractions.
float fresnel_schlick_bidir(float v_dot_h, in material mat)
{
    if(mat.eta > 1.0f)
    {
        float sin_theta2 = mat.eta * mat.eta * (1.0f - v_dot_h * v_dot_h);
        if(sin_theta2 >= 1.0f)
            return 1.0f;
        v_dot_h = sqrt(1.0f - sin_theta2);
    }
    return fresnel_schlick(v_dot_h, mat.f0);
}

// https://seblagarde.wordpress.com/2011/08/17/hello-world/
float fresnel_schlick_attenuated(float v_dot_h, float f0, float roughness)
{
    return f0 + (max(1.0f - roughness, f0) - f0) * pow(1.0f - v_dot_h, 5.0f);
}

float fresnel_schlick_attenuated_bidir(float v_dot_h, in material mat)
{
    if(mat.eta > 1.0f)
    {
        float sin_theta2 = mat.eta * mat.eta * (1.0f - v_dot_h * v_dot_h);
        if(sin_theta2 >= 1.0f)
            return 1.0f;
        v_dot_h = sqrt(1.0f - sin_theta2);
    }
    return fresnel_schlick_attenuated(v_dot_h, mat.f0, mat.roughness);
}

// =============================================================================
// Distribution terms (D)
// =============================================================================

float trowbridge_reitz_distribution_anisotropic(vec3 h, float ax, float ay)
{
    float ax2 = ax * ax;
    float ay2 = ay * ay;
    float axy2 = ax2 * ay2;
    float denom = dot(vec3(ay2, ax2, axy2), h * h);
    return (axy2 * ax * ay) / max(M_PI * denom * denom, 1e-16);
}

float trowbridge_reitz_distribution_isotropic(float hdotn, float a)
{
    float a2 = a * a;
    float denom = hdotn * hdotn * (a2 - 1) + 1;
    return a2 / max(M_PI * denom * denom, 1e-10);
}

float trowbridge_reitz_distribution_isotropic(vec3 h, float a)
{
    return trowbridge_reitz_distribution_isotropic(h.z, a);
}

// !!! IMPORTANT !!! Pre-multiplied by M_PI
float charlie_distribution(float hdotn, float a)
{
    float inva = 0.5f / a;
    return (1.0f + inva) * pow(1.0f - hdotn * hdotn, inva);
}

// =============================================================================
// Geometry terms (G)
// =============================================================================

// !!! IMPORTANT !!! This is pre-divided by 4 * dot(l,normal) * dot(v,normal)
float trowbridge_reitz_masking_shadowing_anisotropic(vec3 l, vec3 v, vec3 h, float ax, float ay)
{
    return step(0.0f, v.z * dot(v, h)) * step(0.0f, l.z * dot(l, h)) * 0.5f / (
        abs(v.z) * sqrt(l.z * l.z + l.x * l.x * ax * ax + l.y * l.y * ay * ay) +
        abs(l.z) * sqrt(v.z * v.z + v.x * v.x * ax * ax + v.y * v.y * ay * ay)
    );
}

// !!! IMPORTANT !!! This is pre-divided by 4 * dot(l,normal) * dot(v,normal)
float trowbridge_reitz_masking_shadowing_isotropic(
    float ldotn, float ldoth,
    float vdotn, float vdoth,
    float a
){
    return step(0.0f, vdotn * vdoth) * step(0.0f, ldotn * ldoth) * 0.5f / (
        abs(vdotn) * sqrt(ldotn * ldotn - a * a * ldotn * ldotn + a * a) +
        abs(ldotn) * sqrt(vdotn * vdotn - a * a * vdotn * vdotn + a * a)
    );
}

// !!! IMPORTANT !!! This is pre-divided by 4 * dot(l,normal) * dot(v,normal)
float trowbridge_reitz_masking_shadowing_isotropic(vec3 l, vec3 v, vec3 h, float a)
{
    return trowbridge_reitz_masking_shadowing_isotropic(l.z, dot(l, h), v.z, dot(v, h), a);
}

float trowbridge_reitz_masking_anisotropic(vec3 v, vec3 h, float ax, float ay)
{
    return step(0.0f, v.z * dot(v, h)) * 2.0f * v.z / (v.z + sqrt(v.z * v.z + ax * ax * v.x * v.x + ay * ay * v.y * v.y));
}

float trowbridge_reitz_masking_isotropic(float vdotn, float vdoth, float a)
{
    return step(0.0f, vdotn * vdoth) * 2.0f * vdotn / (vdotn + sqrt(vdotn * vdotn * (1 - a * a) + a * a));
}

float trowbridge_reitz_masking_isotropic(vec3 v, vec3 h, float a)
{
    return trowbridge_reitz_masking_isotropic(v.z, dot(v, h), a);
}

float ashikhmin_masking_shadowing(float vdotn, float ldotn)
{
    return 1.0f/(4.0f * (ldotn + vdotn - ldotn * vdotn));
}

// =============================================================================
// BSDFs
// =============================================================================

// All BSDFs in raybase return the contribution in two parts, 'reflected' and
// 'transmitted' and 'absorbed'. Generally speaking, 'transmitted' is the part
// of light that has been transmitted through the material (and possibly
// returned back again, through diffuse scattering). 'Reflected' is then the
// part of light that has reflected immediately. 'Absorbed' is the part that
// is unaccounted for and can be supplemented by another BSDF. It's given as
// a return value.
//
// There is a big exception to this: metals. For metals, all contribution is
// put in 'transmitted', even if that isn't true, physically speaking.
//
// Separating these two is important for denoising algorithms, among other uses.
//
// Note: 'transmitted' is _not_ multiplied by the albedo of the material when
// returned from this function. This is vital for denoising as well. You can
// calculate the final contribution of a bsdf as:
//     reflected + transmitted * mat.albedo.rgb

// Basic lambertian diffuse BSDF.
void lambert_bsdf(
    in material mat,
    vec3 light_dir,
    vec3 view_dir,
    out vec3 reflected,
    out vec3 transmitted
){
    reflected = vec3(0);
    transmitted = vec3(max(light_dir.z, 0)) / M_PI;
    if(HAS_MATERIAL(TRANSMISSION))
    {
        transmitted *= (1.0 - mat.transmission);
        transmitted += max(-light_dir.z, 0) * mat.transmission * mat.translucency / M_PI;
    }
}

// https://mimosa-pudica.net/improved-oren-nayar.html
void oren_nayar_bsdf(
    in material mat,
    vec3 light_dir,
    vec3 view_dir,
    out vec3 reflected,
    out vec3 transmitted
){
    reflected = vec3(0);
    float denom = (1 + (1.0f/2.0f - 2.0f/(3.0f * M_PI)) * mat.roughness);
    float ldotn = max(light_dir.z, 0);
    float vdotn = max(view_dir.z, 0);
    float ldotv = dot(light_dir, view_dir);
    float s = ldotv - ldotn * vdotn;
    float t = s > 0 ? max(light_dir.z, view_dir.z) : 1;
    float trans = ldotn * (t + mat.roughness * s) / (t * denom * M_PI);
    if(isnan(trans)) trans = ldotn;

    transmitted = vec3(trans);
    if(HAS_MATERIAL(TRANSMISSION))
    {
        transmitted *= (1.0f - mat.transmission);
        // Just lambertian for translucency, I don't know what to do with the visiblity otherwise.
        transmitted += max(-light_dir.z, 0.0f) * mat.transmission * mat.translucency / M_PI;
    }
}

vec3 clearcoat_brdf_core(
    in material mat,
    vec3 light_dir,
    vec3 view_dir,
    vec3 h,
    float distribution,
    out vec3 reflected
){
    const float l_dot_n = dot(light_dir, mat.clearcoat_normal);
    const float v_dot_n = dot(view_dir, mat.clearcoat_normal);
    const float v_dot_h = dot(view_dir, h);
    const float l_dot_h = dot(light_dir, h);
    const float h_dot_n = dot(h, mat.clearcoat_normal);

    float fresnel = fresnel_schlick(max(v_dot_h, 0), mat.f0);
    float geometry = trowbridge_reitz_masking_shadowing_isotropic(
        l_dot_n, l_dot_h, v_dot_n, v_dot_h, mat.clearcoat_roughness
    );

    reflected = vec3(mat.clearcoat * fresnel * geometry * distribution * max(l_dot_n, 0));
    return vec3(1.0f - fresnel * mat.clearcoat);
}

vec3 clearcoat_brdf(
    in material mat,
    vec3 light_dir,
    vec3 view_dir,
    out vec3 reflected
){
    vec3 h = normalize(view_dir + light_dir);
    const float h_dot_n = dot(h, mat.clearcoat_normal);
    float distribution = trowbridge_reitz_distribution_isotropic(h_dot_n, mat.clearcoat_roughness);
    return clearcoat_brdf_core(mat, light_dir, view_dir, h, distribution, reflected);
}

vec3 sheen_brdf(
    in material mat,
    vec3 light_dir,
    vec3 view_dir,
    out vec3 reflected
){
    vec3 h = normalize(view_dir + light_dir);

    const float l_dot_n = light_dir.z;
    const float v_dot_n = view_dir.z;
    const float v_dot_h = dot(view_dir, h);
    const float l_dot_h = dot(light_dir, h);
    const float h_dot_n = dot(h, mat.clearcoat_normal);

    float geometry = ashikhmin_masking_shadowing(v_dot_n, l_dot_n);
    float distribution = charlie_distribution(h_dot_n, mat.sheen_roughness);

    reflected = mat.sheen_color * geometry * distribution * max(l_dot_n, 0);
    float r = sqrt(mat.sheen_roughness);
    float el = texture(material_lut, vec2(l_dot_n, r)).b;
    float ev = texture(material_lut, vec2(v_dot_n, r)).b;
    float sc = max(mat.sheen_color.r, max(mat.sheen_color.g, mat.sheen_color.b));
    return vec3(clamp(1.0f - max(el * sc, ev * sc), 0.0f, 1.0f));
}

vec3 trowbridge_reitz_bsdf_core(
    in material mat,
    vec3 light_dir,
    vec3 view_dir,
    vec3 h,
    float distribution,
    out vec3 reflected,
    out vec3 transmitted
){
    const float l_dot_n = light_dir.z;
    const float v_dot_n = view_dir.z;

    const bool brdf = !HAS_MATERIAL(TRANSMISSION) || l_dot_n > 0;

    float v_dot_h = dot(view_dir, h);
    float l_dot_h = dot(light_dir, h);

    float fresnel = HAS_MATERIAL(TRANSMISSION) ?
        fresnel_schlick_bidir(v_dot_h, mat) :
        fresnel_schlick(v_dot_h, mat.f0);
    float geometry;
    if(HAS_MATERIAL(ANISOTROPY))
    {
        vec2 ar = get_anisotropic_roughness(mat.roughness, mat.anisotropy);
        geometry = trowbridge_reitz_masking_shadowing_anisotropic(
            light_dir, view_dir, h, ar.x, ar.y
        );
    }
    else
    {
        geometry = trowbridge_reitz_masking_shadowing_isotropic(
            light_dir, view_dir, h, mat.roughness
        );
    }

    float energy_compensation = 0;
    if(HAS_MATERIAL(MULTISCATTER))
    {
        float r = sqrt(mat.roughness);
        float e_v_dot_n = texture(material_lut, vec2(v_dot_n, r)).a;
        float e_l_dot_n = texture(material_lut, vec2(l_dot_n, r)).a;
        // Numerical fit
        float e_avg = 1.0f - 0.51f * mat.roughness;
        float f_avg = (1 + 20 * mix(mat.f0, 1.0f, mat.metallic)) / 21;
        // TODO: Energy compensation for BTDF
        energy_compensation = mat.roughness < 0.07f || !brdf ? 0 :
            ((1-e_v_dot_n)*(1-e_l_dot_n)*f_avg*f_avg*e_avg)/
            (M_PI * (1-e_avg)*(1-f_avg*(1-e_avg)));
    }

    float transmission = HAS_MATERIAL(TRANSMISSION) ? mat.transmission * (1.0f - mat.translucency) : 0;
    if(brdf)
    { // BRDF
        float specular = geometry * distribution;
        reflected = vec3((fresnel * specular + energy_compensation) * (1.0f - mat.metallic) * max(l_dot_n, 0));
        transmitted = vec3((specular + energy_compensation) * mat.metallic * max(l_dot_n, 0));
    }
    else
    { // BTDF
        float denom = mat.eta * v_dot_h + l_dot_h;
        reflected = vec3(0);
        transmitted = vec3(
            transmission * (energy_compensation + (1.0f - mat.metallic) * abs(v_dot_h * l_dot_h) *
            (1.0f - fresnel) * 4.0f * geometry * distribution / (denom * denom)) * -l_dot_n
        );
    }
    return vec3((1.0f - fresnel) * (1.0f - mat.metallic) * (1.0f - transmission));
}

vec3 trowbridge_reitz_bsdf(
    in material mat,
    vec3 light_dir,
    vec3 view_dir,
    out vec3 reflected,
    out vec3 transmitted
){
    const bool brdf = !HAS_MATERIAL(TRANSMISSION) || light_dir.z > 0;

    vec3 h;
    if(brdf) h = normalize(view_dir + light_dir);
    else h = sign(mat.eta - 1.0f) * normalize(light_dir + mat.eta * view_dir);

    float distribution;
    if(HAS_MATERIAL(ANISOTROPY))
    {
        vec2 ar = get_anisotropic_roughness(mat.roughness, mat.anisotropy);
        distribution = trowbridge_reitz_distribution_anisotropic(h, ar.x, ar.y);
    }
    else
    {
        distribution = trowbridge_reitz_distribution_isotropic(h, mat.roughness);
    }
    return trowbridge_reitz_bsdf_core(mat, light_dir, view_dir, h, distribution, reflected, transmitted);
}

// This BSDF is a combination of all that are specified in 'material'.
void full_bsdf(
    in material mat,
    vec3 light_dir,
    vec3 view_dir,
    out vec3 reflected,
    out vec3 transmitted
){
    reflected = vec3(0);
    transmitted = vec3(0);
    vec3 attenuation = vec3(1);
    if(HAS_MATERIAL(CLEARCOAT))
    {
        vec3 local_reflected;
        vec3 local_attenuation = clearcoat_brdf(
            mat,
            light_dir,
            view_dir,
            local_reflected
        );
        reflected += local_reflected * attenuation;
        attenuation *= local_attenuation;
    }
    if(HAS_MATERIAL(SHEEN))
    {
        vec3 local_reflected;
        vec3 local_attenuation = sheen_brdf(
            mat,
            light_dir,
            view_dir,
            local_reflected
        );
        reflected += local_reflected * attenuation;
        attenuation *= local_attenuation;
    }
    if(HAS_MATERIAL(METALLIC_ROUGHNESS))
    {
        vec3 local_reflected;
        vec3 local_transmitted;
        vec3 local_attenuation = trowbridge_reitz_bsdf(
            mat,
            light_dir,
            view_dir,
            local_reflected,
            local_transmitted
        );
        reflected += local_reflected * attenuation;
        transmitted += local_transmitted * attenuation;
        attenuation *= local_attenuation;
    }
    vec3 local_reflected;
    vec3 local_transmitted;
    oren_nayar_bsdf(mat, light_dir, view_dir, local_reflected, local_transmitted);
    reflected += local_reflected * attenuation;
    transmitted += local_transmitted * attenuation;
}

float trowbridge_reitz_bsdf_indirect(
    in material mat,
    vec2 bi,
    float v_dot_n,
    out vec3 reflection,
    out vec3 refraction
){
    // The fresnel value must be attenuated, because we are actually
    // integrating over all directions instead of just one specific
    // direction here. This is an approximated function, though.
    float fresnel = HAS_MATERIAL(TRANSMISSION) ?
        fresnel_schlick_attenuated_bidir(v_dot_n, mat) :
        fresnel_schlick_attenuated(v_dot_n, mat.f0, mat.roughness);

    reflection = mix(vec3(fresnel * bi.x + bi.y), mat.albedo.rgb, mat.metallic);
    float transmission = HAS_MATERIAL(TRANSMISSION) ? mat.transmission * (1.0f - mat.translucency) : 0;
    if(HAS_MATERIAL(TRANSMISSION))
        refraction = (1.0f - fresnel) * (1.0f - mat.metallic) * transmission * mat.albedo.rgb;
    else
        refraction = vec3(0);

    return (1.0f - fresnel) * (1.0f - transmission) * (1.0f - mat.metallic);
}

float clearcoat_brdf_indirect(
    in material mat,
    vec2 bi,
    float v_dot_n,
    out vec3 reflection
){
    float fresnel = fresnel_schlick_attenuated(v_dot_n, mat.f0, mat.clearcoat_roughness);
    reflection = vec3((fresnel * bi.x + bi.y) * mat.clearcoat);
    return (1.0f - fresnel * mat.clearcoat);
}


// =============================================================================
// BSDF importance sampling
// =============================================================================

// Derived from:
// https://arxiv.org/pdf/2306.05044.pdf
vec3 ggx_vndf_sample(vec3 view, vec2 roughness, vec2 u)
{
    if(roughness.x < ZERO_ROUGHNESS_LIMIT || roughness.y < ZERO_ROUGHNESS_LIMIT)
        return vec3(0,0,1);

    vec3 v = normalize(vec3(roughness * view.xy, view.z));

    float phi = 2.0f * M_PI * u.x;

    float z = fma((1.0f - u.y), (1.0f + v.z), -v.z);
    float sin_theta = sqrt(clamp(1.0f - z * z, 0.0f, 1.0f));
    float x = sin_theta * cos(phi);
    float y = sin_theta * sin(phi);
    vec3 h = vec3(x, y, z) + v;

    return normalize(vec3(roughness * h.xy, max(0.0, h.z)));
}

struct full_bsdf_sampling_probabilities
{
    float clearcoat_brdf_probability;

    float trowbridge_reitz_brdf_probability;
    float trowbridge_reitz_btdf_probability;

    float diffuse_brdf_probability;
    // AKA translucency
    float diffuse_btdf_probability;
};

full_bsdf_sampling_probabilities full_bsdf_sampling_probability_heuristic(
    material mat, vec3 view
){
    full_bsdf_sampling_probabilities ret;
    float leftover_probability = 1.0f;

    if(HAS_MATERIAL(CLEARCOAT) && mat.clearcoat > 0.0f)
    {
        float fresnel = fresnel_schlick_attenuated_bidir(dot(view, mat.clearcoat_normal), mat);
        ret.clearcoat_brdf_probability = mat.clearcoat * fresnel * leftover_probability;
        leftover_probability -= ret.clearcoat_brdf_probability;
    }
    else ret.clearcoat_brdf_probability = 0.0f;

    if(HAS_MATERIAL(METALLIC_ROUGHNESS))
    {
        float fresnel = fresnel_schlick_attenuated_bidir(view.z, mat);
        float albedo = luminance(mat.albedo.rgb);
        fresnel = mix(1.0f, fresnel, albedo * (1.0f-mat.metallic));
        float transmission = HAS_MATERIAL(TRANSMISSION) ? mat.transmission * (1.0f - mat.translucency) : 0;
        ret.trowbridge_reitz_brdf_probability = fresnel * leftover_probability;
        ret.trowbridge_reitz_btdf_probability =
            (1.0f-fresnel) * leftover_probability * transmission;
        leftover_probability -=
            ret.trowbridge_reitz_brdf_probability +
            ret.trowbridge_reitz_btdf_probability;
    }
    else
    {
        ret.trowbridge_reitz_brdf_probability = 0.0f;
        ret.trowbridge_reitz_btdf_probability = 0.0f;
    }

    ret.diffuse_brdf_probability = leftover_probability;
    if(HAS_MATERIAL(TRANSMISSION))
    {
        // AKA translucency
        ret.diffuse_btdf_probability = leftover_probability * mat.transmission * mat.translucency;
        ret.diffuse_brdf_probability -= ret.diffuse_btdf_probability;
    }
    else ret.diffuse_btdf_probability = 0.0f;

    return ret;
}

// Returns a sample direction and indicates if that direction is an extremity
// whose pdf reaches infinity (this happens with perfectly smooth materials).
vec3 full_bsdf_select_sample(
    vec3 u,
    material mat,
    vec3 view,
    out vec3 h,
    out bool extremity,
    full_bsdf_sampling_probabilities prob
){
    if(HAS_MATERIAL(CLEARCOAT) && u.z < prob.clearcoat_brdf_probability)
    {
        extremity = mat.clearcoat_roughness < ZERO_ROUGHNESS_LIMIT;
        mat3 clearcoat_tangent_space = create_tangent_space(mat.clearcoat_normal);
        vec3 cview = view * clearcoat_tangent_space;
        cview.z = max(cview.z, 1e-6f);
        cview = normalize(cview);
        h = clearcoat_tangent_space * ggx_vndf_sample(cview, vec2(mat.clearcoat_roughness), u.xy);
        return reflect(-view, h);
    }
    else u.z -= prob.clearcoat_brdf_probability;

    if(HAS_MATERIAL(METALLIC_ROUGHNESS))
    {
        extremity = mat.roughness < ZERO_ROUGHNESS_LIMIT;
        vec2 a = get_anisotropic_roughness(mat.roughness, mat.anisotropy);
        if(u.z < prob.trowbridge_reitz_brdf_probability)
        {
            h = ggx_vndf_sample(view, a, u.xy);
            return reflect(-view, h);
        }
        else u.z -= prob.trowbridge_reitz_brdf_probability;

        if(HAS_MATERIAL(TRANSMISSION) && u.z < prob.trowbridge_reitz_btdf_probability)
        {
            h = ggx_vndf_sample(view, a, u.xy);
            return refract(-view, h, mat.eta);
        }
        else u.z -= prob.trowbridge_reitz_btdf_probability;
    }

    extremity = false;

    vec3 dir = cosine_hemisphere_sample(u.xy);
    h = normalize(view + dir);
    if(HAS_MATERIAL(TRANSMISSION) && u.z < prob.diffuse_btdf_probability)
        dir = -dir;
    return dir;
}

void full_bsdf_weight(
    material mat,
    vec3 view,
    vec3 h,
    vec3 light,
    bool extremity,
    full_bsdf_sampling_probabilities prob,
    out vec3 reflected,
    out vec3 transmitted,
    out float pdf
){
    reflected = vec3(0);
    transmitted = vec3(0);
    float v_dot_h = dot(view, h);

    pdf = 0.0f;
    vec3 attenuation = vec3(1.0f);
    const float same_dir_epsilon = 1e-4;
    if(HAS_MATERIAL(CLEARCOAT) && mat.clearcoat > 0.0f)
    {
        const float h_dot_n = dot(h, mat.clearcoat_normal);
        const float l_dot_n = dot(light, mat.clearcoat_normal);
        const float v_dot_n = dot(view, mat.clearcoat_normal);
        float distribution = trowbridge_reitz_distribution_isotropic(h_dot_n, mat.clearcoat_roughness);

        if(extremity)
        {
            distribution = direction_equal(
                reflect(-view, mat.clearcoat_normal), light, same_dir_epsilon
            ) && mat.clearcoat_roughness < ZERO_ROUGHNESS_LIMIT ? 4 * l_dot_n * v_dot_n : 0.0f;
        }

        vec3 local_reflected;
        vec3 local_attenuation = clearcoat_brdf_core(mat, light, view, h, distribution, local_reflected);
        reflected += local_reflected * attenuation;
        attenuation *= local_attenuation;

        float geom1 = trowbridge_reitz_masking_isotropic(
            max(v_dot_n, 1e-6), v_dot_h, mat.clearcoat_roughness
        );
        pdf += geom1 * distribution / (4.0f * v_dot_n) * prob.clearcoat_brdf_probability;
    }
    if(HAS_MATERIAL(SHEEN) && !extremity)
    {
        vec3 local_reflected;
        vec3 local_attenuation = sheen_brdf(mat, light, view, local_reflected);
        reflected += local_reflected * attenuation;
        attenuation *= local_attenuation;
    }
    if(HAS_MATERIAL(METALLIC_ROUGHNESS))
    {
        float distribution;

        float geom1;
        if(HAS_MATERIAL(ANISOTROPY))
        {
            vec2 ar = get_anisotropic_roughness(mat.roughness, mat.anisotropy);
            distribution = trowbridge_reitz_distribution_anisotropic(h, ar.x, ar.y);
            geom1 = trowbridge_reitz_masking_anisotropic(view, h, ar.x, ar.y);
        }
        else
        {
            distribution = trowbridge_reitz_distribution_isotropic(h, mat.roughness);
            geom1 = trowbridge_reitz_masking_isotropic(view, h, mat.roughness);
        }

        if(extremity)
        {
            vec3 sharp_dir = light.z > 0 ? vec3(-view.xy, view.z) : refract(-view, vec3(0,0,1), mat.eta);
            distribution = direction_equal(
                sharp_dir, light, same_dir_epsilon
            ) && mat.roughness < ZERO_ROUGHNESS_LIMIT ? abs(4 * light.z * view.z) : 0.0f;
        }

        float l_dot_h = dot(light, h);
        vec3 local_reflected;
        vec3 local_transmitted;
        vec3 local_attenuation = trowbridge_reitz_bsdf_core(mat, light, view, h, distribution, local_reflected, local_transmitted);
        reflected += local_reflected * attenuation;
        transmitted += local_transmitted * attenuation;
        attenuation *= local_attenuation;

        if(light.z >= 0)
        {
            pdf += geom1 * distribution / (4.0f * view.z) * prob.trowbridge_reitz_brdf_probability;
        }
        else if(HAS_MATERIAL(TRANSMISSION))
        {
            float ht = mat.eta * v_dot_h + l_dot_h;
            pdf += step(0, -light.z) * abs(v_dot_h * l_dot_h) * geom1 *
                distribution / (abs(view.z) * ht * ht) * prob.trowbridge_reitz_btdf_probability;
        }
    }

    if(!extremity)
    {
        vec3 local_reflected;
        vec3 local_transmitted;
        oren_nayar_bsdf(mat, light, view, local_reflected, local_transmitted);
        reflected += local_reflected * attenuation;
        transmitted += local_transmitted * attenuation;

        float diffuse_pdf = cosine_hemisphere_pdf(light);
        pdf += max(diffuse_pdf, 0.0f) * prob.diffuse_brdf_probability;
        if(HAS_MATERIAL(TRANSMISSION))
            pdf += max(-diffuse_pdf, 0.0f) * prob.diffuse_btdf_probability;
    }
}

vec3 full_bsdf_sample(
    vec3 u,
    material mat,
    vec3 view,
    out vec3 light,
    out float pdf
){
#if 1
    full_bsdf_sampling_probabilities prob = full_bsdf_sampling_probability_heuristic(mat, view);
    vec3 h;

    bool extremity = false;
    light = full_bsdf_select_sample(u, mat, view, h, extremity, prob);

    vec3 reflected;
    vec3 transmitted;
    pdf = 0.0f;
    full_bsdf_weight(mat, view, h, light, extremity, prob, reflected, transmitted, pdf);

    if(extremity)
    {
        // If at extremity, we can pre-apply the PDF.
        reflected = pdf <= 1e-8f ? vec3(0.0f) : reflected / pdf;
        transmitted = pdf <= 1e-8f ? vec3(0.0f) : transmitted / pdf;
        pdf = 0;
    }
    return reflected + transmitted * mat.albedo.rgb;
#else
    light = uniform_hemisphere_sample(u.xy);
    pdf = uniform_hemisphere_pdf(light);

    vec3 reflected;
    vec3 transmitted;
    full_bsdf(mat, light, view, reflected, transmitted);
    //lambert_bsdf(mat, light, view, reflected, transmitted);
    //reflected = vec3(0);
    //transmitted = vec3(max(light.z, 0) / M_PI);
    return reflected + transmitted * mat.albedo.rgb;
#endif
}

float full_bsdf_pdf(
    in material mat,
    vec3 light,
    vec3 view,
    out vec3 reflected,
    out vec3 transmitted
){
#if 1
    vec3 h;
    if(light.z > 0) h = normalize(view + light);
    else h = sign(mat.eta - 1.0f) * normalize(light + mat.eta * view);

    // The 'u' does not matter because we don't care about the half-vectors here.
    full_bsdf_sampling_probabilities prob = full_bsdf_sampling_probability_heuristic(mat, view);

    float pdf = 0.0f;
    full_bsdf_weight(mat, view, h, light, false, prob, reflected, transmitted, pdf);

    return pdf;
#else
    full_bsdf(mat, light, view, reflected, transmitted);
    //lambert_bsdf(mat, light, view, reflected, transmitted);
    //reflected = vec3(0);
    //transmitted = vec3(max(light.z, 0) / M_PI);
    return uniform_hemisphere_pdf(light);
#endif
}

#endif
