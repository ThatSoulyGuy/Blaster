#pragma once

#include "Client/Core/InputManager.hpp"
#include "Client/Render/Vertices/FatVertex.hpp"
#include "Client/Render/Camera.hpp"
#include "Client/Render/Model.hpp"
#include "Client/Render/ShaderManager.hpp"
#include "Client/Render/TextureManager.hpp"
#include "Independent/ComponentRegistry.hpp"
#include "Independent/ECS/GameObject.hpp"
#include "Independent/Utility/Time.hpp"
#include "Independent/Thread/MainThreadExecutor.hpp"
#include "Server/Entity/EntityBase.hpp"

using namespace std::chrono_literals;
using namespace Blaster::Client::Network;
using namespace Blaster::Client::Render::Vertices;
using namespace Blaster::Client::Render;
using namespace Blaster::Independent::Thread;

namespace Blaster::Server::Entity::Entities
{
    class EntityPlayer final : public EntityBase<EntityPlayer>
    {

    public:

        EntityPlayer(const EntityPlayer&) = delete;
        EntityPlayer(EntityPlayer&&) = delete;
        EntityPlayer& operator=(const EntityPlayer&) = delete;
        EntityPlayer& operator=(EntityPlayer&&) = delete;

        void Initialize() override
        {
            if (GetGameObject()->IsLocallyControlled())
            {
                const auto cameraGameObject = GameObjectManager::GetInstance().Register(GameObject::Create("camera"), GetGameObject()->GetAbsolutePath());

                cameraGameObject->GetTransform()->SetLocalPosition({ 0.0f, 0.0f, 0.0f });
                camera = cameraGameObject->AddComponent(Camera::Create(45.0f, 0.01f, 10000.0f));

                const auto modelGameObject = GameObjectManager::GetInstance().Register(GameObject::Create("model"), GetGameObject()->GetAbsolutePath());

                modelGameObject->AddComponent(TextureManager::GetInstance().Get("blaster.wood").value());

                modelGameObject->AddComponent(Model::Create({ "Blaster", "Model/Player.fbx" }));

                InputManager::GetInstance().SetMouseMode(MouseMode::LOCKED);
            }
#ifndef IS_SERVER
            else
            {
                const auto modelGameObject = GameObjectManager::GetInstance().Register(GameObject::Create("model"), GetGameObject()->GetAbsolutePath());

                modelGameObject->AddComponent(TextureManager::GetInstance().Get("blaster.wood").value());

                modelGameObject->AddComponent(Model::Create({ "Blaster", "Model/Player.fbx" }));
            }
#endif
        }

        void Update() override
        {
            UpdateMouselook();
            UpdateMovement();
        }

        static std::shared_ptr<EntityPlayer> Create()
        {
            return std::shared_ptr<EntityPlayer>(new EntityPlayer());
        }

    private:

        EntityPlayer()
        {
            Builder<EntityBase>::New()
                    .Set(EntityBase::RegistryNameSetter{ "entity_player" })
                    .Set(EntityBase::CurrentHealthSetter{ 100.0f })
                    .Set(EntityBase::MaximumHealthSetter{ 100.0f })
                    .Set(EntityBase::MovementSpeedSetter{ 0.28f })
                    .Set(EntityBase::RunningMultiplierSetter{ 1.2f })
                    .Set(EntityBase::JumpHeightSetter{ 5.0f })
                    .Set(EntityBase::CanJumpSetter{ true })
                    .Build(static_cast<EntityBase&>(*this));

            Builder<EntityPlayer>::New()
                .Set(EntityPlayer::MouseSensitivitySetter{ 0.1f })
                .Build(static_cast<EntityPlayer&>(*this));
        }

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & boost::serialization::make_nvp("registryName", RegistryName);
            archive & boost::serialization::make_nvp("currentHealth", CurrentHealth);
            archive & boost::serialization::make_nvp("maximumHealth", MaximumHealth);
            archive & boost::serialization::make_nvp("movementSpeed", MovementSpeed);
            archive & boost::serialization::make_nvp("runningMultiplier", RunningMultiplier);
            archive & boost::serialization::make_nvp("jumpHeight", JumpHeight);
            archive & boost::serialization::make_nvp("canJump", CanJump);
        }

        void UpdateMouselook()
        {
            if (!GetGameObject()->IsLocallyControlled())
                return;

            if (InputManager::GetInstance().GetKeyState(KeyCode::ESCAPE, KeyState::PRESSED))
                InputManager::GetInstance().SetMouseMode(!InputManager::GetInstance().GetMouseMode());

            Vector<float, 2> mouseDelta = InputManager::GetInstance().GetMouseDelta();

            const auto transform = camera->GetGameObject()->GetTransform();
            Vector<float, 3> rotation = transform->GetLocalRotation();

            rotation.y() -= mouseDelta.x() * MouseSensitivity;
            rotation.x() += mouseDelta.y() * MouseSensitivity;

            transform->SetLocalRotation(rotation);
        }

        void UpdateMovement()
        {
            if (!GetGameObject()->IsLocallyControlled())
                return;

            Vector<float, 3> direction = { 0.0f, 0.0f, 0.0f };

            const Vector<float, 3> forward = camera->GetGameObject()->GetTransform()->GetForward();
            const Vector<float, 3> right = camera->GetGameObject()->GetTransform()->GetRight();

            if (InputManager::GetInstance().GetKeyState(KeyCode::W, KeyState::HELD))
                direction += forward;

            if (InputManager::GetInstance().GetKeyState(KeyCode::S, KeyState::HELD))
                direction -= forward;

            if (InputManager::GetInstance().GetKeyState(KeyCode::A, KeyState::HELD))
                direction += right;

            if (InputManager::GetInstance().GetKeyState(KeyCode::D, KeyState::HELD))
                direction -= right;

            const auto transform = GetGameObject()->GetTransform();

            if (const float length = Vector<float, 3>::Magnitude(direction); length > 1e-4f)
            {
                direction /= length;

                constexpr float targetSpeed = 6.0f;
                Vector<float,3> desiredVel = direction * targetSpeed;

                const auto body = GetGameObject()->GetComponent<Rigidbody>().value()->GetNativeBody();
                const btVector3 linearVelocity = body->getLinearVelocity();

                const btVector3 deltaV(desiredVel.x() - linearVelocity.x(),0.0f, desiredVel.z() - linearVelocity.z());

                GetGameObject()->GetComponent<Rigidbody>().value()->AddImpulse(Vector<float, 3>{ deltaV.x(), 0, deltaV.z() } * body->getMass());
            }
        }

        std::shared_ptr<Camera> camera;

        BUILDABLE_PROPERTY(MouseSensitivity, float, EntityPlayer)

        DESCRIBE_AND_REGISTER(EntityPlayer, (EntityBase<EntityPlayer>), (), (), (camera, MouseSensitivity))

    };
}

REGISTER_COMPONENT(Blaster::Server::Entity::Entities::EntityPlayer)