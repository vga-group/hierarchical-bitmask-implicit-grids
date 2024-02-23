#include "gltf.hh"
#include "core/error.hh"
#include "gfx/texture.hh"
#include "gfx/primitive.hh"
#include "gfx/sampler.hh"
#include "gfx/decal.hh"
#include "gfx/vulkan_helpers.hh"
#include "gfx/scene_stage.hh"
#include "external/tiny_gltf.h"
#include "gfx/external/stb_image.h"
#include "phys/collider.hh"
#include "phys/skeleton_collider.hh"
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <iostream>

namespace
{
using namespace rb;
using namespace rb::gfx;
using namespace rb::phys;
using namespace rb::extra;

void flip_lines(unsigned pitch, unsigned char* line_a, unsigned char* line_b)
{
    for(unsigned i = 0; i < pitch; ++i)
    {
        unsigned char tmp = line_a[i];
        line_a[i] = line_b[i];
        line_b[i] = tmp;
    }
}

void flip_vector_image(std::vector<unsigned char>& image, unsigned height)
{
    unsigned pitch = image.size() / height;
    for(unsigned i = 0; i < height/2; ++i)
        flip_lines(
            pitch,
            image.data()+i*pitch,
            image.data()+(height-1-i)*pitch
        );
}

bool check_opaque(tinygltf::Image& img)
{
    if(img.component != 4) return true;
    if(img.bits == 8)
    {
        // Check that every fourth (alpha) value is 255.
        for(size_t i = 3; i < img.image.size(); i += 4)
            if(img.image[i] != 255)
                return false;
        return true;
    }
    return false;
}

template<typename T>
vec4 vector_to_vec4(const std::vector<T>& v, float fill_value = 0.0f)
{
    vec4 ret(fill_value);
    for(size_t i = 0; i < min(v.size(), 4lu); ++i)
        ret[i] = v[i];
    return ret;
}

template<typename T>
void cast_gltf_data(uint8_t* data, int componentType, int, T& out)
{
    switch(componentType)
    {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        out = *reinterpret_cast<int8_t*>(data);
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        out = *reinterpret_cast<uint8_t*>(data);
        break;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        out = *reinterpret_cast<int16_t*>(data);
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        out = *reinterpret_cast<uint16_t*>(data);
        break;
    case TINYGLTF_COMPONENT_TYPE_INT:
        out = *reinterpret_cast<int32_t*>(data);
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        out = *reinterpret_cast<uint32_t*>(data);
        break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        out = *reinterpret_cast<float*>(data);
        break;
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
        out = *reinterpret_cast<double*>(data);
        break;
    }
}

template<int size, typename T, glm::qualifier Q>
void cast_gltf_data(
    uint8_t* data,
    int componentType,
    int type,
    vec<size, T, Q>& out
){
    int component_size = tinygltf::GetComponentSizeInBytes(componentType);
    int components = tinygltf::GetNumComponentsInType(type);
    out = glm::vec<size, T, Q>(0);
    for(int i = 0; i < min(components, (int)size); ++i)
        cast_gltf_data(data+i*component_size, componentType, type, out[i]);
}

template<int C, int R, typename T>
void cast_gltf_data(
    uint8_t* data, int componentType, int type, mat<C, R, T>& out
){
    int component_size = tinygltf::GetComponentSizeInBytes(componentType);
    int components = tinygltf::GetNumComponentsInType(type);
    out = glm::mat<C, R, T>(1);
    for(int x = 0; x < C; ++x)
    for(int y = 0; y < R; ++y)
    {
        int i = y+x*C;
        if(i < components)
            cast_gltf_data(
                data+i*component_size, componentType, type, out[x][y]
            );
    }
}

void cast_gltf_data(
    uint8_t* data, int componentType, int type, quat& out
){
    vec4 tmp;
    cast_gltf_data(data, componentType, type, tmp);
    out = quat(tmp.w, tmp.x, tmp.y, tmp.z);
}

template<typename T>
std::vector<T> read_accessor(tinygltf::Model& model, int index)
{
    tinygltf::Accessor& accessor = model.accessors[index];
    tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    tinygltf::Buffer& buf = model.buffers[view.buffer];

    std::vector<T> res(accessor.count);
    int stride = accessor.ByteStride(view);

    size_t offset = view.byteOffset;
    for(T& entry: res)
    {
        uint8_t* data = buf.data.data() + offset;
        cast_gltf_data(data, accessor.componentType, accessor.type, entry);
        offset += stride;
    }
    return res;
}

template<typename T>
std::vector<animation_sample<T>> read_animation_accessors(
    tinygltf::Model& model, int input, int output, bool expect_tangents
){
    std::vector<animation_sample<T>> res;

    std::vector<float> timestamps = read_accessor<float>(model, input);
    std::vector<T> data = read_accessor<T>(model, output);

    res.resize(timestamps.size());
    for(size_t i = 0; i < res.size(); ++i)
    {
        // Convert timestamps into microseconds
        res[i].timestamp = round(timestamps[i]*1000000);
        if(expect_tangents)
        {
            res[i].in_tangent = data[i*3];
            res[i].data = data[i*3+1];
            res[i].out_tangent = data[i*3+2];
        }
        else res[i].data = data[i];
    }

    return res;
}

template<typename T>
std::vector<animation_sample<std::vector<T>>> read_animation_accessors_vector(
    tinygltf::Model& model, int input, int output, bool expect_tangents
){
    std::vector<animation_sample<std::vector<T>>> res;

    std::vector<float> timestamps = read_accessor<float>(model, input);
    std::vector<T> data = read_accessor<T>(model, output);

    size_t out_vec_size = data.size() / timestamps.size();
    if(expect_tangents) out_vec_size /= 3;

    res.resize(timestamps.size());
    for(size_t i = 0, j = 0; i < res.size(); ++i)
    {
        auto& r = res[i];
        // Convert timestamps into microseconds
        r.timestamp = round(timestamps[i]*1000000);
        if(expect_tangents)
        {
            r.in_tangent.insert(
                r.in_tangent.begin(),
                data.begin() + (i*3+0)*out_vec_size,
                data.begin() + (i*3+1)*out_vec_size
            );
            r.data.insert(
                r.data.begin(),
                data.begin() + (i*3+1)*out_vec_size,
                data.begin() + (i*3+2)*out_vec_size
            );
            r.out_tangent.insert(
                r.out_tangent.begin(),
                data.begin() + (i*3+2)*out_vec_size,
                data.begin() + (i*3+3)*out_vec_size
            );
        }
        else
        {
            r.data.insert(
                r.data.begin(),
                data.begin() + i * out_vec_size,
                data.begin() + (i + 1) * out_vec_size
            );
        }
    }

    return res;
}

void load_gltf_transform(transformable& tnode, transformable* parent, tinygltf::Node& node)
{
    if(node.matrix.size())
        tnode.set_transform(glm::make_mat4(node.matrix.data()));
    else
    {
        if(node.translation.size())
            tnode.set_position((vec3)glm::make_vec3(node.translation.data()));

        if(node.scale.size())
            tnode.set_scaling((vec3)glm::make_vec3(node.scale.data()));

        if(node.rotation.size())
            tnode.set_orientation(glm::make_quat(node.rotation.data()));
    }
    tnode.set_parent(parent);
}

phys::shape* create_convex_hull_shape(
    phys::context* pctx,
    tinygltf::Model& model,
    tinygltf::Mesh& mesh,
    phys::shape::params params
){
    std::vector<vec3> points;
    for(tinygltf::Primitive& primitive: mesh.primitives)
    {
        auto it = primitive.attributes.find("POSITION");
        if(it == primitive.attributes.end()) continue;
        std::vector<vec3> tmp = read_accessor<vec3>(model, it->second);
        points.insert(points.end(), tmp.begin(), tmp.end());
    }
    return new phys::shape(*pctx, phys::shape::convex_hull{std::move(points)}, params);
}

phys::shape* create_mesh_shape(
    phys::context* pctx,
    tinygltf::Model& model,
    tinygltf::Mesh& mesh,
    phys::shape::params params,
    bool bvh
){
    std::vector<uint32_t> indices;
    std::vector<vec3> points;
    for(tinygltf::Primitive& primitive: mesh.primitives)
    {
        auto it = primitive.attributes.find("POSITION");
        if(it == primitive.attributes.end()) continue;

        uint32_t index_offset = points.size();
        std::vector<vec3> tmp = read_accessor<vec3>(model, it->second);
        points.insert(points.end(), tmp.begin(), tmp.end());

        std::vector<uint32_t> tmp_indices;
        if(primitive.indices >= 0)
        {
            tmp_indices = read_accessor<uint32_t>(model, primitive.indices);
            for(uint32_t& ind: tmp_indices) ind += index_offset;
        }
        else
        {
            tmp_indices.resize(tmp.size());
            std::iota(tmp_indices.begin(), tmp_indices.end(), index_offset);
        }
        indices.insert(indices.end(), tmp_indices.begin(), tmp_indices.end());
    }
    return bvh ?
        new phys::shape(
            *pctx,
            phys::shape::static_mesh{std::move(indices), std::move(points)},
            params
        ) :
        new phys::shape(
            *pctx,
            phys::shape::dynamic_mesh{std::move(indices), std::move(points)},
            params
        );
}

void load_gltf_constraint(
    tinygltf::Model& model,
    int i,
    std::unordered_map<int, entity>& node_to_entity,
    rb::scene& e
){
    tinygltf::Node& node = model.nodes[i];
    tinygltf::Value* engine_data =
        node.extensions.count("RB_engine_data") ?
        &node.extensions["RB_engine_data"] : nullptr;
    if(!engine_data || !engine_data->Has("constraint")) return;
    tinygltf::Value c = engine_data->Get("constraint");

    entity node_entity = node_to_entity[i];
    transformable* c_it = e.get<transformable>(node_entity);

    int o1_index = c.Get("object1").GetNumberAsInt();
    int o2_index = c.Get("object2").GetNumberAsInt();
    RB_CHECK(
        !node_to_entity.count(o1_index) || !node_to_entity.count(o2_index),
        "Specified constraint endpoints are not objects!"
    );

    entity o1_entity = node_to_entity[o1_index];
    entity o2_entity = node_to_entity[o2_index];
    transformable* o1_it = e.get<transformable>(o1_entity);
    transformable* o2_it = e.get<transformable>(o2_entity);

    rb::phys::constraint con;

    std::string type = c.Get("type").Get<std::string>();
    if(type == "FIXED") con.set_type(rb::phys::constraint::FIXED);
    else if(type == "POINT") con.set_type(rb::phys::constraint::POINT);
    else if(type == "HINGE") con.set_type(rb::phys::constraint::HINGE);
    else if(type == "SLIDER") con.set_type(rb::phys::constraint::SLIDER);
    else if(type == "PISTON") con.set_type(rb::phys::constraint::PISTON);
    else if(type == "GENERIC") con.set_type(rb::phys::constraint::GENERIC);
    else if(type == "GENERIC_SPRING") con.set_type(rb::phys::constraint::GENERIC_SPRING);
    else RB_PANIC("Unknown constraint type ", type);

    static const quat blender_to_gl = quat(0.707106781, -0.707106781, 0, 0);
    vec3 c_pos = c_it->get_global_position();
    quat c_ori = c_it->get_global_orientation();
    con.set_collider(
        0, o1_entity,
        rb::phys::constraint::frame{
            c_pos-o1_it->get_global_position(),
            inverse(o1_it->get_global_orientation()) * c_ori * blender_to_gl
        }
    );
    con.set_collider(
        1, o2_entity,
        rb::phys::constraint::frame{
            c_pos-o2_it->get_global_position(),
            inverse(o2_it->get_global_orientation()) * c_ori * blender_to_gl
        }
    );
    con.set_params(rb::phys::constraint::params{
        c.Get("disable_collisions").Get<bool>(),
        bvec3(
            c.Get("use_limit_ang_x").Get<bool>(),
            c.Get("use_limit_ang_y").Get<bool>(),
            c.Get("use_limit_ang_z").Get<bool>()
        ),
        vec3(
            degrees(c.Get("limit_ang_x_lower").GetNumberAsDouble()),
            degrees(c.Get("limit_ang_y_lower").GetNumberAsDouble()),
            degrees(c.Get("limit_ang_z_lower").GetNumberAsDouble())
        ),
        vec3(
            degrees(c.Get("limit_ang_x_upper").GetNumberAsDouble()),
            degrees(c.Get("limit_ang_y_upper").GetNumberAsDouble()),
            degrees(c.Get("limit_ang_z_upper").GetNumberAsDouble())
        ),
        bvec3(
            c.Get("use_limit_lin_x").Get<bool>(),
            c.Get("use_limit_lin_y").Get<bool>(),
            c.Get("use_limit_lin_z").Get<bool>()
        ),
        vec3(
            c.Get("limit_lin_x_lower").GetNumberAsDouble(),
            c.Get("limit_lin_y_lower").GetNumberAsDouble(),
            c.Get("limit_lin_z_lower").GetNumberAsDouble()
        ),
        vec3(
            c.Get("limit_lin_x_upper").GetNumberAsDouble(),
            c.Get("limit_lin_y_upper").GetNumberAsDouble(),
            c.Get("limit_lin_z_upper").GetNumberAsDouble()
        ),
        vec3(
            c.Get("spring_damping_ang_x").GetNumberAsDouble(),
            c.Get("spring_damping_ang_y").GetNumberAsDouble(),
            c.Get("spring_damping_ang_z").GetNumberAsDouble()
        ),
        vec3(
            c.Get("spring_damping_x").GetNumberAsDouble(),
            c.Get("spring_damping_y").GetNumberAsDouble(),
            c.Get("spring_damping_z").GetNumberAsDouble()
        ),
        vec3(
            c.Get("spring_stiffness_ang_x").GetNumberAsDouble(),
            c.Get("spring_stiffness_ang_y").GetNumberAsDouble(),
            c.Get("spring_stiffness_ang_z").GetNumberAsDouble()
        ),
        vec3(
            c.Get("spring_stiffness_x").GetNumberAsDouble(),
            c.Get("spring_stiffness_y").GetNumberAsDouble(),
            c.Get("spring_stiffness_z").GetNumberAsDouble()
        ),
        bvec3(
            c.Get("use_spring_ang_x").Get<bool>(),
            c.Get("use_spring_ang_y").Get<bool>(),
            c.Get("use_spring_ang_z").Get<bool>()
        ),
        bvec3(
            c.Get("use_spring_x").Get<bool>(),
            c.Get("use_spring_y").Get<bool>(),
            c.Get("use_spring_z").Get<bool>()
        )
    });
    e.add(
        gltf_node{node.name, &model},
        std::move(con)
    );
}

collider create_gltf_collider(
    tinygltf::Value* engine_data,
    shape* s,
    transformable* tnode,
    bool is_bone_collider
){
    const tinygltf::Value& col_data = engine_data->Get("collision");
    phys::shape::params params = {
        (float)col_data.Get("margin").GetNumberAsDouble(),
        tnode->get_global_scaling()
    };
    if(is_bone_collider)
    {
        params.origin = tnode->get_position();
        params.orientation = tnode->get_orientation();
    }
    s->reset(params);

    std::string type = col_data.Get("type").Get<std::string>();
    bool kinematic = col_data.Get("kinematic").Get<bool>();
    float friction = col_data.Get("friction").GetNumberAsDouble();
    float restitution = col_data.Get("restitution").GetNumberAsDouble();
    float linear_damping =
        col_data.Get("linear_damping").GetNumberAsDouble();
    float angular_damping =
        col_data.Get("angular_damping").GetNumberAsDouble();
    float mass = max(col_data.Get("mass").GetNumberAsDouble(), 0.0);
    bool use_deactivation = col_data.Has("use_deactivation") ?
        col_data.Get("use_deactivation").Get<bool>() : true;
    uint32_t categories = phys::collider::DYNAMIC;
    uint32_t mask = phys::collider::ALL;

    if(mass == 0 || type == "PASSIVE")
    {
        categories = (kinematic ? phys::collider::KINEMATIC : phys::collider::STATIC);
        mask = phys::collider::DYNAMIC;
    }

    phys::collider c(s, categories, mask, mass);
    c.set_friction(friction);
    c.set_restitution(restitution);
    c.set_linear_damping(linear_damping);
    c.set_angular_damping(angular_damping);
    c.set_use_deactivation(use_deactivation);
    return c;
}

bool file_exists_adapter(const std::string& name, void* userdata)
{
    filesystem* fs = static_cast<filesystem*>(userdata);
    return fs->exists(name);
}

bool read_file_adapter(
    std::vector<unsigned char>* data,
    std::string* err,
    const std::string& name,
    void* userdata
){
    filesystem* fs = static_cast<filesystem*>(userdata);
    try
    {
        file f = fs->get(name);
        data->resize(f.get_size());
        memcpy(data->data(), f.get_data(), f.get_size());
        return true;
    }
    catch(std::runtime_error& ex)
    {
        *err = ex.what();
        return false;
    }
}

struct node_meta_info
{
    std::unordered_set<int /*node*/> joints;
    std::unordered_map<int /*node*/, rigid_animation_pool*> rigid_animations;
    std::unordered_map<int /*node*/, mesh::animation_pool*> morph_target_animations;
    std::unordered_map<int /*node*/, phys::shape*> shapes;

    struct joint_meta_info
    {
        mat4 inverse_bind_matrix = mat4(1);
    };

    struct skeleton_meta_info
    {
        std::unordered_map<int /*node*/, int /*joint index*/> joints;
        int root_node;
        std::vector<joint_meta_info> true_joints;
        // Fake joints, ones that actually aren't joints but are part of the
        // hierarchy anyway.
        int false_joints;
    };
    std::unordered_map<int /*skin*/, skeleton_meta_info> skeleton_info;
    std::unordered_map<int /*skin*/, skeleton> skeletons;
    std::unordered_map<int /*skin*/, skeleton_collider> skeleton_colliders;
    std::unordered_map<std::string /*name*/, model> models;
};


material::sampler_tex get_texture(
    tinygltf::Model& model,
    std::vector<std::unique_ptr<sampler>>& samplers,
    std::vector<std::unique_ptr<texture>>& textures,
    int index
){
    if(index == -1) return {nullptr, nullptr};
    int sampler_index = model.textures[index].sampler;
    sampler* s = samplers.back().get();
    if(sampler_index >= 0)
        samplers[sampler_index].get();
    return {s, textures[model.textures[index].source].get() };
}

material create_material(
    tinygltf::Material& mat,
    tinygltf::Model& model,
    std::vector<std::unique_ptr<sampler>>& samplers,
    std::vector<std::unique_ptr<texture>>& textures
){
    material m;
    m.color = vector_to_vec4(mat.pbrMetallicRoughness.baseColorFactor);
    m.color_texture = get_texture(model, samplers, textures, mat.pbrMetallicRoughness.baseColorTexture.index);

    m.metallic = mat.pbrMetallicRoughness.metallicFactor;
    m.roughness = mat.pbrMetallicRoughness.roughnessFactor;

    m.metallic_roughness_texture = get_texture(
        model, samplers, textures, mat.pbrMetallicRoughness.metallicRoughnessTexture.index
    );

    m.normal_texture = get_texture(model, samplers, textures, mat.normalTexture.index);

    m.ior = 1.5f;
    if(mat.extensions.count("KHR_materials_ior"))
    {
        const tinygltf::Value& ior_ext = mat.extensions["KHR_materials_ior"];
        if(ior_ext.Has("ior"))
            m.ior = ior_ext.Get("ior").GetNumberAsDouble();
    }

    m.emission = vector_to_vec4(mat.emissiveFactor);
    m.emission_texture = get_texture(model, samplers, textures, mat.emissiveTexture.index);

    if(mat.extensions.count("KHR_materials_emissive_strength"))
    {
        const tinygltf::Value& emissive_ext = mat.extensions["KHR_materials_emissive_strength"];
        if(emissive_ext.Has("emissiveStrength"))
            m.emission *= emissive_ext.Get("emissiveStrength").GetNumberAsDouble();
    }

    if(mat.extensions.count("KHR_materials_transmission"))
    {
        const tinygltf::Value& transmission_ext = mat.extensions["KHR_materials_transmission"];
        if(transmission_ext.Has("transmissionFactor"))
            m.transmission = transmission_ext.Get("transmissionFactor").GetNumberAsDouble();
        if(transmission_ext.Has("transmissionTexture"))
            m.transmission_translucency_texture = get_texture(
                model, samplers, textures,
                transmission_ext.Get("transmissionTexture").Get("index").GetNumberAsInt()
            );
    }

    if(mat.extensions.count("KHR_materials_volume"))
    {
        const tinygltf::Value& volume_ext = mat.extensions["KHR_materials_volume"];
        double attenuation_distance = INFINITY;
        dvec3 attenuation_color = dvec3(1);
        if(volume_ext.Has("attenuationDistance"))
            attenuation_distance =
                volume_ext.Get("attenuationDistance").GetNumberAsDouble();
        if(volume_ext.Has("attenuationColor"))
            attenuation_color = dvec3(
                volume_ext.Get("attenuationColor").Get(0).GetNumberAsDouble(),
                volume_ext.Get("attenuationColor").Get(1).GetNumberAsDouble(),
                volume_ext.Get("attenuationColor").Get(2).GetNumberAsDouble()
            );

        m.volume_attenuation = pow(attenuation_color, dvec3(1.0/attenuation_distance));
    }

    if(mat.extensions.count("KHR_materials_clearcoat"))
    {
        const tinygltf::Value& clearcoat_ext = mat.extensions["KHR_materials_clearcoat"];
        int clearcoat_texture = -1;
        int clearcoat_roughness_texture = -1;
        if(clearcoat_ext.Has("clearcoatFactor"))
            m.clearcoat = clearcoat_ext.Get("clearcoatFactor").GetNumberAsDouble();
        if(clearcoat_ext.Has("clearcoatTexture"))
            clearcoat_texture = clearcoat_ext.Get("clearcoatTexture").Get("index").GetNumberAsInt();
        if(clearcoat_ext.Has("clearcoatRoughnessFactor"))
            m.clearcoat_roughness = clearcoat_ext.Get("clearcoatRoughnessFactor").GetNumberAsDouble();
        if(clearcoat_ext.Has("clearcoatRoughnessTexture"))
            clearcoat_roughness_texture = clearcoat_ext.Get("clearcoatRoughnessTexture").Get("index").GetNumberAsInt();
        if(clearcoat_ext.Has("clearcoatNormalTexture"))
            m.clearcoat_normal_texture = get_texture(
                model, samplers, textures,
                clearcoat_ext.Get("clearcoatNormalTexture").Get("index").GetNumberAsInt()
            );

        RB_CHECK(
            clearcoat_texture != clearcoat_roughness_texture,
            "Clearcoat color and roughness textures must be the same!"
        );
        if(clearcoat_texture != -1)
            m.clearcoat_texture = get_texture(
                model, samplers, textures, clearcoat_texture
            );
    }

    // No extension for anisotropy yet...

    if(mat.extensions.count("KHR_materials_sheen"))
    {
        const tinygltf::Value& sheen_ext = mat.extensions["KHR_materials_sheen"];
        int sheen_texture = -1;
        int sheen_roughness_texture = -1;
        if(sheen_ext.Has("sheenColorFactor"))
            m.sheen_color = vec3(
                sheen_ext.Get("sheenColorFactor").Get(0).GetNumberAsDouble(),
                sheen_ext.Get("sheenColorFactor").Get(1).GetNumberAsDouble(),
                sheen_ext.Get("sheenColorFactor").Get(2).GetNumberAsDouble()
            );
        if(sheen_ext.Has("sheenColorTexture"))
            sheen_texture = sheen_ext.Get("sheenColorTexture").Get("index").GetNumberAsInt();
        if(sheen_ext.Has("sheenRoughnessFactor"))
            m.sheen_roughness = sheen_ext.Get("sheenRoughnessFactor").GetNumberAsDouble();
        if(sheen_ext.Has("sheenRoughnessTexture"))
            sheen_roughness_texture = sheen_ext.Get("sheenRoughnessTexture").Get("index").GetNumberAsInt();

        RB_CHECK(
            sheen_texture != sheen_roughness_texture,
            "Sheen color and roughness textures must be the same!"
        );
        if(sheen_texture != -1)
            m.sheen_texture = get_texture(model, samplers, textures, sheen_texture);
    }

    m.double_sided = mat.doubleSided;
    if(mat.alphaMode == "MASK")
        m.alpha_cutoff = mat.alphaCutoff;

    return m;
}

primitive::vertex_data load_vertex_data(
    tinygltf::Model& gltf_model,
    const std::map<std::string, int>& attributes,
    aabb* bounding_box = nullptr,
    bool* has_bounding_box = nullptr
){
    primitive::vertex_data vd;
    for(const auto& pair: attributes)
    {
        if(pair.first == "POSITION")
        {
            vd.position = read_accessor<pvec3>(gltf_model, pair.second);
            tinygltf::Accessor& accessor =
                gltf_model.accessors[pair.second];
            if(
                bounding_box &&
                !accessor.minValues.empty() &&
                !accessor.maxValues.empty()
            ){
                bounding_box->min = vector_to_vec4(accessor.minValues);
                bounding_box->max = vector_to_vec4(accessor.maxValues);
                *has_bounding_box = true;
            }
        }
        else if(pair.first == "NORMAL")
            vd.normal = read_accessor<pvec3>(gltf_model, pair.second);
        else if(pair.first == "TANGENT")
            vd.tangent = read_accessor<pvec4>(gltf_model, pair.second);
        else if(pair.first == "TEXCOORD_0")
            vd.texture_uv = read_accessor<pvec2>(gltf_model, pair.second);
        else if(pair.first == "TEXCOORD_1")
            vd.lightmap_uv = read_accessor<pvec2>(gltf_model, pair.second);
        else if(pair.first == "JOINTS_0")
            vd.joints = read_accessor<pivec4>(gltf_model, pair.second);
        else if(pair.first == "WEIGHTS_0")
        {
            vd.weights = read_accessor<pvec4>(gltf_model, pair.second);
            // Some models have blasted and broken weights, so let's just
            // re-normalize them.
            for(pvec4& v: vd.weights)
                v /= max(v.x + v.y + v.z + v.w, 1e-4f);
        }
        else if(pair.first == "COLOR_0")
            vd.color = read_accessor<pvec4>(gltf_model, pair.second);
    }
    return vd;
}

void count_gltf_skeleton_node(
    tinygltf::Model& gltf_model,
    int node_index,
    node_meta_info& meta,
    int skeleton_index,
    bool rooted = false
){
    tinygltf::Node& node = gltf_model.nodes[node_index];

    auto& info = meta.skeleton_info[skeleton_index];

    bool is_joint = info.joints.count(node_index);
    bool local_is_root = false;
    if(!rooted && (node_index == info.root_node || is_joint))
    {
        rooted = true;
        local_is_root = true;
    }

    // Load child nodes
    for(int child_index: node.children)
        count_gltf_skeleton_node(gltf_model, child_index, meta, skeleton_index, rooted);

    if(!is_joint) // If non-joint
    {
        // Add as false joint to skeleton
        info.joints[node_index] = info.true_joints.size() + info.false_joints;
        info.false_joints++;
    }

    if(local_is_root)
        rooted = false;
}

void load_gltf_skeleton_node(
    tinygltf::Model& gltf_model,
    int parent_index,
    int node_index,
    transformable* parent,
    node_meta_info& meta,
    int skeleton_index
){
    tinygltf::Node& node = gltf_model.nodes[node_index];

    auto& info = meta.skeleton_info[skeleton_index];

    tinygltf::Value* engine_data = node.extensions.count("RB_engine_data") ?
        &node.extensions["RB_engine_data"] : nullptr;

    transformable* next_parent = parent;

    // If the joints structure has this index, that means that this node is a
    // joint.
    if(info.joints.count(node_index))
    {
        int joint_index = info.joints[node_index];
        skeleton::joint& joint =
            meta.skeletons.at(skeleton_index).get_joint(joint_index);

        joint.name = node.name;
        joint.root = info.root_node == node_index;

        // Set transformation for node
        load_gltf_transform(joint.node, parent, node);
        next_parent = &joint.node;

        // Come up with some inverse bind matrix for the false joints too.
        if(joint_index >= info.true_joints.size())
            joint.inverse_bind_matrix = inverse(joint.node.get_global_transform());
        else
            joint.inverse_bind_matrix = info.true_joints[joint_index].inverse_bind_matrix;

        if(engine_data && engine_data->Has("joint_constraint"))
        {
            tinygltf::Value jc = engine_data->Get("joint_constraint");
            std::string type = jc.Get("type").Get<std::string>();
            if(type == "NONE") joint.constraint.type = skeleton::joint::NONE;
            else if(type == "FIXED")
                joint.constraint.type = skeleton::joint::FIXED;
            else if(type == "POINT")
                joint.constraint.type = skeleton::joint::POINT;
            else if(type == "CONE_TWIST")
            {
                joint.constraint.type = skeleton::joint::CONE_TWIST;
                joint.constraint.cone_twist.swing_span1 =
                    jc.Get("swing_span1").GetNumberAsDouble();
                joint.constraint.cone_twist.swing_span2 =
                    jc.Get("swing_span2").GetNumberAsDouble();
                joint.constraint.cone_twist.twist_span =
                    jc.Get("twist_span").GetNumberAsDouble();
            }
        }

        // Add animation pool if found.
        auto it = meta.rigid_animations.find(node_index);
        if(it != meta.rigid_animations.end())
            joint.pool = it->second;
    }

    // We may also have a skeleton collider node! We gotta load those too.
    const tinygltf::Value* collision_data =
        engine_data && engine_data->Has("collision") ?
        &engine_data->Get("collision") : nullptr;

    bool is_bone_collider = collision_data &&
        collision_data->Has("bone_parent") ?
        collision_data->Get("bone_parent").Get<bool>() &&
        meta.joints.count(parent_index) : false;

    phys::shape* s = meta.shapes[node_index];
    if(s && is_bone_collider)
    {
        transformable tmp;
        load_gltf_transform(tmp, parent, node);
        int joint_index = info.joints.at(parent_index);
        meta.skeleton_colliders.at(skeleton_index).joints[joint_index].col = create_gltf_collider(engine_data, s, &tmp, true);
    }

    // Load child nodes
    for(int child_index: node.children)
        load_gltf_skeleton_node(gltf_model, node_index, child_index, next_parent, meta, skeleton_index);
}

void load_gltf_node(
    device& dev,
    scene& entities,
    tinygltf::Model& gltf_model,
    int node_index,
    int parent_index,
    transformable* parent,
    node_meta_info& meta,
    std::vector<std::unique_ptr<texture>>& textures,
    std::unordered_map<std::string, std::unique_ptr<texture>>& light_probe_data,
    std::vector<entity>& added_entities,
    std::unordered_map<int, entity>& node_to_entity,
    const std::string& subfile_prefix,
    filesystem* fs
){
    tinygltf::Node& node = gltf_model.nodes[node_index];
    entity id = entities.add(
        gltf_node{node.name, &gltf_model},
        transformable()
    );
    added_entities.push_back(id);
    node_to_entity[node_index] = id;

    tinygltf::Value* engine_data = node.extensions.count("RB_engine_data") ?
        &node.extensions["RB_engine_data"] : nullptr;

    const tinygltf::Value* collision_data =
        engine_data && engine_data->Has("collision") ?
        &engine_data->Get("collision") : nullptr;

    bool visible = engine_data && engine_data->Has("visibility") ?
        engine_data->Get("visibility").Get<bool>() : true;

    bool is_bone_collider = collision_data &&
        collision_data->Has("bone_parent") ?
        collision_data->Get("bone_parent").Get<bool>() &&
        meta.joints.count(parent_index) : false;

    // Set transformation for node
    transformable* tnode = entities.get<transformable>(id);
    load_gltf_transform(*tnode, parent, node);

    auto it = meta.rigid_animations.find(node_index);
    if(it != meta.rigid_animations.end())
        entities.attach(id, rigid_animated(it->second));

    if(node.mesh != -1 && visible)
    {
        entities.attach(
            id,
            model(meta.models.at(gltf_model.meshes[node.mesh].name))
        );
        mesh* m = entities.get<model>(id)->m;

        auto it = meta.morph_target_animations.find(node_index);
        if(it != meta.morph_target_animations.end())
        {
            m->set_morph_target_animation_pool(it->second);
        }

        if(node.skin != -1)
        {
            auto it = meta.skeletons.find(node.skin);
            if(it != meta.skeletons.end())
            {
                entities.attach(id, skeleton(it->second));
                m->set_skeleton(entities.get<skeleton>(id));
            }

            // The spec specifies that nodes with skinned meshes must be root.
            tnode->set_parent(nullptr);
            tnode->set_transform(mat4(1));
            entities.remove<rigid_animated>(id);

            auto cit = meta.skeleton_colliders.find(node.skin);
            if(cit != meta.skeleton_colliders.end() && !cit->second.is_empty())
                entities.attach(id, skeleton_collider(cit->second));
        }
    }

    if(node.camera != -1)
    {
        camera cam;
        tinygltf::Camera& c = gltf_model.cameras[node.camera];

        if(c.type == "perspective")
        {
            cam.perspective(
                glm::degrees(c.perspective.yfov), c.perspective.aspectRatio,
                c.perspective.znear, c.perspective.zfar
            );
        }
        else if(c.type == "orthographic")
            cam.ortho(
                -0.5*c.orthographic.xmag, 0.5*c.orthographic.xmag,
                -0.5*c.orthographic.ymag, 0.5*c.orthographic.ymag,
                c.orthographic.znear, c.orthographic.zfar
            );

        entities.attach(id, std::move(cam));
    }

    // Add light, if present.
    if(node.extensions.count("KHR_lights_punctual"))
    {
        tinygltf::Light& l = gltf_model.lights[
            node.extensions["KHR_lights_punctual"].Get("light").Get<int>()
        ];
        float radius = 0.0f;
        if(engine_data && engine_data->Has("light"))
        {
            const tinygltf::Value* light_data = &engine_data->Get("light");
            if(light_data->Has("radius"))
                radius = light_data->Get("radius").GetNumberAsDouble();
        }

        // Apparently Blender's gltf exporter is broken in terms of light
        // intensity as of writing this, so the multipliers here are just magic
        // numbers. Fix this when this issue is solved:
        // https://github.com/KhronosGroup/glTF-Blender-IO/issues/564
        vec3 color(vector_to_vec4(l.color) * (float)l.intensity);
        if(l.type == "directional")
        {
            entities.attach(id, directional_light{color, degrees(radius)});
        }
        else if(l.type == "point")
        {
            entities.attach(id, point_light{color*float(0.25/M_PI), radius});
        }
        else if(l.type == "spot")
        {
            spotlight sl{
                {color * float(0.25/M_PI), radius},
                glm::degrees((float)l.spot.outerConeAngle)
            };
            sl.set_inner_angle(glm::degrees(l.spot.innerConeAngle), 4/255.0f);
            entities.attach(id, std::move(sl));
        }
    }

    // Add light probe
    if(engine_data && engine_data->Has("light_probe"))
    {
        tinygltf::Value light_probe = engine_data->Get("light_probe");
        std::string type = light_probe.Get("type").Get<std::string>();
        if(type == "CUBEMAP") // Reflection cubemap
        {
            environment_map e;

            std::string cubemap_path = subfile_prefix + node.name + ".ktx";
            e.cubemap = light_probe_data[cubemap_path].get();
            std::string parallax = light_probe.Get("parallax_type").Get<std::string>();
            if(parallax == "BOX") e.parallax = environment_map::BOX;
            if(parallax == "ELIPSOID") e.parallax = environment_map::SPHERE;

            tnode->scale(vec3(light_probe.Get("parallax_radius").GetNumberAsDouble()));
            e.guard_radius = light_probe.Get("cutoff_guard_radius").GetNumberAsDouble();
            e.clip_range.x = light_probe.Get("clip_near").GetNumberAsDouble();
            e.clip_range.y = light_probe.Get("clip_far").GetNumberAsDouble();

            entities.attach(id, std::move(e));
        }
        else if(type == "GRID") // Irradiance volume
        {
            uvec3 res;
            res.x = light_probe.Get("resolution_x").GetNumberAsDouble();
            res.y = light_probe.Get("resolution_y").GetNumberAsDouble();
            res.z = light_probe.Get("resolution_z").GetNumberAsDouble();
            /*
            sh_grid g(res);

            std::string path = subfile_prefix + node.name + ".ktx";
            texture* cb = light_probe_data[path].get();
            try
            {
                // If set_coef() throws, it's because the resolution doesn't
                // match or the format is not good - just don't use the
                // texture if it isn't good.
                g.set_coef(cb, cb->get_metadata());
                textures.emplace_back(cb);
            }
            catch(std::runtime_error&)
            {
            }
            g.set_radius(light_probe.Get("radius").GetNumberAsDouble());
            vec2 clip_range(0, std::numeric_limits<float>::infinity());
            clip_range.x = light_probe.Get("clip_near").GetNumberAsDouble();
            clip_range.y = light_probe.Get("clip_far").GetNumberAsDouble();
            g.set_clip_range(clip_range);

            entities.attach(id, std::move(g));
            */
        }
    }

    // Try to find collision shape if possible. This must be done after
    // transformation. We also want to skip bone colliders here, because they're
    // already handled earlier.
    phys::shape* s = meta.shapes[node_index];
    if(s && !is_bone_collider)
        entities.attach(id, create_gltf_collider(engine_data, s, tnode, false));

    // Load child nodes
    for(int child_index: node.children)
        load_gltf_node(
            dev,
            entities,
            gltf_model,
            child_index,
            node_index,
            tnode,
            meta,
            textures,
            light_probe_data,
            added_entities,
            node_to_entity,
            subfile_prefix,
            fs
        );
}

void load_gltf_node_names(
    tinygltf::Model& gltf_model,
    int node_index,
    std::unordered_map<std::string, int>& name_to_node
){
    tinygltf::Node& node = gltf_model.nodes[node_index];
    name_to_node[node.name] = node_index;
    for(int child_index: node.children)
        load_gltf_node_names(gltf_model, child_index, name_to_node);
}

void dump_gltf_node(
    tinygltf::Model& gltf_model,
    tinygltf::Scene& scene,
    int node_index,
    int depth,
    node_meta_info& meta
){
    tinygltf::Node& node = gltf_model.nodes[node_index];
    std::vector<std::string> tags;

    tinygltf::Value* engine_data = node.extensions.count("RB_engine_data") ?
        &node.extensions["RB_engine_data"] : nullptr;

    auto it = meta.rigid_animations.find(node_index);
    if(it != meta.rigid_animations.end())
        tags.push_back("animated");

    if(meta.joints.count(node_index))
        tags.push_back("joint");

    if(node.mesh != -1)
    {
        tags.push_back("mesh");
        auto it = meta.morph_target_animations.find(node_index);
        if(it != meta.morph_target_animations.end())
            tags.push_back("morph");

        if(node.skin != -1)
            tags.push_back("skin");
    }

    if(node.camera != -1)
        tags.push_back("camera");

    // Add light, if present.
    if(node.extensions.count("KHR_lights_punctual"))
        tags.push_back("light");

    // Add light probe
    if(engine_data && engine_data->Has("light_probe"))
    {
        tinygltf::Value light_probe = engine_data->Get("light_probe");
        std::string type = light_probe.Get("type").Get<std::string>();
        if(type == "CUBEMAP") // Reflection cubemap
            tags.push_back("cubemap");
        else if(type == "GRID") // Irradiance volume
            tags.push_back("irrvolume");
    }

    if(tags.empty())
        tags.push_back("empty");

    std::cout << std::string(depth, ' ') << node.name << " (";
    for(size_t i = 0; i < tags.size(); ++i)
    {
        std::cout << tags[i];
        if(i+1 != tags.size())
            std::cout << ", ";
    }
    std::cout << ")" << std::endl;

    // Load child nodes
    for(int child_index: node.children)
        dump_gltf_node(
            gltf_model,
            scene,
            child_index,
            depth+1,
            meta
        );
}

}

namespace rb
{

entity search_index<extra::gltf_node>::find(const std::string_view& name) const
{
    auto it = index.find(name);
    if(it == index.end())
        return INVALID_ENTITY;
    return it->second;
}

void search_index<extra::gltf_node>::add_entity(entity id, const extra::gltf_node& data)
{
    index.emplace(data.name, id);
}

void search_index<extra::gltf_node>::remove_entity(entity id, const extra::gltf_node& data)
{
    index.erase(data.name);
}

void search_index<extra::gltf_node>::update(scene& e){}

}

namespace rb::extra
{

struct shape_assets
{
    std::unique_ptr<phys::shape> convex_hull;
    std::unique_ptr<phys::shape> bvh;
};

struct gltf_data_internal
{
    tinygltf::Model gltf_model;
    node_meta_info meta;
    std::string subfile_prefix;
    filesystem* fs;
    std::unordered_map<std::string, int> name_to_node;
    std::unordered_map<std::string, std::unique_ptr<texture>> light_probe_data;
    std::vector<std::unique_ptr<texture>> textures;
    std::vector<std::unique_ptr<sampler>> samplers;
    std::vector<std::unique_ptr<primitive>> primitives;
    std::vector<std::unique_ptr<mesh>> meshes;
    std::vector<std::unique_ptr<rigid_animation_pool>> rigid_animation_pools;
    std::vector<std::unique_ptr<mesh::animation_pool>> morph_target_animation_pools;
    std::vector<std::unique_ptr<phys::shape>> shapes;
    // Only heavy but reusable shape assets are stored like this, others are
    // created per-use.
    std::unordered_map<int /* mesh id */, shape_assets> shared_shapes;
};

gltf_data::gltf_data(device& dev, phys::context* pctx, const file& f, const options& opt)
: opt(opt), dev(&dev)
{
    source_path = f.get_name();
    data.reset(new gltf_data_internal());
    loading_ticket = dev.ctx->get_thread_pool().add_task(
        [dev = &dev, pctx = pctx, f, data = data.get(), opt](){
            load_resources(*dev, pctx, f, data, opt);
        }
    );
}

gltf_data::gltf_data(gltf_data&& other) noexcept
{
    operator=(std::move(other));
}

gltf_data::~gltf_data()
{
    if(data)
        loading_ticket.wait();
}

gltf_data& gltf_data::operator=(gltf_data&& other) noexcept
{
    opt = other.opt;
    dev = other.dev;
    data = std::move(other.data);
    source_path = std::move(other.source_path);
    loading_ticket = std::move(other.loading_ticket);
    other.data.reset();
    return *this;
}

gltf_data gltf_data::load_resource(
    const file& f, device& dev, phys::context* pctx, const options& opt
){
    return gltf_data(dev, pctx, f, opt);
}

bool gltf_data::is_loaded() const
{
    if(!loading_ticket.finished())
        return false;
    for(auto& tex: data->textures)
        if(!tex->is_loaded())
            return false;
    for(auto& prim: data->primitives)
        if(!prim->is_loaded())
            return false;
    return true;
}

void gltf_data::wait() const
{
    loading_ticket.wait();
    for(auto& tex: data->textures)
        tex->wait();
    for(auto& prim: data->primitives)
        prim->wait();
}

void gltf_data::dump_objects() const
{
    loading_ticket.wait();
    for(tinygltf::Scene& scene: data->gltf_model.scenes)
    {
        for(int node_index: scene.nodes)
            dump_gltf_node(
                data->gltf_model,
                scene,
                node_index,
                0,
                data->meta
            );
    }
}

std::vector<entity> gltf_data::add(scene& e, const std::string& root_name)
{
    loading_ticket.wait();
    std::vector<entity> added_entities;
    std::unordered_map<int, entity> node_to_entity;
    if(root_name != "")
    {
        auto it = data->name_to_node.find(root_name);
        if(it != data->name_to_node.end())
        {
            load_gltf_node(
                *dev,
                e,
                data->gltf_model,
                it->second,
                -1,
                nullptr,
                data->meta,
                data->textures,
                data->light_probe_data,
                added_entities,
                node_to_entity,
                data->subfile_prefix,
                data->fs
            );
        }
    }
    else
    {
        for(tinygltf::Scene& scene: data->gltf_model.scenes)
        {
            for(int node_index: scene.nodes)
                load_gltf_node(
                    *dev,
                    e,
                    data->gltf_model,
                    node_index,
                    -1,
                    nullptr,
                    data->meta,
                    data->textures,
                    data->light_probe_data,
                    added_entities,
                    node_to_entity,
                    data->subfile_prefix,
                    data->fs
                );
        }
    }

    // Add constraints
    for(size_t i = 0; i < data->gltf_model.nodes.size(); ++i)
    {
        // This function just skips non-constraints, so it's called for all
        // nodes regardless of whether they have a constraint specifier or not.
        load_gltf_constraint(data->gltf_model, i, node_to_entity, e);
    }

    for(entity id: added_entities)
    {
        model* m = e.get<model>(id);

        // Disable decals for animated meshes by default.
        if(m && m->m && m->m->is_animated())
            e.attach(id, disable_decals{});

        // Make everything rendered by default.
        e.attach(id, rb::gfx::rendered{});

        skeleton* skel = e.get<skeleton>(id);
        collider* col = e.get<collider>(id);
        skeleton_collider* skel_col = e.get<skeleton_collider>(id);
        transformable* t = e.get<transformable>(id);
        if(opt.aggressive_static)
        {
            rigid_animated* anim = e.get<rigid_animated>(id);
            // For aggressive static, the node has to prove that it isn't
            // static.
            bool non_static = anim || skel ||
                (m && m->m && m->m->is_animated()) ||
                (col && (col->get_category_flags() & collider::STATIC) == 0);
            t->set_static(!non_static);
        }
        else if(col && (col->get_category_flags() & collider::STATIC) != 0)
        {
            // For non-aggressive static, the node has to prove that it
            // definitely is static.
            t->set_static(true);
        }

        if(skel_col)
        {
            skel_col->init_joint_collider_scalings();
            if(skel)
                skel_col->auto_joint_constraints(skel);
        }
    }

    for(entity id: added_entities)
    {
        // Fix incorrectly static trees
        transformable* t = e.get<transformable>(id);
        if(!t->ancestors_are_static())
            t->set_static(false);
    }

    return added_entities;
}

void gltf_data::remove(scene& e)
{
    e([&](entity id, gltf_node& node){
        if(node.source == &data->gltf_model)
            e.remove(id);
    });
}

void gltf_data::foreach(scene& e, const std::function<void(entity id)>& f)
{
    e.foreach([&](entity id, gltf_node& node){
        if(node.source == &data->gltf_model)
            f(id);
    });
}

model gltf_data::get_model(const std::string& name) const
{
    loading_ticket.wait();
    auto it = data->meta.models.find(name);
    if(it == data->meta.models.end())
        return model{};
    return it->second;
}

device& gltf_data::get_device() const
{
    return *dev;
}

void gltf_data::load_resources(
    device& dev, phys::context* pctx, const file& f, gltf_data_internal* data,
    const options& opt
){
    std::string prefix = fs::path(f.get_name()).parent_path().generic_string();

    std::string err, warn;
    data->subfile_prefix =
        fs::path(f.get_name()).replace_extension(".").generic_string();
    tinygltf::Model& gltf_model = data->gltf_model;
    tinygltf::TinyGLTF loader;
    filesystem* fs = f.get_source_filesystem();
    data->fs = fs;

    // Use our fancy filesystem with tinygltf
    loader.SetFsCallbacks({
        file_exists_adapter,
        tinygltf::ExpandFilePath,
        read_file_adapter,
        nullptr,
        fs
    });

    // TinyGLTF uses stb_image too, and expects this value.
    stbi_set_flip_vertically_on_load(true);

    if(!loader.LoadBinaryFromMemory(
        &gltf_model, &err, &warn, f.get_data(), f.get_size()
    )) throw std::runtime_error(err);

    for(tinygltf::Image& image: gltf_model.images)
    {
        if(image.bufferView != -1)
        {// Embedded image
            VkFormat format;

            switch(image.component)
            {
            case 1:
                format = image.bits == 8 ? VK_FORMAT_R8_UNORM : VK_FORMAT_R16_UNORM;
                break;
            case 2:
                format = image.bits == 8 ? VK_FORMAT_R8G8_UNORM : VK_FORMAT_R16G16_UNORM;
                break;
            default:
            case 3:
                format = image.bits == 8 ? VK_FORMAT_R8G8B8_UNORM : VK_FORMAT_R16G16B16_UNORM;
                break;
            case 4:
                format = image.bits == 8 ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R16G16B16A16_UNORM;
                break;
            }

            flip_vector_image(image.image, image.height);
            if(image.component == 3 && image.bits == 8)
            {
                RB_CHECK(
                    image.pixel_type != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE,
                    "16-bit channel interlacing not implemented yet"
                );
                std::vector<uint8_t> new_image(image.image.size()/3*4, 0);
                uint8_t fill = 255;
                interlace(
                    new_image.data(),
                    image.image.data(),
                    &fill,
                    3, 4,
                    image.width*image.height
                );
                image.image = std::move(new_image);
                format = VK_FORMAT_R8G8B8A8_UNORM;
            }

            data->textures.emplace_back(new texture(
                dev,
                {
                    uvec3(image.width, image.height, 1),
                    format,
                    1,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_IMAGE_VIEW_TYPE_2D,
                    true,
                    check_opaque(image)
                },
                image.image.size(),
                image.image.data()
            ));
        }
        else
        {// URI
            data->textures.emplace_back(
                new texture(dev, fs->get(prefix + "/" + image.uri))
            );
        }
    }

    for(tinygltf::Sampler& gltf_sampler: gltf_model.samplers)
    {
        VkFilter min = VK_FILTER_LINEAR;
        VkFilter mag = VK_FILTER_LINEAR;
        VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        VkSamplerAddressMode extension = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        bool use_mipmaps = true;

        switch(gltf_sampler.minFilter)
        {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
            min = VK_FILTER_NEAREST;
            use_mipmaps = false;
            break;
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
            min = VK_FILTER_NEAREST;
            use_mipmaps = true;
            mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
            min = VK_FILTER_NEAREST;
            use_mipmaps = true;
            mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
            min = VK_FILTER_LINEAR;
            use_mipmaps = false;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
            min = VK_FILTER_LINEAR;
            use_mipmaps = true;
            mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            min = VK_FILTER_LINEAR;
            use_mipmaps = true;
            mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        default:
            break;
        }

        switch(gltf_sampler.magFilter)
        {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
            mag = VK_FILTER_NEAREST;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
            mag = VK_FILTER_LINEAR;
            break;
        default:
            break;
        }

        switch(gltf_sampler.wrapS)
        {
        case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
            extension = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            break;
        case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
            extension = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            break;
        case TINYGLTF_TEXTURE_WRAP_REPEAT:
            extension = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            break;
        default:
            break;
        }
        data->samplers.emplace_back(new sampler(
            dev,
            min,
            mag,
            mipmap_mode,
            extension,
            16,
            use_mipmaps ? 100.0f : 0.0f
        ));
    }
    data->samplers.emplace_back(new sampler(dev));

    bool has_lightmap_uvs = false;
    node_meta_info& meta = data->meta;
    for(tinygltf::Mesh& mesh: gltf_model.meshes)
    {
        model mod;
        rb::gfx::mesh& me = *data->meshes.emplace_back(new rb::gfx::mesh(dev));
        mod.m = &me;

        size_t morph_target_count = 0;
        for(tinygltf::Primitive& p: mesh.primitives)
        {
            aabb bounding_box;
            bool has_bounding_box = false;

            primitive::vertex_data vd = load_vertex_data(gltf_model, p.attributes, &bounding_box, &has_bounding_box);

            bool current_has_lightmap_uvs = false;
            if(vd.lightmap_uv.size() != 0)
                has_lightmap_uvs = current_has_lightmap_uvs = true;

            std::vector<const primitive*> morph_targets;
            for(auto& attributes: p.targets)
            {
                primitive::vertex_data vd = load_vertex_data(gltf_model, attributes);
                morph_targets.push_back(data->primitives.emplace_back(new primitive(
                    dev, std::move(vd)
                )).get());
            }
            morph_target_count = max(morph_target_count, p.targets.size());

            vd.index = read_accessor<uint32_t>(gltf_model, p.indices);
            vd.ensure_attributes(
                primitive::POSITION|
                primitive::NORMAL|
                primitive::TANGENT|
                primitive::TEXTURE_UV|
                primitive::LIGHTMAP_UV
            );

            material mat;
            if(p.material >= 0)
            {
                mat = create_material(
                    gltf_model.materials[p.material], gltf_model,
                    data->samplers, data->textures
                );
            }
            bool is_animated = vd.joints.size() != 0 || morph_targets.size() != 0;

            data->primitives.emplace_back(new primitive(
                dev,
                std::move(vd),
                !mat.potentially_transparent()
            ));

            if(!has_bounding_box)
            {
                bounding_box = data->primitives.back()->calculate_bounding_box();
                has_bounding_box = true;
            }

            // Expand bounding boxes for animated things, they cannot be
            // contained :(
            if(is_animated)
            {
                vec3 center = (bounding_box.max + bounding_box.min) * 0.5f;
                vec3 radius = bounding_box.max - center;
                bounding_box.min = center - 1.5f * radius;
                bounding_box.max = center + 1.5f * radius;
            }
            data->primitives.back()->set_bounding_box(bounding_box);

            me.add_vertex_group(
                data->primitives.back().get(),
                std::move(morph_targets)
            );
            mod.materials.push_back(mat);
        }
        std::vector<float> weights;
        weights.insert(weights.end(), mesh.weights.begin(), mesh.weights.end());

        if(weights.size() < morph_target_count)
            weights.resize(morph_target_count, 0);

        me.set_morph_target_weights(weights);

        meta.models[mesh.name] = mod;
    }

    // Add collision shapes
    for(size_t i = 0; i < gltf_model.nodes.size(); ++i)
    {
        if(!pctx) continue;

        tinygltf::Node& node = gltf_model.nodes[i];
        if(!node.extensions.count("RB_engine_data"))
            continue;

        tinygltf::Value& engine_data = node.extensions["RB_engine_data"];
        if(!engine_data.Has("collision"))
            continue;

        const tinygltf::Value& collision_data = engine_data.Get("collision");
        phys::shape* shape = nullptr;
        int mesh_id = collision_data.Get("mesh").GetNumberAsInt();
        tinygltf::Mesh& mesh = gltf_model.meshes[mesh_id];
        std::string shape_type = collision_data.Get("shape").Get<std::string>();
        phys::shape::params params = {
            (float)collision_data.Get("margin").GetNumberAsDouble()
        };

        aabb bounding_box;
        const model* mod = &meta.models.at(mesh.name);
        if(!mod->m || !mod->m->get_bounding_box(bounding_box))
            RB_PANIC("Cannot create collision shape; missing bounding box!");

        if(shape_type == "BOX")
            shape = new phys::shape(*pctx, phys::shape::box(bounding_box), params);
        else if(shape_type == "SPHERE")
            shape = new phys::shape(*pctx, phys::shape::sphere(bounding_box), params);
        else if(shape_type == "CAPSULE")
            shape = new phys::shape(*pctx, phys::shape::capsule(bounding_box), params);
        else if(shape_type == "CYLINDER")
            shape = new phys::shape(*pctx, phys::shape::cylinder(bounding_box), params);
        else if(shape_type == "CONE")
            shape = new phys::shape(*pctx, phys::shape::cone(bounding_box), params);
        else if(shape_type == "CONVEX_HULL")
        {
            shape_assets& sa = data->shared_shapes[mesh_id];
            if(!sa.convex_hull)
                sa.convex_hull.reset(create_convex_hull_shape(pctx, gltf_model, mesh, params));
            shape = new phys::shape(*sa.convex_hull, params);
        }
        else if(shape_type == "MESH")
        {
            if(
                collision_data.Get("type").Get<std::string>() == "PASSIVE" &&
                collision_data.Get("kinematic").Get<bool>() == false
            ){
                shape_assets& sa = data->shared_shapes[mesh_id];
                if(!sa.bvh)
                    sa.bvh.reset(create_mesh_shape(pctx, gltf_model, mesh, params, true));
                shape = new phys::shape(*sa.bvh, params);
            }
            else
            {
                RB_DBG(
                    "Creating dynamic triangle mesh collision shape for ",
                    mesh.name, " through ", node.name, ", you probably don't "
                    "want this because it's very, very slow. Mark the object "
                    "as passive and non-animated in Blender's rigid body "
                    "properties to make it a BVH instead, or use a convex hull "
                    "shape if it needs to be animated."
                );
                shape = create_mesh_shape(pctx, gltf_model, mesh, params, false);
            }
        }
        else RB_PANIC(
            "Cannot create collision shape; unsupported shape type ", shape_type
        );

        data->shapes.emplace_back(shape);
        if(shape)
            meta.shapes[i] = shape;
    }

    // Add animations
    for(tinygltf::Animation& anim: gltf_model.animations)
    {
        for(tinygltf::AnimationChannel& chan: anim.channels)
        {
            rigid_animation* ran = nullptr;
            variable_animation<std::vector<float>>* mtan = nullptr;

            if(chan.target_path == "weights")
            { // Morph target animation
                auto it = meta.morph_target_animations.find(chan.target_node);
                if(it == meta.morph_target_animations.end())
                    it = meta.morph_target_animations.emplace(
                        chan.target_node,
                        data->morph_target_animation_pools.emplace_back(
                            new mesh::animation_pool()
                        ).get()
                    ).first;
                mtan = &(*it->second)[anim.name];
            }
            else
            { // Rigid animation
                auto it = meta.rigid_animations.find(chan.target_node);
                if(it == meta.rigid_animations.end())
                    it = meta.rigid_animations.emplace(
                        chan.target_node,
                        data->rigid_animation_pools.emplace_back(
                            new rigid_animation_pool()
                        ).get()
                    ).first;
                ran = &(*it->second)[anim.name];
            }
            tinygltf::AnimationSampler& sampler = anim.samplers[chan.sampler];

            interpolation interp = LINEAR;
            if(sampler.interpolation == "LINEAR") interp = LINEAR;
            else if(sampler.interpolation == "STEP") interp = STEP;
            else if(sampler.interpolation == "CUBICSPLINE")
                interp = CUBICSPLINE;

            if(chan.target_path == "translation")
                ran->set_position(
                    interp,
                    read_animation_accessors<vec3>(
                        gltf_model, sampler.input, sampler.output,
                        interp == CUBICSPLINE
                    )
                );
            else if(chan.target_path == "rotation")
                ran->set_orientation(
                    interp,
                    read_animation_accessors<quat>(
                        gltf_model, sampler.input, sampler.output,
                        interp == CUBICSPLINE
                    )
                );
            else if(chan.target_path == "scale")
                ran->set_scaling(
                    interp,
                    read_animation_accessors<vec3>(
                        gltf_model, sampler.input, sampler.output,
                        interp == CUBICSPLINE
                    )
                );
            else if(chan.target_path == "weights")
                *mtan = variable_animation(
                    interp,
                    read_animation_accessors_vector<float>(
                        gltf_model, sampler.input, sampler.output,
                        interp == CUBICSPLINE
                    )
                );
            // Unknown target type (probably weights for morph targets)
            else continue;
        }
    }

    // Add skins and mark joint nodes
    for(tinygltf::Skin& skin: gltf_model.skins)
    {
        std::vector<mat4> inverse_bind_matrices = read_accessor<mat4>(
            gltf_model, skin.inverseBindMatrices
        );

        if(inverse_bind_matrices.size() != skin.joints.size())
        {
            RB_LOG(skin.name, ": Inverse bind matrix count does not match joint count; chaos may ensue.");
            inverse_bind_matrices.resize(skin.joints.size(), mat4(1.0f));
        }

        size_t skeleton_index = meta.skeleton_info.size();
        auto& info = meta.skeleton_info[skeleton_index];
        info.root_node = -1;
        for(size_t i = 0; i < skin.joints.size(); ++i)
        {
            meta.joints.insert(skin.joints[i]);
            info.true_joints.push_back({inverse_bind_matrices[i]});
            info.joints[skin.joints[i]] = i;
        }

        if(skin.skeleton >= 0)
        {
            info.root_node = skin.skeleton;
            if(!info.joints.count(skin.skeleton))
            { // Skeleton root is a false joint
                info.joints[skin.skeleton] = info.true_joints.size();
                info.false_joints++;
            }
        }
        else
        {
            RB_LOG(
                skin.name,
                ": skin does not have a skeleton. Moving related objects may "
                "break animation!"
            );
            inverse_bind_matrices.resize(skin.joints.size(), mat4(1.0f));
        }

        // Count false joints & detect root nodes
        for(tinygltf::Scene& scene: data->gltf_model.scenes)
        {
            std::unordered_set<int> skeleton_indices;
            for(int node_index: scene.nodes)
                count_gltf_skeleton_node(
                    data->gltf_model,
                    skin.skeleton >= 0 ? skin.skeleton : node_index,
                    data->meta,
                    skeleton_index
                );
        }

        skeleton& skel = meta.skeletons.emplace(
            skeleton_index,
            skeleton(info.true_joints.size(), info.false_joints)
        ).first->second;

        // We just pre-emptively add a skeleton collider, which may not used.
        // The unused skeleton colliders will simply never be added into the
        // scene, so it doesn't matter.
        skeleton_collider& skel_col = meta.skeleton_colliders.emplace(
            skeleton_index,
            skeleton_collider{}
        ).first->second;
        skel_col.joints = std::vector<joint_collider>(skel.get_true_joint_count());

        // Load skeleton nodes
        for(tinygltf::Scene& scene: data->gltf_model.scenes)
        {
            for(int node_index: scene.nodes)
                load_gltf_skeleton_node(
                    data->gltf_model,
                    -1,
                    skin.skeleton >= 0 ? skin.skeleton : node_index,
                    nullptr,
                    data->meta,
                    skeleton_index
                );
        }

        skel.set_root_parent(nullptr);
        skel.reset();
    }

    for(tinygltf::Scene& scene: data->gltf_model.scenes)
    {
        for(int node_index: scene.nodes)
            load_gltf_node_names(data->gltf_model, node_index, data->name_to_node);
    }

    // All buffers should be loaded up now -- free everything that we don't
    // need anymore!
    gltf_model.accessors.clear();
    gltf_model.animations.clear();
    gltf_model.buffers.clear();
    gltf_model.bufferViews.clear();
    gltf_model.materials.clear();
    gltf_model.textures.clear();
    gltf_model.images.clear();
    gltf_model.skins.clear();
    gltf_model.samplers.clear();
    for(tinygltf::Mesh& m: gltf_model.meshes)
    {
        m.primitives.clear();
        m.weights.clear();
    }
}

}
