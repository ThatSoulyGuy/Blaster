#pragma once

#include "Client/Core/InputManager.hpp"
#include "Client/Render/Vertices/FatVertex.hpp"
#include "Client/Render/Camera.hpp"
#include "Client/Render/Model.hpp"
#include "Client/Render/ShaderManager.hpp"
#include "Client/Render/TextureManager.hpp"
#include "Client/UI/Elements/UIElementText.hpp"
#include "Client/UI/Elements/UIElementImage.hpp"
#include "Client/UI/Layouts/UILayoutGrid.hpp"
#include "Client/UI/UIBuilder.hpp"
#include "Independent/ECS/GameObject.hpp"
#include "Independent/Item/ItemRegistry.hpp"
#include "Independent/Utility/Time.hpp"
#include "Independent/Thread/MainThreadExecutor.hpp"
#include "Independent/Physics/CharacterController.hpp"
#include "Independent/ComponentRegistry.hpp"
#include "Server/Entity/EntityBase.hpp"

using namespace std::chrono_literals;
using namespace Blaster::Client::Network;
using namespace Blaster::Client::Render::Vertices;
using namespace Blaster::Client::Render;
using namespace Blaster::Client::UI::Elements;
using namespace Blaster::Client::UI::Layouts;
using namespace Blaster::Client::UI;
using namespace Blaster::Independent::Thread;
using namespace Blaster::Independent::Item;

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

        struct Hotbar
        {
            std::uint8_t index;
            std::array<std::uint32_t, 5> slots;

            template <typename Archive>
            void serialize(Archive& archive, const unsigned)
            {
                archive & BOOST_SERIALIZATION_NVP(index);
                archive & BOOST_SERIALIZATION_NVP(slots);
            }
        };

        EntityPlayer(const EntityPlayer&) = delete;
        EntityPlayer(EntityPlayer&&) = delete;
        EntityPlayer& operator=(const EntityPlayer&) = delete;
        EntityPlayer& operator=(EntityPlayer&&) = delete;

        void Initialize() override
        {
            currentHealth = GetMaximumHealth();

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

#ifndef IS_SERVER
            hudRoot = UIBuilder::NewMenu("ui_hud_" + Blaster::Client::Network::ClientNetwork::GetInstance().GetStringId())
                    .AddElement<UIElementText>("ui_health_text")
                        .CallAndThen<&Component::GetGameObject>([&](std::shared_ptr<GameObject> gameObject)
                            {
                                gameObject->GetTransform2d()->SetPosition({ 10.0f, -10.0f });
                                gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::BOTTOM | Transform2d::Anchor::LEFT);
                            })
                        .Call<&UIElementText::SetFont>(UIElementText::Font::Create({ "Blaster", "Font/DS-DIGIB.TTF" }, 48, 0, 8))
                        .Call<&UIElementText::SetText>("Current Health: -1")
                        .Call<&UIElementText::Generate>()
                    .MoveDown()
                    .AddElement<UIElementImage>("ui_hotbar_background")
                        .CallAndThen<&Component::GetGameObject>([&](std::shared_ptr<GameObject> gameObject)
                            {
                                gameObject->GetTransform2d()->SetPosition({ 0.0f, -10.0f });
                                gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::BOTTOM | Transform2d::Anchor::CENTER_X);
                            })
                        .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.hotbar_background").value())
                        .Call<&UIElementImage::Generate>()
                    .MoveDown()
                    .AddElement<UIElementImage>("ui_hotbar_selector")
                        .CallAndThen<&Component::GetGameObject>([&](std::shared_ptr<GameObject> gameObject)
                            {
                                gameObject->GetTransform2d()->SetPosition({ 0.0f, -20.0f });
                                gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::BOTTOM | Transform2d::Anchor::CENTER_X);
                            })
                        .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.hotbar_selector").value())
                        .Call<&UIElementImage::Generate>()
                    .MoveDown()
                    .AddLayout<UILayoutGrid>("ui_hotbar_grid", Vector<float, 2>{ 205, 205 }, Vector<float, 2>{ 20, 20 }, Vector<float, 2>{ 20, 20 }, 5)
                        .CallAndThen<&Component::GetGameObject>([&](std::shared_ptr<GameObject> gameObject)
                            {
                                gameObject->GetTransform2d()->SetPosition({ 0.0f, -10.0f });
                                gameObject->GetTransform2d()->SetDimensions({ 1280, 256 });
                                gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::BOTTOM | Transform2d::Anchor::CENTER_X);
                            })
                        .AddElement<UIElementImage>("ui_slot_0")
                            .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.slot_background").value())
                            .Call<&UIElementImage::Generate>()
                            .AddElement<UIElementImage>("ui_slot_image")
                                .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.item.resource_empty").value())
                                .Call<&UIElementImage::Generate>()
                            .MoveDown()
                        .MoveDown()
                        .AddElement<UIElementImage>("ui_slot_1")
                            .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.slot_background").value())
                            .Call<&UIElementImage::Generate>()
                            .AddElement<UIElementImage>("ui_slot_image")
                                .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.item.resource_empty").value())
                                .Call<&UIElementImage::Generate>()
                            .MoveDown()
                        .MoveDown()
                        .AddElement<UIElementImage>("ui_slot_2")
                            .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.slot_background").value())
                            .Call<&UIElementImage::Generate>()
                            .AddElement<UIElementImage>("ui_slot_image")
                                .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.item.resource_empty").value())
                                .Call<&UIElementImage::Generate>()
                            .MoveDown()
                        .MoveDown()
                        .AddElement<UIElementImage>("ui_slot_3")
                            .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.slot_background").value())
                            .Call<&UIElementImage::Generate>()
                            .AddElement<UIElementImage>("ui_slot_image")
                                .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.item.resource_empty").value())
                                .Call<&UIElementImage::Generate>()
                            .MoveDown()
                        .MoveDown()
                        .AddElement<UIElementImage>("ui_slot_4")
                            .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.slot_background").value())
                            .Call<&UIElementImage::Generate>()
                            .AddElement<UIElementImage>("ui_slot_image")
                                .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.item.resource_empty").value())
                                .Call<&UIElementImage::Generate>()
                            .MoveDown()
                        .MoveDown()
                    .MoveDown()
                .Finish();

            healthText = GameObjectManager::GetInstance().Get(hudRoot->GetAbsolutePath() + ".ui_health_text").value()->GetComponent<UIElementText>().value();
            hotbarSelector = GameObjectManager::GetInstance().Get(hudRoot->GetAbsolutePath() + ".ui_hotbar_selector").value();

            const auto& slotContainer = GameObjectManager::GetInstance().Get(hudRoot->GetAbsolutePath() + ".ui_hotbar_grid").value();

            for (const auto& child : slotContainer->GetChildMap() | std::views::values)
                hotbarSlotImageList.push_back(child->GetChildMap().at("ui_slot_image")->GetComponent<UIElementImage>().value());
#endif
        }

        void Update() override
        {
            if (!GetGameObject()->IsLocallyControlled())
                return;

            modelGameObject->SetLocallyActive(false);

            GameObjectManager::GetInstance().Get(GetGameObject()->GetAbsolutePath() + ".model").value()->GetTransform3d()->SetLocalRotation({ 90.0f, camera->GetGameObject()->GetTransform3d()->GetLocalRotation().y(), 0.0f });

            UpdateControls();
            UpdateMouselook();
            UpdateMovement();

#ifndef IS_SERVER
            if (float(std::atoi(healthText->GetText().substr(17).data())) != GetCurrentHealth())
            {
                healthText->SetText("Current Health: " + std::to_string(int(GetCurrentHealth())));
                healthText->Generate();
            }

            for (int i = 0; i < 5; ++i)
            {
                if (hotbarSlotImageList[i]->GetTexture().value()->GetName() != ItemRegistry::GetInstance().Get(hotbar.slots[i]).value()->GetTextureName())
                {
                    hotbarSlotImageList[i]->SetTexture(TextureManager::GetInstance().Get(ItemRegistry::GetInstance().Get(hotbar.slots[i]).value()->GetTextureName()).value());
                    hotbarSlotImageList[i]->Generate();
                }
            }
#endif
        }

        std::string GetRegistryName() const override
        {
            return "entity_player";
        }

        float GetCurrentHealth() const override
        {
            return currentHealth;
        }

        float GetMaximumHealth() const override
        {
            return 100.0f;
        }

        float GetMovementSpeed() const override
        {
            return 30.0f;
        }

        float GetRunningMultiplier() const override
        {
            return 1.2f;
        }

        float GetJumpHeight() const override
        {
            return 5.0f;
        }

        bool GetCanJump() const override
        {
            return true;
        }

        static std::shared_ptr<EntityPlayer> Create(const Team& team)
        {
            std::shared_ptr<EntityPlayer> result(new EntityPlayer());

            result->team = team;

            return result;
        }

    private:

        EntityPlayer() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);


            archive & BOOST_SERIALIZATION_NVP(currentHealth);
            archive & BOOST_SERIALIZATION_NVP(team);
            archive & BOOST_SERIALIZATION_NVP(hotbar);
        }

        void UpdateControls()
        {
            if (InputManager::GetInstance().GetKeyState(KeyCode::C, KeyState::PRESSED))
                std::cout << "Current Position: " << GetGameObject()->GetTransform3d()->GetWorldPosition() << std::endl;

            if (InputManager::GetInstance().GetKeyState(KeyCode::V, KeyState::PRESSED))
                hotbar.slots[3] = ItemRegistry::GetInstance().Get(std::string("item_assault_rifle")).value()->GetId();

            if (InputManager::GetInstance().GetScrollDelta() > 0)
                hotbar.index += 1;

            if (InputManager::GetInstance().GetScrollDelta() < 0)
                hotbar.index -= 1;

            if (hotbar.index < 1)
                hotbar.index = 5;

            if (hotbar.index > 5)
                hotbar.index = 1;

#ifndef IS_SERVER
            hotbarSelector->GetTransform2d()->SetPosition({ ((int)hotbar.index - 3) * (float)225, -20 });
#endif
        }

        void UpdateMouselook() const
        {
            if (InputManager::GetInstance().GetKeyState(KeyCode::ESCAPE, KeyState::PRESSED))
                InputManager::GetInstance().SetMouseMode(!InputManager::GetInstance().GetMouseMode());

            Vector<float, 2> mouseDelta = InputManager::GetInstance().GetMouseDelta();

            const auto transform = camera->GetGameObject()->GetTransform3d();
            Vector<float, 3> rotation = transform->GetLocalRotation();

            rotation.y() -= mouseDelta.x() * mouseSensitivity;
            rotation.x() += mouseDelta.y() * mouseSensitivity;

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

#ifndef IS_SERVER
        std::shared_ptr<GameObject> hudRoot = nullptr;
        std::shared_ptr<UIElementText> healthText = nullptr;

        std::shared_ptr<GameObject> hotbarSelector = nullptr;
        
        std::vector<std::shared_ptr<UIElementImage>> hotbarSlotImageList;
#endif

        Hotbar hotbar;

        float currentHealth;

        constexpr static float blendTime = 0.20f;

        constexpr static float mouseSensitivity = 0.1f;

        DESCRIBE_AND_REGISTER(EntityPlayer, (EntityBase<EntityPlayer>), (), (), (team, hotbar, currentHealth))

    };
}

REGISTER_COMPONENT(Blaster::Server::Entity::Entities::EntityPlayer, 57854)