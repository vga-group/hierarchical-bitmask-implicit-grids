#include "primitive.hh"
#include "vulkan_helpers.hh"
#include "core/error.hh"
#include <algorithm>
#include <numeric>

namespace rb::gfx
{

std::atomic_uint64_t primitive::id_counter = 0;

void primitive::vertex_data::ensure_attributes(attribute_flag mask)
{
    if(index.size() == 0 && position.size() != 0)
    {
        index.resize(position.size());
        std::iota(index.begin(), index.end(), 0);
    }

    if((mask&NORMAL) && normal.size() != position.size())
    {
        normal.resize(position.size());
        std::fill(normal.begin(), normal.end(), pvec3(0));
        // Generate normals
        for(size_t i = 0; i < index.size(); i += 3)
        {
            pvec3 p0 = position[index[i+0]];
            pvec3 p1 = position[index[i+1]];
            pvec3 p2 = position[index[i+2]];
            pvec3 n = cross(p1-p0, p2-p0);
            float len = length(n);
            if(len > 1e-8) n /= len;
            else n = pvec3(0);
            normal[index[i+0]] += n;
            normal[index[i+1]] += n;
            normal[index[i+2]] += n;
        }
        for(size_t i = 0; i < normal.size(); i+=3)
        {
            pvec3 n = normal[i];
            float len = length(n);
            if(len > 1e-8) n /= len;
            normal[i] = n;
        }
    }

    if((mask&TEXTURE_UV) && texture_uv.size() != position.size())
    {
        texture_uv.resize(position.size());
        std::fill(texture_uv.begin(), texture_uv.end(), pvec2(0));
    }

    if((mask&TANGENT) && tangent.size() != position.size())
    {
        tangent.resize(position.size());
        std::fill(tangent.begin(), tangent.end(), pvec4(0));
        // Generate tangents
        for(size_t i = 0; i < index.size(); i += 3)
        {
            pvec3 p0 = position[index[i+0]];
            pvec3 p1 = position[index[i+1]];
            pvec3 p2 = position[index[i+2]];

            pvec2 uv0 = texture_uv[index[i+0]];
            pvec2 uv1 = texture_uv[index[i+1]];
            pvec2 uv2 = texture_uv[index[i+2]];

            pvec3 d0 = p1 - p0;
            pvec3 d1 = p2 - p0;

            pvec3 n = cross(d0, d1);

            pvec2 s0 = uv1 - uv0;
            pvec2 s1 = uv2 - uv0;

            float r = 1.0f / (s0.x * s1.y - s1.x * s0.y);
            pvec3 sdir = (s1.y * d0 - s0.y * d1) * r;
            pvec3 tdir = (s0.x * d1 - s1.x * d0) * r;

            pvec4 t = pvec4(
                tdir, dot(cross(n, sdir), tdir) < 0.0f ? -1.0f : 1.0f
            );

            tangent[index[i+0]] += t;
            tangent[index[i+1]] += t;
            tangent[index[i+2]] += t;
        }

        for(size_t i = 0; i < tangent.size(); ++i)
        {
            pvec4 th = tangent[i];
            pvec3 t = th;
            float handedness = th.w;
            pvec3 n = normal[i];
            t = t - n * dot(n, t);
            float len = length(t);
            if(len > 1e-8) t /= len;
            else t = create_tangent(n);
            tangent[i] = pvec4(t, handedness < 0 ? -1 : 1);
        }

    }

    if((mask&LIGHTMAP_UV) && lightmap_uv.size() != position.size())
    {
        lightmap_uv.resize(position.size());
        if(texture_uv.size() == position.size())
        {
            std::copy(
                texture_uv.begin(),
                texture_uv.end(),
                lightmap_uv.begin()
            );
        }
        else std::fill(lightmap_uv.begin(), lightmap_uv.end(), pvec2(0));
    }

    if((mask&JOINTS) && joints.size() != position.size())
    {
        joints.resize(position.size());
        std::fill(joints.begin(), joints.end(), pivec4(0));
    }

    if((mask&WEIGHTS) && weights.size() != position.size())
    {
        weights.resize(position.size());
        std::fill(weights.begin(), weights.end(), pvec4(0));
    }

    if((mask&COLOR) && color.size() != position.size())
    {
        color.resize(position.size());
        std::fill(color.begin(), color.end(), pvec4(0,0,0,1));
    }
}

primitive::attribute_flag
primitive::vertex_data::get_available_attributes() const
{
    attribute_flag mask = 0;

    size_t count = get_vertex_count();

    if(position.size() == count) mask |= POSITION;
    if(normal.size() == count) mask |= NORMAL;
    if(tangent.size() == count) mask |= TANGENT;
    if(texture_uv.size() == count) mask |= TEXTURE_UV;
    if(lightmap_uv.size() == count) mask |= LIGHTMAP_UV;
    if(joints.size() == count) mask |= JOINTS;
    if(weights.size() == count) mask |= WEIGHTS;
    if(color.size() == count) mask |= COLOR;

    return mask;
}

size_t primitive::vertex_data::get_vertex_count() const
{
    return std::max({
        position.size(),
        normal.size(),
        tangent.size(),
        texture_uv.size(),
        lightmap_uv.size(),
        joints.size(),
        weights.size(),
        color.size()
    });
}

void* primitive::vertex_data::get_attribute_data(attribute_flag attribute) const
{
    if(attribute == POSITION || attribute == PREV_POSITION)
        return (void*)position.data();
    if(attribute == NORMAL) return (void*)normal.data();
    if(attribute == TANGENT) return (void*)tangent.data();
    if(attribute == TEXTURE_UV) return (void*)texture_uv.data();
    if(attribute == LIGHTMAP_UV) return (void*)lightmap_uv.data();
    if(attribute == JOINTS) return (void*)joints.data();
    if(attribute == WEIGHTS) return (void*)weights.data();
    if(attribute == COLOR) return (void*)color.data();
    return nullptr;
}

primitive::primitive(
    device& dev,
    vertex_data&& data,
    bool opaque
):  async_loadable_resource(dev)
{
    super_impl_data* d = &impl(false);
    d->source = nullptr;
    d->opaque = opaque;
    d->data = std::move(data);
    d->has_bounding_box = false;
    d->unique_id = id_counter++;
    d->alignment =
        dev.physical_device_props.properties.limits.minStorageBufferOffsetAlignment;
    async_load([d](){
        d->available_attributes = d->data.get_available_attributes();

        size_t vertex_buf_size = d->get_attribute_size(ALL_ATTRIBS);
        size_t index_buf_size = d->data.index.size() * sizeof(uint32_t);
        VkBufferUsageFlags extra_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if(d->dev->supports_ray_tracing_pipeline)
        {
            extra_flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT|
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        }

        d->vertex_buffer = create_gpu_buffer(
            *d->dev, vertex_buf_size,
            extra_flags|VK_BUFFER_USAGE_TRANSFER_DST_BIT|
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
        );
        VkBufferDeviceAddressInfo vertex_info = {
            VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            nullptr,
            *d->vertex_buffer
        };
        d->vertex_buffer_address = vkGetBufferDeviceAddress(d->dev->logical_device, &vertex_info);
        if(index_buf_size != 0)
        {
            d->index_buffer = upload_buffer(
                *d->dev, d->loading_events.emplace_back(),
                index_buf_size, d->data.index.data(),
                extra_flags|VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            );
            VkBufferDeviceAddressInfo index_info = {
                VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                nullptr,
                *d->index_buffer
            };
            d->index_buffer_address = vkGetBufferDeviceAddress(d->dev->logical_device, &index_info);
        }

        vkres<VkCommandBuffer> cmd = begin_command_buffer(*d->dev);
        for(attribute_flag i = 1; i < d->available_attributes; i <<= 1)
        {
            if(d->available_attributes&i)
            {
                void* ptr = d->data.get_attribute_data(i);
                size_t bytes = d->get_attribute_size(i);
                size_t offset = d->get_attribute_offset(i);
                vkres<VkBuffer> staging = create_staging_buffer(*d->dev, bytes, ptr);
                VkBufferCopy region = {0, offset, bytes};
                vkCmdCopyBuffer(cmd, staging, d->vertex_buffer, 1, &region);
                cmd.depend({*staging, *d->vertex_buffer});
            }
        }
        d->loading_events.emplace_back(end_command_buffer(*d->dev, cmd));
    });
}

primitive::primitive(const primitive* source):
    async_loadable_resource(source->get_device())
{
    super_impl_data* d = &impl(false);
    d->source = source;
    d->opaque = source->impl(false).opaque;
    d->has_bounding_box = source->get_bounding_box(d->bounding_box);
    d->unique_id = id_counter++;
    d->alignment = source->impl(false).alignment;
    async_load([d](){
        constexpr attribute_flag duplicated_attributes =
            POSITION | NORMAL | TANGENT | PREV_POSITION;

        super_impl_data& sd = d->source->impl();
        d->available_attributes =
            (sd.data.get_available_attributes() & duplicated_attributes) |
            PREV_POSITION;

        size_t vertex_buf_size = sd.get_attribute_size(d->available_attributes);
        VkBufferUsageFlags extra_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if(d->dev->supports_ray_tracing_pipeline)
        {
            extra_flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT|
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        }

        d->vertex_buffer = create_gpu_buffer(
            *d->dev, vertex_buf_size,
            extra_flags|VK_BUFFER_USAGE_TRANSFER_DST_BIT|
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
        );
        VkBufferDeviceAddressInfo vertex_info = {
            VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            nullptr,
            *d->vertex_buffer
        };
        d->vertex_buffer_address = vkGetBufferDeviceAddress(d->dev->logical_device, &vertex_info);
        d->index_buffer_address = 0;

        vkres<VkCommandBuffer> cmd = begin_command_buffer(*d->dev);
        for(attribute_flag i = 1; i < d->available_attributes; i <<= 1)
        {
            if(d->available_attributes&i)
            {
                void* ptr = sd.data.get_attribute_data(i);
                size_t bytes = d->get_attribute_size(i);
                size_t offset = d->get_attribute_offset(i);
                vkres<VkBuffer> staging = create_staging_buffer(*d->dev, bytes, ptr);
                VkBufferCopy region = {0, offset, bytes};
                vkCmdCopyBuffer(cmd, staging, d->vertex_buffer, 1, &region);
                cmd.depend({*staging, *d->vertex_buffer});
            }
        }
        d->loading_events.emplace_back(end_command_buffer(*d->dev, cmd));
    }, *source);
}

VkBuffer primitive::get_vertex_buffer(attribute_flag attribute) const
{
    auto& i = impl();
    if(attribute == PREV_POSITION && !(i.available_attributes & PREV_POSITION))
        attribute = POSITION;

    if((i.available_attributes & attribute) != attribute)
    {
        if(i.source)
            return i.source->get_vertex_buffer(attribute);
        return VK_NULL_HANDLE;
    }
    return i.vertex_buffer;
}

VkDeviceAddress primitive::get_vertex_buffer_address(attribute_flag attribute) const
{
    auto& i = impl();
    if(attribute == PREV_POSITION && !(i.available_attributes & PREV_POSITION))
        attribute = POSITION;

    if((i.available_attributes & attribute) != attribute)
    {
        if(i.source)
            return i.source->get_vertex_buffer_address(attribute);
        return 0;
    }
    return i.vertex_buffer_address;
}

VkBuffer primitive::get_index_buffer() const
{
    auto& i = impl();
    if(!i.index_buffer)
    {
        if(i.source)
            return i.source->get_index_buffer();
        return VK_NULL_HANDLE;
    }
    return i.index_buffer;
}

VkDeviceAddress primitive::get_index_buffer_address() const
{
    auto& i = impl();
    if(!i.index_buffer)
    {
        if(i.source)
            return i.source->get_index_buffer_address();
        return 0;
    }
    return i.index_buffer_address;
}

bool primitive::is_opaque() const
{
    return impl().opaque;
}

aabb primitive::calculate_bounding_box() const
{
    pvec3 min_pos(FLT_MAX);
    pvec3 max_pos(-FLT_MAX);
    const auto& i = impl();
    if(i.source)
    {
        return i.source->calculate_bounding_box();
    }
    else
    {
        for(pvec3 pos: i.data.position)
        {
            min_pos = min(pos, min_pos);
            max_pos = max(pos, max_pos);
        }
        return aabb{min_pos, max_pos};
    }
}

void primitive::set_bounding_box(aabb bounding_box)
{
    auto& i = impl();
    i.bounding_box = bounding_box;
    i.has_bounding_box = true;
}

bool primitive::get_bounding_box(aabb& bounding_box) const
{
    auto& i = impl();
    bounding_box = i.bounding_box;
    return i.has_bounding_box;
}

bool primitive::has_bounding_box() const
{
    return impl().has_bounding_box;
}

size_t primitive::get_index_count() const
{
    auto& i = impl();
    if(!i.index_buffer && i.source)
        return i.source->get_index_count();
    return i.data.index.size();
}

size_t primitive::get_vertex_count() const
{
    auto& i = impl();
    if(i.source)
        return i.source->get_vertex_count();
    return i.data.get_vertex_count();
}

size_t primitive::get_attribute_offset(attribute_flag attribute) const
{
    return impl().get_attribute_offset(attribute);
}

size_t primitive::impl_data::get_attribute_offset(attribute_flag attribute) const
{
    // If we don't have PREV_POSITION, this primitive is not animated and we can
    // just give the current position instead.
    if(attribute == PREV_POSITION && !(available_attributes & PREV_POSITION))
        attribute = POSITION;

    // If we don't have this attribute, it's from the parent instead.
    if((available_attributes & attribute) != attribute && source)
        return source->get_attribute_offset(attribute);

    size_t offset = 0;
    for(attribute_flag i = 1; i < attribute; i <<= 1)
    {
        if(available_attributes & i)
            offset += get_attribute_size(i);
    }
    return offset;
}

size_t primitive::get_attribute_size(attribute_flag attribute) const
{
    return impl().get_attribute_size(attribute);
}

size_t primitive::impl_data::get_attribute_size(attribute_flag attribute) const
{
    // The source will have the same attributes, except PREV_POSITION. But that
    // doesn't matter, since it will give the correct POSITION size for it
    // anyways.
    if(source)
        return source->get_attribute_size(attribute);

#define add_aligned(ns) size += (ns + alignment - 1)/alignment * alignment

    size_t size = 0;
    if(attribute&POSITION)
        add_aligned(sizeof(data.position[0]) * data.position.size());
    if(attribute&NORMAL)
        add_aligned(sizeof(data.normal[0]) * data.normal.size());
    if(attribute&TANGENT)
        add_aligned(sizeof(data.tangent[0]) * data.tangent.size());
    if(attribute&TEXTURE_UV)
        add_aligned(sizeof(data.texture_uv[0]) * data.texture_uv.size());
    if(attribute&LIGHTMAP_UV)
        add_aligned(sizeof(data.lightmap_uv[0]) * data.lightmap_uv.size());
    if(attribute&JOINTS)
        add_aligned(sizeof(data.joints[0]) * data.joints.size());
    if(attribute&WEIGHTS)
        add_aligned(sizeof(data.weights[0]) * data.weights.size());
    if(attribute&COLOR)
        add_aligned(sizeof(data.color[0]) * data.color.size());
    if(attribute&PREV_POSITION)
        add_aligned(sizeof(data.position[0]) * data.position.size());

    return size;
}

bool primitive::has_attribute(attribute_flag attribute) const
{
    return impl().has_attribute(attribute);
}

bool primitive::impl_data::has_attribute(attribute_flag attribute) const
{
    // Prev position can be read from current position as well, if the model
    // isn't animated. So it's fine if that is missing, we still kind of have
    // the attribute.
    if(attribute & PREV_POSITION)
    {
        attribute ^= PREV_POSITION;
        attribute |= POSITION;
    }
    return (get_all_available_attributes() & attribute) == attribute;
}

primitive::attribute_flag primitive::get_available_attributes() const
{
    return impl().get_all_available_attributes();
}

primitive::attribute_flag
primitive::impl_data::get_all_available_attributes() const
{
    attribute_flag all = available_attributes;
    if(source)
        all |= source->impl().get_all_available_attributes();
    return all;
}

const primitive::vertex_data& primitive::get_vertex_data() const
{
    return impl().data;
}

void primitive::draw(
    VkCommandBuffer buf,
    attribute_flag mask,
    uint32_t num_instances
) const
{
    RB_CHECK(!has_attribute(mask), "Some requested attributes do not exist.");
    auto& im = impl();

    size_t attribute_count = 0;
    VkBuffer buffers[8];
    VkDeviceSize offsets[8];
    for(attribute_flag i = 1; i <= mask; i <<= 1)
    {
        if(mask&i)
        {
            offsets[attribute_count] = get_attribute_offset(i);
            buffers[attribute_count] = get_vertex_buffer(i);
            attribute_count++;
        }
    }
    vkCmdBindVertexBuffers(buf, 0, attribute_count, buffers, offsets);
    VkBuffer index_buffer = get_index_buffer();
    if(index_buffer)
    {
        vkCmdBindIndexBuffer(buf, index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(buf, get_index_count(), num_instances, 0, 0, 0);
        dev->gc.depend(index_buffer, buf);
    }
    else
    {
        RB_PANIC("Unindexed geometry! This won't work with ray tracing!");
        vkCmdDraw(buf, get_vertex_count(), num_instances, 0, 0);
    }

    dev->gc.depend(*im.vertex_buffer, buf);
    if(im.source)
        dev->gc.depend(*im.source->impl().vertex_buffer, buf);
}

std::vector<VkVertexInputBindingDescription>
primitive::get_bindings(attribute_flag mask)
{
    std::vector<VkVertexInputBindingDescription> res;
    uint32_t index = 0;
    if(mask&POSITION)
        res.push_back({index++, sizeof(vertex_data().position[0]), VK_VERTEX_INPUT_RATE_VERTEX});
    if(mask&NORMAL)
        res.push_back({index++, sizeof(vertex_data().normal[0]), VK_VERTEX_INPUT_RATE_VERTEX});
    if(mask&TANGENT)
        res.push_back({index++, sizeof(vertex_data().tangent[0]), VK_VERTEX_INPUT_RATE_VERTEX});
    if(mask&TEXTURE_UV)
        res.push_back({index++, sizeof(vertex_data().texture_uv[0]), VK_VERTEX_INPUT_RATE_VERTEX});
    if(mask&LIGHTMAP_UV)
        res.push_back({index++, sizeof(vertex_data().lightmap_uv[0]), VK_VERTEX_INPUT_RATE_VERTEX});
    if(mask&JOINTS)
        res.push_back({index++, sizeof(vertex_data().joints[0]), VK_VERTEX_INPUT_RATE_VERTEX});
    if(mask&WEIGHTS)
        res.push_back({index++, sizeof(vertex_data().weights[0]), VK_VERTEX_INPUT_RATE_VERTEX});
    if(mask&COLOR)
        res.push_back({index++, sizeof(vertex_data().color[0]), VK_VERTEX_INPUT_RATE_VERTEX});
    if(mask&PREV_POSITION)
        res.push_back({index++, sizeof(vertex_data().position[0]), VK_VERTEX_INPUT_RATE_VERTEX});
    return res;
}

std::vector<VkVertexInputAttributeDescription>
primitive::get_attributes(attribute_flag mask)
{
    std::vector<VkVertexInputAttributeDescription> res;
    uint32_t index = 0;
    if(mask&POSITION)
    {
        res.push_back({index, index, VK_FORMAT_R32G32B32_SFLOAT, 0});
        index++;
    }
    if(mask&NORMAL)
    {
        res.push_back({index, index, VK_FORMAT_R32G32B32_SFLOAT, 0});
        index++;
    }
    if(mask&TANGENT)
    {
        res.push_back({index, index, VK_FORMAT_R32G32B32A32_SFLOAT, 0});
        index++;
    }
    if(mask&TEXTURE_UV)
    {
        res.push_back({index, index, VK_FORMAT_R32G32_SFLOAT, 0});
        index++;
    }
    if(mask&LIGHTMAP_UV)
    {
        res.push_back({index, index, VK_FORMAT_R32G32_SFLOAT, 0});
        index++;
    }
    if(mask&JOINTS)
    {
        res.push_back({index, index, VK_FORMAT_R32G32B32A32_UINT, 0});
        index++;
    }
    if(mask&WEIGHTS)
    {
        res.push_back({index, index, VK_FORMAT_R32G32B32A32_SFLOAT, 0});
        index++;
    }
    if(mask&COLOR)
    {
        res.push_back({index, index, VK_FORMAT_R32G32B32_SFLOAT, 0});
        index++;
    }
    if(mask&PREV_POSITION)
    {
        res.push_back({index, index, VK_FORMAT_R32G32B32_SFLOAT, 0});
        index++;
    }
    return res;
}

uint64_t primitive::get_unique_id() const
{
    return impl().unique_id;
}

}
