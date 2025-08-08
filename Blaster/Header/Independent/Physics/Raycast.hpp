#pragma once

#include <optional>
#include "Independent/Math/Vector.hpp"
#include "Independent/Physics/PhysicsWorld.hpp"
#include "Independent/Physics/PhysicsBody.hpp" 

using namespace Blaster::Independent::Math;

namespace Blaster::Independent::Physics
{
    class Raycast final
    {

    public:

        Raycast(const Raycast&) = delete;
        Raycast(Raycast&&) = delete;
        Raycast& operator=(const Raycast&) = delete;
        Raycast& operator=(Raycast&&) = delete;
        
        struct Hit
        {
            bool success = false;

            btCollisionObject* object{ nullptr };
            PhysicsBody* body = nullptr;

            Vector<float, 3> point{ 0,0,0 };
            Vector<float, 3> normal{ 0,0,0 };

            float distance = 0.f;
        };

        static Hit Fire(const Vector<float, 3>& origin, const Vector<float, 3>& direction, float maxDistance, const btCollisionObject* ignore = nullptr, int filterGroup = btBroadphaseProxy::AllFilter, int filterMask = btBroadphaseProxy::AllFilter)
        {
            Hit result;

            if (maxDistance <= 0.f)
                return result;

            auto* world = PhysicsWorld::GetInstance().GetHandle();

            if (!world)
                return result;

            const btVector3 from(origin.x(), origin.y(), origin.z());
            const btVector3 to = from + btVector3(direction.x(), direction.y(), direction.z()) * maxDistance;

            struct Callback : btCollisionWorld::ClosestRayResultCallback
            {
                const btCollisionObject* ignore;

                Callback(const btVector3& f, const btVector3& t, const btCollisionObject* ign) : btCollisionWorld::ClosestRayResultCallback(f, t), ignore(ign) {}

                bool needsCollision(btBroadphaseProxy* proxy0) const override
                {
                    if (proxy0->m_clientObject == ignore)
                        return false;

                    return btCollisionWorld::ClosestRayResultCallback::needsCollision(proxy0);
                }
            };

            Callback cb(from, to, ignore);

            cb.m_collisionFilterGroup = filterGroup;
            cb.m_collisionFilterMask = filterMask;

            world->rayTest(from, to, cb);

            if (cb.hasHit())
            {
                result.success = true;
                result.object = const_cast<btCollisionObject*>(cb.m_collisionObject);

                const btVector3& p = cb.m_hitPointWorld;
                const btVector3& n = cb.m_hitNormalWorld;

                result.point = { p.x(), p.y(), p.z() };
                result.normal = { n.x(), n.y(), n.z() };
                result.distance = cb.m_closestHitFraction * maxDistance;
            }

            return result;
        }

    private:

        Raycast() = default;

    };
}