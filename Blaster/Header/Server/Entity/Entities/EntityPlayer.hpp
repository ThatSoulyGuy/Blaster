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
#include "Independent/Physics/CharacterController.hpp"
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

        enum class Team
        {
            Red,
            Blue
        };

        EntityPlayer(const EntityPlayer&) = delete;
        EntityPlayer(EntityPlayer&&) = delete;
        EntityPlayer& operator=(const EntityPlayer&) = delete;
        EntityPlayer& operator=(EntityPlayer&&) = delete;

        void Initialize() override
        {
            if (GetGameObject()->IsLocallyControlled())
            {
                const auto cameraGameObject = GameObjectManager::GetInstance().Register(GameObject::Create("camera"), GetGameObject()->GetAbsolutePath());

                cameraGameObject->GetTransform3d()->SetLocalPosition({ 0.0f, 16.0f, 0.0f });
                camera = cameraGameObject->AddComponent(Camera::Create(45.0f, 0.01f, 10000.0f));
                
                modelGameObject = GameObjectManager::GetInstance().Register(GameObject::Create("model"), GetGameObject()->GetAbsolutePath());

                modelGameObject->GetTransform3d()->SetLocalPosition({ 0.0f, 6.0f, 0.0f });
                modelGameObject->GetTransform3d()->SetLocalRotation({ 90.0f, 0.0f, 0.0f });
                modelGameObject->GetTransform3d()->SetLocalScale({ 0.0002f, 0.0002f, 0.0002f });

                if (team == Team::Red)
                    modelGameObject->AddComponent(Model::Create({ "Blaster", "Model/MTF2_Red.fbx" }, true));
                else
                    modelGameObject->AddComponent(Model::Create({ "Blaster", "Model/MTF2_Blue.fbx" }, true));

                InputManager::GetInstance().SetMouseMode(MouseMode::LOCKED);
            }
        }

        void Update() override
        {
            if (!GetGameObject()->IsLocallyControlled())
                return;

            modelGameObject->SetLocallyActive(false);

            if (InputManager::GetInstance().GetKeyState(KeyCode::C, KeyState::PRESSED))
                std::cout << "Current Position: " << GetGameObject()->GetTransform3d()->GetWorldPosition() << std::endl;

            GameObjectManager::GetInstance().Get(GetGameObject()->GetAbsolutePath() + ".model").value()->GetTransform3d()->SetLocalRotation({ 90.0f, camera->GetGameObject()->GetTransform3d()->GetLocalRotation().y(), 0.0f });

            UpdateMouselook();
            UpdateMovement();
        }

        static std::shared_ptr<EntityPlayer> Create(const Team& team)
        {
            std::shared_ptr<EntityPlayer> result(new EntityPlayer());

            result->team = team;

            return result;
        }

    private:

        EntityPlayer()
        {
            Builder<EntityBase>::New()
                .Set(EntityBase::RegistryNameSetter{ "entity_player" })
                .Set(EntityBase::CurrentHealthSetter{ 100.0f })
                .Set(EntityBase::MaximumHealthSetter{ 100.0f })
                .Set(EntityBase::MovementSpeedSetter{ 30.0f })
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

        void UpdateMouselook() const
        {
            if (InputManager::GetInstance().GetKeyState(KeyCode::ESCAPE, KeyState::PRESSED))
                InputManager::GetInstance().SetMouseMode(!InputManager::GetInstance().GetMouseMode());

            Vector<float, 2> mouseDelta = InputManager::GetInstance().GetMouseDelta();

            const auto transform = camera->GetGameObject()->GetTransform3d();
            Vector<float, 3> rotation = transform->GetLocalRotation();

            rotation.y() -= mouseDelta.x() * MouseSensitivity;
            rotation.x() += mouseDelta.y() * MouseSensitivity;

            transform->SetLocalRotation(rotation);
        }

        void UpdateMovement() const
        {
            constexpr float epsilon = 1e-6f;

            auto animator = modelGameObject->GetComponent<Animator>().value();
            auto controller = GetGameObject()->GetComponent<CharacterController>().value();

            Vector<float, 3> forward = camera->GetGameObject()->GetTransform3d()->GetForward();

            forward.y() = 0;

            if (Vector<float, 3>::LengthSquared(forward) < epsilon)
                forward = { 0.0f, 0.0f, 1.0f };
            else
                forward = Vector<float, 3>::Normalize(forward);

            Vector<float, 3> right = { -forward.z(), 0.0f, forward.x() };

            Vector<float, 3> direction{ 0.0f, 0.0f, 0.0f };

            if (InputManager::GetInstance().GetKeyState(KeyCode::W, KeyState::HELD))
                direction += forward;

            if (InputManager::GetInstance().GetKeyState(KeyCode::S, KeyState::HELD))
                direction -= forward;

            if (InputManager::GetInstance().GetKeyState(KeyCode::D, KeyState::HELD))
                direction += right;

            if (InputManager::GetInstance().GetKeyState(KeyCode::A, KeyState::HELD))
                direction -= right;

            if (Vector<float, 3>::LengthSquared(direction) > epsilon)
            {
                direction = Vector<float, 3>::Normalize(direction);

                controller->SetWalkDirection(direction * GetMovementSpeed());

                if (!animator->IsPlaying("mtf2.walk"))
                    animator->Play("mtf2.walk", blendTime, 1.8f * Vector<float, 3>::LengthSquared(direction));
            }
            else
            {
                controller->SetWalkDirection({ 0.0f, 0.0f, 0.0f });

                if (!animator->IsPlaying("mtf2.idle"))
                    animator->Play("mtf2.idle", blendTime);
            }

            if (InputManager::GetInstance().GetKeyState(KeyCode::SPACE, KeyState::PRESSED) && controller->OnGround())
                controller->Jump();
        }

        Team team;

        std::shared_ptr<Camera> camera;
        std::shared_ptr<GameObject> modelGameObject;

        constexpr static float blendTime = 0.20f;

        BUILDABLE_PROPERTY(MouseSensitivity, float, EntityPlayer)

        DESCRIBE_AND_REGISTER(EntityPlayer, (EntityBase<EntityPlayer>), (), (), (MouseSensitivity))

    };
}

REGISTER_COMPONENT(Blaster::Server::Entity::Entities::EntityPlayer, 57854)