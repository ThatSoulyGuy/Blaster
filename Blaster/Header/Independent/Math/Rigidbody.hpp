#pragma once

#include "Independent/Math/Vector.hpp"
#include "Independent/Network/CommonNetwork.hpp"
#include "Independent/Collider/ColliderBase.hpp"
#include "Independent/Collider/PhysicsWorld.hpp"
#include "Independent/ECS/Component.hpp"
#include "Independent/ECS/GameObject.hpp"
#include "Independent/Math/Transform.hpp"
#include "Client/Network/ClientNetwork.hpp"

using namespace Blaster::Independent::Collider;

namespace Blaster::Independent::Math
{
    struct OpRigidbodyOperation
    {
        std::string path;
        Vector<float, 3> value;
    };

    struct OpRigidbodySetTransform
    {
        std::string path;
        Vector<float, 3> position;
        Vector<float, 3> rotation;
    };
}

namespace Blaster::Independent::Network
{
    template <>
    struct DataConversion<Blaster::Independent::Math::OpRigidbodyOperation> : DataConversionBase<DataConversion<Blaster::Independent::Math::OpRigidbodyOperation>, Blaster::Independent::Math::OpRigidbodyOperation>
    {
        using Type = Blaster::Independent::Math::OpRigidbodyOperation;

        static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
        {
            CommonNetwork::EncodeString(buffer, operation.path);
            DataConversion<Vector<float, 3>>::Encode(operation.value, buffer);
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t offset = 0;

            Type out;

            out.path = CommonNetwork::DecodeString(bytes, offset);
            out.value = std::any_cast<Vector<float, 3>>(DataConversion<Vector<float, 3>>::Decode(bytes.subspan(offset, DataConversion<Vector<float, 3>>::kWireSize)));

            return out;
        }
    };

    template <>
    struct DataConversion<Blaster::Independent::Math::OpRigidbodySetTransform> : DataConversionBase<DataConversion<Blaster::Independent::Math::OpRigidbodySetTransform>, Blaster::Independent::Math::OpRigidbodySetTransform>
    {
        using Type = Blaster::Independent::Math::OpRigidbodySetTransform;

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

namespace Blaster::Independent::Math
{
    class Rigidbody final : public Component
    {

    public:

        enum class Axis : uint8_t
        {
            None = 0,
            X = 1,
            Y = 2,
            Z = 4
        };

        ~Rigidbody() override
        {
            if (body)
                PhysicsWorld::GetInstance().UnregisterRigidBody(body.get());
        }

        void Initialize() override
        {
            const auto gameObject = GetGameObject();

            if (!gameObject)
                return;

            std::shared_ptr<ColliderBase> collider = nullptr;

            for (const auto &comp: gameObject->GetComponentMap() | std::views::values)
            {
                if (const auto col = std::dynamic_pointer_cast<ColliderBase>(comp))
                {
                    collider = col;
                    break;
                }
            }

            if (!collider)
            {
                std::cerr << "Rigidbody added to game object '" << GetGameObject()->GetAbsolutePath() << "' without collider!" << std::endl;
                return;
            }

            const auto transform = gameObject->GetTransform();

            btTransform start;

            start.setIdentity();

            const auto position = transform->GetLocalPosition();
            const auto rotation = transform->GetLocalRotation() * (std::numbers::pi_v<float> / 180.f);

            start.setOrigin({ position.x(), position.y(), position.z() });
            start.getBasis().setEulerZYX(rotation.z(), rotation.y(), rotation.x());

            shape = collider->GetShape();

            btVector3 inertia(0, 0, 0);

            if (isDynamic)
                shape->calculateLocalInertia(mass, inertia);

            motionState = std::make_unique<TransformMotionState>(transform, start);

            btRigidBody::btRigidBodyConstructionInfo constructionInformation(mass, motionState.get(), shape, inertia);

            body = std::make_unique<btRigidBody>(constructionInformation);

            PhysicsWorld::GetInstance().RegisterRigidBody(body.get());
        }

        void Update() override
        {
            if (!isDynamic && queuedPosition && queuedRotation && body)
            {
                SetStaticTransform(*queuedPosition, *queuedRotation);
                queuedPosition.reset();
                queuedRotation.reset();
            }

#ifndef IS_SERVER
            if (isDynamic && !GetGameObject()->IsLocallyControlled())
                return;
#endif

            ApplyAngularFactor();
        }

        void SetStaticTransform(const Vector<float, 3>& position, const Vector<float, 3>& rotationDegrees)
        {
            if (!body)
            {
                queuedPosition = position;
                queuedRotation = rotationDegrees;

                return;
            }

            btTransform bulletTransform;

            bulletTransform.setIdentity();
            bulletTransform.setOrigin({ position.x(), position.y(), position.z() });

            constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.f;

            bulletTransform.getBasis().setEulerZYX(rotationDegrees.z() * degreesToRadians, rotationDegrees.y() * degreesToRadians, rotationDegrees.x() * degreesToRadians);

            body->setWorldTransform(bulletTransform);
            motionState->setWorldTransform(bulletTransform);

            PhysicsWorld::GetInstance().GetWorld()->updateSingleAabb(body.get());

            const auto transform = GetGameObject()->GetTransform();

            transform->SetLocalPosition(position, false);
            transform->SetLocalRotation(rotationDegrees, false);

#ifndef IS_SERVER
            ClientNetwork::GetInstance().Send(PacketType::C2S_Rigidbody_SetStaticTransform, OpRigidbodySetTransform{ GetGameObject()->GetAbsolutePath(), position, rotationDegrees });
#else
            Blaster::Independent::ECS::Synchronization::SenderSynchronization::MarkDirty(GetGameObject(), typeid(Rigidbody));
#endif
        }

        void AddForce(const Vector<float, 3>& force) const
        {
            if (!body || !isDynamic)
                return;

            body->activate(true);
            body->applyCentralForce( { force.x(), force.y(), force.z() } );

#ifndef IS_SERVER
            ClientNetwork::GetInstance().Send(PacketType::C2S_Rigidbody_AddForce, OpRigidbodyOperation { GetGameObject()->GetAbsolutePath(), force });
#endif
        }

        void AddImpulse(const Vector<float, 3>& impulse) const
        {
            if (!body || !isDynamic)
                return;

            body->activate(true);
            body->applyCentralImpulse( { impulse.x(), impulse.y(), impulse.z() } );

#ifndef IS_SERVER
            ClientNetwork::GetInstance().Send(PacketType::C2S_Rigidbody_AddImpulse, OpRigidbodyOperation{ GetGameObject()->GetAbsolutePath(), impulse });
#endif
        }

        void LockRotation(const Axis axis)
        {
#ifdef IS_SERVER
            lockedAxes = axis;

            ApplyAngularFactor();

            Blaster::Independent::ECS::Synchronization::SenderSynchronization::MarkDirty(GetGameObject(), typeid(Rigidbody));
#endif
        }

        [[nodiscard]]
        btRigidBody* GetNativeBody() const
        {
            return body.get();
        }

        static std::shared_ptr<Rigidbody> Create(const bool dynamic, const float mass = 0.0f)
        {
            std::shared_ptr<Rigidbody> result(new Rigidbody);

            result->isDynamic = dynamic;
            result->mass = dynamic ? mass : 0.f;

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

            archive & BOOST_SERIALIZATION_NVP(isDynamic);
            archive & BOOST_SERIALIZATION_NVP(mass);

            auto flags = static_cast<std::uint8_t>(lockedAxes);

            archive & BOOST_SERIALIZATION_NVP(flags);

            if constexpr(Archive::is_loading::value)
                lockedAxes = static_cast<Axis>(flags);
        }

        void ApplyAngularFactor() const
        {
            if (!body)
                return;

            btVector3 angularFactor(1,1,1);

            if (static_cast<uint8_t>(lockedAxes) & static_cast<uint8_t>(Axis::X))
                angularFactor.setX(0);

            if (static_cast<uint8_t>(lockedAxes) & static_cast<uint8_t>(Axis::Y))
                angularFactor.setY(0);

            if (static_cast<uint8_t>(lockedAxes) & static_cast<uint8_t>(Axis::Z))
                angularFactor.setZ(0);

            body->setAngularFactor(angularFactor);
        }

        class TransformMotionState final : public btMotionState
        {

        public:

            explicit TransformMotionState(std::shared_ptr<Transform> t, const btTransform& start) : graphicsWorldTransform(start), transform(std::move(t)) {}

            void setWorldTransform(const btTransform& w) override
            {
                graphicsWorldTransform = w;

                const auto position = Vector<float,3>{ w.getOrigin().x(), w.getOrigin().y(), w.getOrigin().z() };

                btVector3 euler;

                w.getBasis().getEulerZYX(euler[2],euler[1],euler[0]);

                constexpr float rad2deg = 180.f / std::numbers::pi_v<float>;

                transform->SetLocalPosition(position, false);
                transform->SetLocalRotation( { euler[0] * rad2deg,euler[1] * rad2deg, euler[2] * rad2deg }, false);
            }

            void getWorldTransform(btTransform& world) const override
            {
                const auto pos = transform->GetLocalPosition();
                const auto rot = transform->GetLocalRotation();

                constexpr float deg2rad = std::numbers::pi_v<float> / 180.f;

                world.setIdentity();
                world.setOrigin( { pos.x(), pos.y(), pos.z() } );
                world.getBasis().setEulerZYX(rot.z() * deg2rad, rot.y() * deg2rad, rot.x() * deg2rad);
            }

            btTransform graphicsWorldTransform;

        private:

            std::shared_ptr<Transform> transform;

        };

        bool isDynamic = false;
        float mass = 0.0f;

        Axis lockedAxes = Axis::None;

        std::optional<Vector<float,3>> queuedPosition;
        std::optional<Vector<float,3>> queuedRotation;

        btCollisionShape* shape = nullptr;
        std::unique_ptr<TransformMotionState> motionState;
        std::unique_ptr<btRigidBody> body;

        DESCRIBE_AND_REGISTER(Rigidbody, (Component), (), (), (isDynamic, mass, lockedAxes))
    };

    inline Rigidbody::Axis operator|(Rigidbody::Axis a, Rigidbody::Axis b)
    {
        return static_cast<Rigidbody::Axis>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }
}

REGISTER_COMPONENT(Blaster::Independent::Math::Rigidbody)