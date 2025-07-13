#pragma once

#include "Independent/ECS/GameObject.hpp"
#include "Independent/Math/Transform.hpp"
#include "Independent/Physics/Collider.hpp"
#include "Independent/Physics/PhysicsWorld.hpp"

#ifdef IS_SERVER
#include "Server/Network/ServerNetwork.hpp"
#else
#include "Client/Network/ClientNetwork.hpp"
#endif

using namespace Blaster::Independent::ECS;
using namespace Blaster::Independent::Math;
using namespace Blaster::Independent::Network;

namespace Blaster::Independent::Physics
{
    struct ImpulseCommand
    {
        std::string path;

        Vector<float, 3> impulse;
        Vector<float, 3> point;

        static constexpr std::uint8_t CODE = 42;
    };

    struct SetTransformCommand
    {
        std::string path;

        Vector<float, 3> position;
        Vector<float, 3> rotation;

        static constexpr std::uint8_t CODE = 43;
    };
}

namespace Blaster::Independent::Network
{
    template <>
    struct DataConversion<Blaster::Independent::Physics::ImpulseCommand> : DataConversionBase<DataConversion<Blaster::Independent::Physics::ImpulseCommand>, Blaster::Independent::Physics::ImpulseCommand>
    {
        using Type = Blaster::Independent::Physics::ImpulseCommand;

        static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
        {
            CommonNetwork::EncodeString(buffer, operation.path);
            DataConversion<Vector<float, 3>>::Encode(operation.impulse, buffer);
            DataConversion<Vector<float, 3>>::Encode(operation.point, buffer);
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t offset = 0;

            Type out;

            out.path = CommonNetwork::DecodeString(bytes, offset);
            out.impulse = std::any_cast<Vector<float, 3>>(DataConversion<Vector<float, 3>>::Decode(bytes.subspan(offset, DataConversion<Vector<float, 3>>::kWireSize)));

            offset += DataConversion<Vector<float, 3>>::kWireSize;

            out.point = std::any_cast<Vector<float, 3>>(DataConversion<Vector<float, 3>>::Decode(bytes.subspan(offset, DataConversion<Vector<float, 3>>::kWireSize)));

            return out;
        }
    };

    template <>
    struct DataConversion<Blaster::Independent::Physics::SetTransformCommand> : DataConversionBase<DataConversion<Blaster::Independent::Physics::SetTransformCommand>, Blaster::Independent::Physics::SetTransformCommand>
    {
        using Type = Blaster::Independent::Physics::SetTransformCommand;

        static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
        {
            CommonNetwork::EncodeString(buffer, operation.path);

            DataConversion<Vector<float, 3>>::Encode(operation.position, buffer);
            DataConversion<Vector<float, 3>>::Encode(operation.rotation, buffer);
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t offset = 0;

            Type out;

            out.path = CommonNetwork::DecodeString(bytes, offset);

            out.position = std::any_cast<Vector<float, 3>>(DataConversion<Vector<float, 3>>::Decode(bytes.subspan(offset, DataConversion<Vector<float, 3>>::kWireSize)));

            offset += DataConversion<Vector<float, 3>>::kWireSize;

            out.rotation = std::any_cast<Vector<float, 3>>(DataConversion<Vector<float, 3>>::Decode(bytes.subspan(offset, DataConversion<Vector<float, 3>>::kWireSize)));

            return out;
        }
    };
}

namespace Blaster::Independent::Physics
{
    class Rigidbody final : public Component
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

        void Initialize() override
        {
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

            const auto position = transform->GetWorldPosition();
            const auto rotation = transform->GetWorldRotation();

            startXform.setOrigin({ position.x(), position.y(), position.z() });
            startXform.setRotation(btQuaternion(btVector3(1, 0, 0), btScalar(rotation.x() * std::numbers::pi_v<float> / 180.0f)) * btQuaternion(btVector3(0, 1, 0), btScalar(rotation.y() * std::numbers::pi_v<float> / 180.0f)) * btQuaternion(btVector3(0, 0, 1), btScalar(rotation.z() * std::numbers::pi_v<float> / 180.0f)));

            auto* motion = new btDefaultMotionState(startXform);

            btRigidBody::btRigidBodyConstructionInfo ci(bodyType == Type::DYNAMIC ? mass : 0.0f, motion, collider->GetShape(), inertia);

            body = new btRigidBody(ci);
            body->setUserPointer(static_cast<void*>(this));

            RegisterWithWorld();

#ifndef IS_SERVER
            GetGameObject()->GetTransform()->SetShouldSynchronize(false);
#endif
        }

        void Update() override
        {
#ifdef IS_SERVER
            if (bodyType == Type::DYNAMIC)
                SyncTransformFromPhysics();
            else
                PushTransformToPhysics();
#else
            SyncTransformFromPhysics();
#endif
        }

        void ApplyImpulse(const Vector<float, 3>& impulse, const Vector<float, 3>& relativePoint = { 0,0,0 })
        {
#ifdef IS_SERVER
            body->applyImpulse({ impulse.x(), impulse.y(), impulse.z() }, { relativePoint.x(), relativePoint.y(), relativePoint.z() });
            body->activate(true);
#else
            if (GetGameObject()->IsLocallyControlled())
                QueueImpulseToServer(impulse, relativePoint);
#endif
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

#ifndef IS_SERVER
            ClientNetwork::GetInstance().Send(PacketType::C2S_Rigidbody_SetTransform, SetTransformCommand{ position, rotation });
#endif
        }

        [[nodiscard]] btRigidBody* GetBtBody() const
        {
            return body;
        }

        [[nodiscard]] Type GetBodyType() const
        {
            return bodyType;
        }

        static std::shared_ptr<Rigidbody> Create(float mass = 1.0f, Type bodyType = Type::DYNAMIC)
        {
            std::shared_ptr<Rigidbody> result(new Rigidbody());

            result->mass = mass;
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
            archive & BOOST_SERIALIZATION_NVP(bodyType);

            auto flags = static_cast<std::uint8_t>(lockedAxes);

            archive& BOOST_SERIALIZATION_NVP(flags);

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

        void QueueImpulseToServer(const Vector<float, 3>& impulse, const Vector<float, 3>& point)
        {
            Blaster::Client::Network::ClientNetwork::GetInstance().Send(PacketType::C2S_Rigidbody_Impulse, ImpulseCommand{ GetGameObject()->GetAbsolutePath(), impulse, point });
        }

        void SyncTransformFromPhysics()
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
            transform->SetLocalRotation({ rotation[2] * 180.0f / std::numbers::pi_v<float>, rotation[1] * 180.0f / std::numbers::pi_v<float>, rotation[0] * 180.0f / std::numbers::pi_v<float> }, false);

            Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(GetGameObject(), typeid(Math::Transform));
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

        Axis lockedAxes = Axis::NONE;

        Type bodyType;
        btRigidBody* body = nullptr;

        DESCRIBE_AND_REGISTER(Rigidbody, (Component), (), (), (mass, bodyType))
    };

    inline Rigidbody::Axis operator|(Rigidbody::Axis a, Rigidbody::Axis b)
    {
        return static_cast<Rigidbody::Axis>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }
}

REGISTER_COMPONENT(Blaster::Independent::Physics::Rigidbody, 48735)