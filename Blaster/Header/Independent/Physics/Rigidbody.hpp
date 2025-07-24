#pragma once

#include <optional>
#include "Independent/ECS/GameObject.hpp"
#include "Independent/Math/Transform.hpp"
#include "Independent/Physics/Collider.hpp"
#include "Independent/Physics/PhysicsBody.hpp"
#include "Independent/Physics/PhysicsWorld.hpp"
#include "Independent/Physics/PhysicsCommands.hpp"
#include "Independent/Test/PhysicsDebugger.hpp"

#ifdef IS_SERVER
#include "Server/Network/ServerNetwork.hpp"
#else
#include "Client/Network/ClientNetwork.hpp"
#endif

using namespace Blaster::Independent::ECS;
using namespace Blaster::Independent::Math;
using namespace Blaster::Independent::Network;
using namespace Blaster::Independent::Test;

namespace Blaster::Independent::Physics
{
    class Rigidbody final : public PhysicsBody
    {

    public:
        
        enum class Type
        {
            STATIC, KINEMATIC, DYNAMIC
        };

        enum class Axis : uint8_t
        {
            NONE = 0,
            X = 1,
            Y = 2,
            Z = 4
        };

        ~Rigidbody()
        {
            if (!body)
                return;

            UnregisterFromWorld();

            delete body;

            body = nullptr;
        }

        Rigidbody(const Rigidbody&) = delete;
        Rigidbody(Rigidbody&&) = delete;
        Rigidbody& operator=(const Rigidbody&) = delete;
        Rigidbody& operator=(Rigidbody&&) = delete;

        void Initialize() override
        {
#ifndef IS_SERVER
            if (!GetGameObject()->IsLocallyControlled())
                GetGameObject()->SetLocal(true);
            
            GetGameObject()->GetTransform()->SetShouldSynchronize(false);
#endif

            auto colliderOptional = GetGameObject()->GetComponent<Collider>();

            if (!colliderOptional)
            {
                std::cerr << "Rigidbody requires a collider on GameObject '" << GetGameObject()->GetName() << "'\n";

                return;
            }

            auto collider = std::static_pointer_cast<Collider>(*colliderOptional);
            auto transform = GetGameObject()->GetTransform();

            btVector3 inertia(0, 0, 0);

            if (mass > 0.0f && bodyType == Type::DYNAMIC)
                collider->GetShape()->calculateLocalInertia(mass, inertia);

            btTransform startXform;
            
            startXform.setIdentity();

            {
                auto p = transform->GetWorldPosition();
                auto r = transform->GetWorldRotation();

                startXform.setOrigin({ p.x(), p.y(), p.z() });
                startXform.setRotation(btQuaternion(btVector3(1, 0, 0), r.x() * SIMD_PI / 180.f) * btQuaternion(btVector3(0, 1, 0), r.y() * SIMD_PI / 180.f) * btQuaternion(btVector3(0, 0, 1), r.z() * SIMD_PI / 180.f));
            }

            auto* motionState = new btDefaultMotionState(startXform);

#ifndef IS_SERVER
            if (!GetGameObject()->IsLocallyControlled() && bodyType == Type::DYNAMIC)
            {
                bodyType = Type::KINEMATIC;
                mass = 0.0f;

                inertia = { 0,0,0 };
            }
#endif

            btRigidBody::btRigidBodyConstructionInfo ci((bodyType == Type::DYNAMIC) ? mass : 0.0f, motionState, collider->GetShape(), inertia);

            body = new btRigidBody(ci);
            
            body->setDamping(linearDamping, angularDamping);
            body->setFriction(friction);
            
            collider->GetShape()->setMargin(colliderMargin);

#ifndef IS_SERVER
            if (bodyType == Type::KINEMATIC)
                body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
#endif
            
            body->setUserPointer(this);

#ifdef _WIN32
            PhysicsDebugger::RegisterRigidbody(body);
#endif

            PhysicsWorld::GetInstance().AddBody(body);
        }

        void ApplyCentralImpulse(const Vector<float, 3>& impulse)
        {
            if (!body)
                return;

            body->applyCentralImpulse({ impulse.x(), impulse.y(), impulse.z() });
            body->activate(true);

            QueueToServer(PacketType::C2S_Rigidbody_Impulse, ImpulseCommand{ GetGameObject()->GetAbsolutePath(), false, impulse, { 0.0f, 0.0f, 0.0f } });
        }

        void SetHorizontalVelocity(const Vector<float, 3>& desiredVelocity)
        {
            if (!body)
                return;

            btVector3 velocity = body->getLinearVelocity();

            velocity.setX(desiredVelocity.x());
            velocity.setZ(desiredVelocity.z());

            body->setLinearVelocity(velocity);  body->activate(true);

            QueueToServer(PacketType::C2S_Rigidbody_SetVelocity, SetVelocityCommand{ GetGameObject()->GetAbsolutePath(), desiredVelocity });
        }

        void SyncToBullet() override
        {
            if (IsAuthoritative())
            {
                if (bodyType == Type::KINEMATIC)
                    PushTransformToPhysics();
            }
            else
                PushTransformToPhysics();
        }

        void SyncFromBullet() override
        {
            if (!IsAuthoritative())
                return;

#ifdef IS_SERVER
            SyncTransformFromLocalPhysics();
#else
            if (GetGameObject()->IsLocallyControlled())
                SyncTransformFromLocalPhysics();
#endif
        }

        btCollisionObject* GetCollisionObject() const override
        {
            return body;
        }

        bool IsAuthoritative() const noexcept override
        {
#ifdef IS_SERVER
            return true;
#else
            return GetGameObject()->IsLocallyControlled();
#endif
        }

        void ApplyImpulseAtPoint(const Vector<float, 3>& impulse, const Vector<float, 3>& worldPoint)
        {
            if (!body)
                return;

            btVector3 btImpulse(impulse.x(), impulse.y(), impulse.z());
            btVector3 rel = btVector3(worldPoint.x(), worldPoint.y(), worldPoint.z()) - body->getCenterOfMassPosition();

            body->applyImpulse(btImpulse, rel);
            body->activate(true);

            QueueToServer(PacketType::C2S_Rigidbody_Impulse, ImpulseCommand{ GetGameObject()->GetAbsolutePath(), true, impulse, worldPoint });
        }

        void LockRotation(const Axis axis)
        {
#ifdef IS_SERVER
            lockedAxes = axis;

            ApplyAngularFactor();

            Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(GetGameObject(), typeid(Rigidbody));
#endif
        }

        void PushTransformToPhysics()
        {
            const auto transform = GetGameObject()->GetTransform();

            const auto position = transform->GetWorldPosition();
            const auto rotation = transform->GetWorldRotation();

            const btTransform bulletTransform = MakeBtTransform(position, rotation);

            body->setWorldTransform(bulletTransform);
            body->getMotionState()->setWorldTransform(bulletTransform);

            if (bodyType == Type::STATIC)
                PhysicsWorld::GetInstance().GetHandle()->updateSingleAabb(body);
        }

        [[nodiscard]]
        btRigidBody* GetBtBody() const
        {
            return body;
        }

        [[nodiscard]] Type GetBodyType() const
        {
            return bodyType;
        }

        static std::shared_ptr<Rigidbody> Create(Type bodyType = Type::DYNAMIC, float mass = 1.0f, float linearDamping = 0.05f, float angularDamping = 0.95f, float friction = 0.2f, float colliderMargin = 0.02f)
        {
            std::shared_ptr<Rigidbody> result(new Rigidbody());

            result->mass = mass;
            result->linearDamping = linearDamping;
            result->angularDamping = angularDamping;
            result->friction = friction;
            result->colliderMargin = colliderMargin;
            result->bodyType = bodyType;

            return result;
        }

    private:

        Rigidbody() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & BOOST_SERIALIZATION_NVP(mass);
            archive & BOOST_SERIALIZATION_NVP(linearDamping);
            archive & BOOST_SERIALIZATION_NVP(angularDamping);
            archive & BOOST_SERIALIZATION_NVP(friction);
            archive & BOOST_SERIALIZATION_NVP(colliderMargin);
            archive & BOOST_SERIALIZATION_NVP(bodyType);

            auto flags = static_cast<std::uint8_t>(lockedAxes);

            archive & BOOST_SERIALIZATION_NVP(flags);

            if constexpr (Archive::is_loading::value)
                lockedAxes = static_cast<Axis>(flags);
        }

        void RegisterWithWorld()
        {
            PhysicsWorld::GetInstance().AddBody(body);
        }

        void UnregisterFromWorld()
        {
            PhysicsWorld::GetInstance().RemoveBody(body);
        }

        void QueueImpulseToServer(const Vector<float, 3>& impulse, std::optional<Vector<float, 3>> point)
        {
            Blaster::Client::Network::ClientNetwork::GetInstance().Send(PacketType::C2S_Rigidbody_Impulse, ImpulseCommand{ GetGameObject()->GetAbsolutePath(), point.has_value(), impulse, point.has_value() ? point.value() : Vector<float, 3>{ 0.0f, 0.0f, 0.0f } });
        
#ifdef _WIN32
            PhysicsDebugger::LogImpulsePacket(ImpulseCommand{ GetGameObject()->GetAbsolutePath(), point.has_value(), impulse, point.has_value() ? point.value() : Vector<float, 3>{ 0.0f, 0.0f, 0.0f } });
#endif
        }

        void SyncTransformFromLocalPhysics()
        {
            if (!body)
                return;

            const btTransform& bulletTransform = body->getWorldTransform();

            auto position = bulletTransform.getOrigin();
            
            float rotationX, rotationY, rotationZ;
                
            bulletTransform.getRotation().getEulerZYX(rotationZ, rotationY, rotationX);

            btVector3 rotation = { rotationX, rotationY, rotationZ };

            auto transform = GetGameObject()->GetTransform();

            transform->SetLocalPosition({ position.x(), position.y(), position.z() }, false);

            float rotZ, rotY, rotX;

            bulletTransform.getRotation().getEulerZYX(rotZ, rotY, rotX);

            auto deg = 180.0f / std::numbers::pi_v<float>;

            transform->SetLocalRotation({ rotX * deg, rotY * deg, rotZ * deg }, false);
        }

        void ApplyAngularFactor() const
        {
            if (!body)
                return;

            btVector3 angularFactor(1, 1, 1);

            if (static_cast<uint8_t>(lockedAxes) & static_cast<uint8_t>(Axis::X))
                angularFactor.setX(0);

            if (static_cast<uint8_t>(lockedAxes) & static_cast<uint8_t>(Axis::Y))
                angularFactor.setY(0);

            if (static_cast<uint8_t>(lockedAxes) & static_cast<uint8_t>(Axis::Z))
                angularFactor.setZ(0);

            body->setAngularFactor(angularFactor);
        }

        static btTransform MakeBtTransform(const Vector<float, 3>& position, const Vector<float, 3>& rotationDegrees)
        {
            btTransform result;

            result.setIdentity();
            result.setOrigin({ position.x(), position.y(), position.z() });

            const float k = std::numbers::pi_v<float> / 180.f;

            result.setRotation(btQuaternion(btVector3(1, 0, 0), rotationDegrees.x() * k) * btQuaternion(btVector3(0, 1, 0), rotationDegrees.y() * k) * btQuaternion(btVector3(0, 0, 1), rotationDegrees.z() * k));
            
            return result;
        }

        float mass;

        float linearDamping = 0.15f;
        float angularDamping = 0.95f;
        float friction = 0.8f;
        float colliderMargin = 0.02f;

        Axis lockedAxes = Axis::NONE;

        Type bodyType;
        btRigidBody* body = nullptr;

        DESCRIBE_AND_REGISTER(Rigidbody, (Component), (), (), (mass, linearDamping, angularDamping, friction, colliderMargin, bodyType))
    };

    inline Rigidbody::Axis operator|(Rigidbody::Axis a, Rigidbody::Axis b)
    {
        return static_cast<Rigidbody::Axis>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }
}

REGISTER_COMPONENT(Blaster::Independent::Physics::Rigidbody, 48735)