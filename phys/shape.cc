#include "shape.hh"
#include "internal.cc"
#include "BulletCollision/Gimpact/btGImpactShape.h"

namespace
{
using namespace rb;

struct tri_vertex_data
{
    tri_vertex_data(std::vector<vec3>&& triangles)
    {
        pos = std::move(triangles);
        for(uint32_t i = 0; i < triangles.size(); ++i)
            index.push_back(i);

        init();
    }

    tri_vertex_data(
        std::vector<uint32_t>&& index,
        std::vector<vec3>&& pos
    ){
        this->index = std::move(index);
        this->pos = std::move(pos);

        init();
    }

    void init()
    {
        tri_mesh.m_numTriangles = index.size()/3;
        tri_mesh.m_triangleIndexBase = (uint8_t*)index.data();
        tri_mesh.m_triangleIndexStride = 3*sizeof(uint32_t);
        tri_mesh.m_numVertices = pos.size();
        tri_mesh.m_vertexBase = (uint8_t*)pos.data();
        tri_mesh.m_vertexStride = sizeof(vec3);
        tri_mesh.m_indexType = PHY_INTEGER;
        tri_mesh.m_vertexType = PHY_FLOAT;
        tri.addIndexedMesh(tri_mesh, tri_mesh.m_indexType);
    }

    btTriangleIndexVertexArray tri;
    btIndexedMesh tri_mesh;
    std::vector<uint32_t> index;
    std::vector<vec3> pos;
};

}

namespace rb::phys
{

shape::box::box(const aabb& bounding_box)
: half_extent((bounding_box.max-bounding_box.min)*0.5f)
{}

shape::sphere::sphere(const aabb& bounding_box)
: radius(vecmax(bounding_box.max-bounding_box.min)*0.5f)
{}

shape::capsule::capsule(const aabb& bounding_box):
    radius(
        std::max(
            bounding_box.max.x-bounding_box.min.x,
            bounding_box.max.z-bounding_box.min.z
        ) * 0.5f
    ),
    height(
        std::max(
            bounding_box.max.y-bounding_box.min.y-
            std::max(
                bounding_box.max.x-bounding_box.min.x,
                bounding_box.max.z-bounding_box.min.z
            ),
            0.0f
        )
    )
{}

shape::cylinder::cylinder(const aabb& bounding_box):
    half_extent((bounding_box.max-bounding_box.min)*0.5f)
{}

shape::cone::cone(const aabb& bounding_box):
    radius(
        std::max(
            bounding_box.max.x-bounding_box.min.x,
            bounding_box.max.z-bounding_box.min.z
        ) * 0.5f
    ),
    height(bounding_box.max.y-bounding_box.min.y)
{}

struct shape::impl_data
{
    std::unique_ptr<btCollisionShape> shape;
    std::unique_ptr<tri_vertex_data> vertex_data;
};

shape::shape(context& ctx, box&& c, const params& p)
{
    data.reset(new impl_data());
    data->shape.reset(new btBoxShape(glm_to_bt(c.half_extent)));
    reset(p);
}

shape::shape(context& ctx, sphere&& c, const params& p)
{
    data.reset(new impl_data());
    data->shape.reset(new btSphereShape(c.radius));
    reset(p);
}

shape::shape(context& ctx, capsule&& c, const params& p)
{
    data.reset(new impl_data());
    data->shape.reset(new btCapsuleShape(c.radius, c.height));
    reset(p);
}

shape::shape(context& ctx, cylinder&& c, const params& p)
{
    data.reset(new impl_data());
    data->shape.reset(new btCylinderShape(glm_to_bt(c.half_extent)));
    reset(p);
}

shape::shape(context& ctx, cone&& c, const params& p)
{
    data.reset(new impl_data());
    data->shape.reset(new btConeShape(c.radius, c.height));
    reset(p);
}

shape::shape(context& ctx, convex_hull&& c, const params& p)
{
    data.reset(new impl_data());

    scaling = p.scaling;
    origin = p.origin;
    orientation = p.orientation;

    loading_ticket = ctx.get_thread_pool().add_task(
        [c = std::move(c), data = data.get(), p = p](){
            btConvexHullShape* shape = new btConvexHullShape(
                (btScalar*)c.points.data(),
                c.points.size(),
                sizeof(c.points[0])
            );
            data->shape.reset(shape);
            shape->optimizeConvexHull();
            shape->recalcLocalAabb();

            set_shape_params(data->shape.get(), p);
        }
    );
}

shape::shape(context& ctx, dynamic_mesh&& c, const params& p)
{
    data.reset(new impl_data());

    scaling = p.scaling;
    origin = p.origin;
    orientation = p.orientation;

    loading_ticket = ctx.get_thread_pool().add_task(
        [c = std::move(c), data = data.get(), p = p]() mutable {
            data->vertex_data.reset(
                new tri_vertex_data(std::move(c.indices), std::move(c.points))
            );
            btGImpactMeshShape* shape = new btGImpactMeshShape(&data->vertex_data->tri);
            shape->updateBound();
            data->shape.reset(shape);

            set_shape_params(data->shape.get(), p);
        }
    );
}

shape::shape(context& ctx, static_mesh&& c, const params& p)
{
    data.reset(new impl_data());

    scaling = p.scaling;
    origin = p.origin;
    orientation = p.orientation;

    loading_ticket = ctx.get_thread_pool().add_task(
        [c = std::move(c), data = data.get(), p = p]() mutable {
            data->vertex_data.reset(
                new tri_vertex_data(std::move(c.indices), std::move(c.points))
            );
            btBvhTriangleMeshShape* shape = new btBvhTriangleMeshShape(&data->vertex_data->tri, true);
            data->shape.reset(shape);

            set_shape_params(data->shape.get(), p);
        }
    );
}

shape::shape(shape& other, const params& p)
{
    data.reset(new impl_data());

    btCollisionShape* other_shape = other.get_bt_shape();

    if(auto* s = dynamic_cast<btBoxShape*>(other_shape))
    {
        data->shape.reset(new btBoxShape(*s));
    }
    else if(auto* s = dynamic_cast<btSphereShape*>(other_shape))
    {
        data->shape.reset(new btSphereShape(*s));
    }
    else if(auto* s = dynamic_cast<btCapsuleShape*>(other_shape))
    {
        data->shape.reset(new btCapsuleShape(*s));
    }
    else if(auto* s = dynamic_cast<btCylinderShape*>(other_shape))
    {
        data->shape.reset(new btCylinderShape(*s));
    }
    else if(auto* s = dynamic_cast<btConeShape*>(other_shape))
    {
        data->shape.reset(new btConeShape(*s));
    }
    else if(auto* s = dynamic_cast<btUniformScalingShape*>(other_shape))
    {
        float scaling = (p.scaling.x + p.scaling.y + p.scaling.z)/3;
        data->shape.reset(new btUniformScalingShape(s->getChildShape(), scaling));
    }
    else if(auto* s = dynamic_cast<btConvexHullShape*>(other_shape))
    {
        float scaling = (p.scaling.x + p.scaling.y + p.scaling.z)/3;
        data->shape.reset(new btUniformScalingShape(s, scaling));
    }
    else if(auto* s = dynamic_cast<btScaledBvhTriangleMeshShape*>(other_shape))
    {
        data->shape.reset(new btScaledBvhTriangleMeshShape(s->getChildShape(), glm_to_bt(p.scaling)));
    }
    else if(auto* s = dynamic_cast<btBvhTriangleMeshShape*>(other_shape))
    {
        data->shape.reset(new btScaledBvhTriangleMeshShape(s, glm_to_bt(p.scaling)));
    }
    else if(auto* s = dynamic_cast<btGImpactMeshShape*>(other_shape))
    {
        auto* shape = new btGImpactMeshShape(&other.data->vertex_data->tri);
        shape->updateBound();
        data->shape.reset(shape);
    }
    else RB_PANIC("I don't know how to instance this shape!");

    reset(p);
}

shape::~shape() { loading_ticket.wait(); }

bool shape::is_loaded() const
{
    return loading_ticket.finished();
}

void shape::wait() const
{
    loading_ticket.wait();
}

btCollisionShape* shape::get_bt_shape() const
{
    loading_ticket.wait();
    return data->shape.get();
}

void shape::reset(const params& p)
{
    btCollisionShape* s = get_bt_shape();
    set_shape_params(s, p);
    scaling = p.scaling;
    origin = p.origin;
    orientation = p.orientation;
}

void shape::apply_scale(vec3 new_scale)
{
    btCollisionShape* s = get_bt_shape();
    reset({
        s->getMargin(),
        new_scale,
        origin / scaling * new_scale,
        orientation
    });
}

vec3 shape::get_scaling() const
{
    return scaling;
}

void shape::get_bt_offset(btTransform& t) const
{
    t = btTransform(glm_to_bt(orientation), glm_to_bt(origin)).inverse();
}

mat4 shape::get_offset() const
{
    mat4 rot = glm::toMat4(orientation);
    return inverse(mat4(rot[0], rot[1], rot[2], vec4(origin, 1)));
}

void shape::set_shape_params(void* shape, const params& p)
{
    btCollisionShape* s = (btCollisionShape*)shape;
    if(auto* c = dynamic_cast<btUniformScalingShape*>(s))
    {
        float scaling = (p.scaling.x+p.scaling.y+p.scaling.z)/3.0f;
        *c = btUniformScalingShape(c->getChildShape(), scaling);
    }
    else
    {
        s->setMargin(p.margin);
        s->setLocalScaling(glm_to_bt(p.scaling));
    }
}

}
