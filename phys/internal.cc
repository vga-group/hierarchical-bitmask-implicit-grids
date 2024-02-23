// Don't include this file from any header, only include it from *.cc files
// within client/phys/
#ifndef RAYBASE_PHYS_INTERNAL_HH
#define RAYBASE_PHYS_INTERNAL_HH
#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btCollisionDispatcherMt.h"
#include "BulletDynamics/Dynamics/btDiscreteDynamicsWorldMt.h"
#include "BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolverMt.h"
#include "core/math.hh"

namespace rb::phys
{

inline btVector3 glm_to_bt(vec3 v)
{
    return btVector3(v.x, v.y, v.z);
}

inline btQuaternion glm_to_bt(quat q)
{
    return btQuaternion(q.x, q.y, q.z, q.w);
}

inline vec3 bt_to_glm(btVector3 v)
{
    return vec3(v.x(), v.y(), v.z());
}

inline quat bt_to_glm(btQuaternion q)
{
    return quat(q.w(), q.x(), q.y(), q.z());
}

inline btTransform glm_to_bt(mat4 m)
{
    vec3 translation;
    vec3 scaling;
    quat orientation;
    decompose_matrix(m, translation, scaling, orientation);
    return btTransform(glm_to_bt(orientation), glm_to_bt(translation));
}

}

#endif

