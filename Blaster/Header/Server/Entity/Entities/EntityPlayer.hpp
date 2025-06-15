#pragma once

#include "Client/Core/InputManager.hpp"
//#include "Client/Network/ClientRpc.hpp"
#include "Client/Render/Vertices/FatVertex.hpp"
#include "Client/Render/Camera.hpp"
#include "Client/Render/ShaderManager.hpp"
#include "Client/Render/TextureManager.hpp"
#include "Independent/ComponentRegistry.hpp"
#include "Independent/ECS/GameObject.hpp"
#include "Independent/Utility/Time.hpp"
#include "Independent/Thread/MainThreadExecutor.hpp"
#include "Server/Entity/EntityBase.hpp"
//#include "Server/Network/ServerSynchronization.hpp"

using namespace std::chrono_literals;
using namespace Blaster::Client::Network;
using namespace Blaster::Client::Render::Vertices;
using namespace Blaster::Client::Render;
using namespace Blaster::Independent::Thread;

namespace Blaster::Server::Entity::Entities
{
    /*
    class EntityPlayer final : public EntityBase<EntityPlayer>
    {

    public:

        EntityPlayer(const EntityPlayer&) = delete;
        EntityPlayer(EntityPlayer&&) = delete;
        EntityPlayer& operator=(const EntityPlayer&) = delete;
        EntityPlayer& operator=(EntityPlayer&&) = delete;

        void Initialize() override
        {
            if (!GetGameObject()->IsLocallyControlled())
                return;

            ClientRpc::CreateGameObject(GetGameObject()->GetName() + ".camera");

            std::this_thread::sleep_for(20ms);

            auto cameraComponentFuture = ClientRpc::AddComponent(GetGameObject()->GetName() + ".camera", Camera::Create(45.0f, 0.01f, 1000.0f));

            std::this_thread::sleep_for(20ms);

            std::thread([future = std::move(cameraComponentFuture), this]() mutable
            {
                camera = std::static_pointer_cast<Camera>(future.get());

                std::this_thread::sleep_for(20ms);

                InputManager::GetInstance().SetMouseMode(MouseMode::LOCKED);

                std::this_thread::sleep_for(20ms);

                ClientRpc::AddComponent(GetGameObject()->GetName() + ".camera", Model::Create({ "Blaster", "Model/Player.fbx" }));

                std::this_thread::sleep_for(20ms);

                ClientRpc::AddComponent(GetGameObject()->GetName() + ".camera", TextureManager::GetInstance().Get("blaster.wood").value());

                std::cout << "Initialized EntityPlayer" << std::endl;

            }).detach();
        }

        void Update() override
        {
            UpdateMouselook();
            UpdateMovement();
        }

        [[nodiscard]]
        std::string GetTypeName() const override
        {
            return typeid(EntityPlayer).name();
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

            if (!camera)
            {
                if (GetGameObject()->HasChild("camera") && GetGameObject()->GetChild("camera").value()->HasComponent<Camera>())
                    camera = GetGameObject()->GetChild("camera").value()->GetComponent<Camera>().value();

                return;
            }

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

            if (!camera)
            {
                if (GetGameObject()->HasChild("camera") && GetGameObject()->GetChild("camera").value()->HasComponent<Camera>())
                    camera = GetGameObject()->GetChild("camera").value()->GetComponent<Camera>().value();

                return;
            }

            Vector<float, 3> movement = { 0.0f, 0.0f, 0.0f };

            const Vector<float, 3> forward = camera->GetGameObject()->GetTransform()->GetForward();
            const Vector<float, 3> right = camera->GetGameObject()->GetTransform()->GetRight();

            if (InputManager::GetInstance().GetKeyState(KeyCode::W, KeyState::HELD))
                movement += forward * MovementSpeed;

            if (InputManager::GetInstance().GetKeyState(KeyCode::S, KeyState::HELD))
                movement -= forward * MovementSpeed;

            if (InputManager::GetInstance().GetKeyState(KeyCode::A, KeyState::HELD))
                movement += right * MovementSpeed;

            if (InputManager::GetInstance().GetKeyState(KeyCode::D, KeyState::HELD))
                movement -= right * MovementSpeed;

            const auto transform = GetGameObject()->GetTransform();

            transform->Translate(movement);

            syncAccumulator += Time::GetInstance().GetDeltaTime();

            if (syncAccumulator >= syncPeriod)
            {
                syncAccumulator = 0.0f;

                ClientRpc::TranslateTo(GetGameObject()->GetName(), transform->GetLocalPosition(), 0.0f);
            }
        }

        std::shared_ptr<Camera> camera;

        static constexpr float syncPeriod = 0.10f;
        float syncAccumulator = 0.0f;

        BUILDABLE_PROPERTY(MouseSensitivity, float, EntityPlayer)

    };*/
}

//REGISTER_COMPONENT(Blaster::Server::Entity::Entities::EntityPlayer)