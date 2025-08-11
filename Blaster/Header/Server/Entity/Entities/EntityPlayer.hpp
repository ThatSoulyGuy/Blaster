#pragma once

#include "Client/Core/InputManager.hpp"
#include "Client/Network/ClientNetwork.hpp"
#include "Client/Render/Vertices/FatVertex.hpp"
#include "Client/Render/Camera.hpp"
#include "Client/Render/Model.hpp"
#include "Client/Render/ShaderManager.hpp"
#include "Client/Render/TextureManager.hpp"
#include "Client/Sound/SoundClip.hpp"
#include "Client/UI/Elements/UIElementText.hpp"
#include "Client/UI/Elements/UIElementImage.hpp"
#include "Client/UI/Layouts/UILayoutGrid.hpp"
#include "Client/UI/UIBuilder.hpp"
#include "Independent/ECS/GameObject.hpp"
#include "Independent/Item/ItemRegistry.hpp"
#include "Independent/Utility/Time.hpp"
#include "Independent/Thread/MainThreadExecutor.hpp"
#include "Independent/Physics/CharacterController.hpp"
#include "Independent/Physics/Raycast.hpp"
#include "Independent/ComponentRegistry.hpp"
#include "Server/Entity/Entities/EntityCommands.hpp"
#include "Server/Entity/LivingEntity.hpp"

using namespace std::chrono_literals;
using namespace Blaster::Client::Network;
using namespace Blaster::Client::Render::Vertices;
using namespace Blaster::Client::Render;
using namespace Blaster::Client::Sound;
using namespace Blaster::Client::UI::Elements;
using namespace Blaster::Client::UI::Layouts;
using namespace Blaster::Client::UI;
using namespace Blaster::Independent::Physics::Colliders;
using namespace Blaster::Independent::Physics;
using namespace Blaster::Independent::Thread;
using namespace Blaster::Independent::Item;

namespace Blaster::Server::Entity::Entities
{
    class EntityPlayer final : public LivingEntity
    {

    public:

        struct Hotbar
        {
            std::uint8_t index{ 1 };
            std::array<std::uint32_t, 5> slots{};
            
            std::uint32_t& GetCurrentSlot()
            {
                return slots[(index ? index : 1) - 1];
            }

            const std::uint32_t GetCurrentSlot() const
            {
                return slots[(index ? index : 1) - 1];
            }

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
                const auto cameraGameObject = GameObjectManager::GetInstance().Register(GameObject::Create("camera", true), GetGameObject()->GetAbsolutePath());

                cameraGameObject->GetTransform3d()->SetLocalPosition({ 0.0f, 10.0f, 0.0f });
                camera = cameraGameObject->AddComponent(Camera::Create(45.0f, 0.01f, 10000.0f));

                GameObjectManager::GetInstance().SetCamera(camera);
                
                modelGameObject = GameObjectManager::GetInstance().Register(GameObject::Create("model"), GetGameObject()->GetAbsolutePath());

                modelGameObject->GetTransform3d()->SetLocalPosition({ 0.0f, -2.5f, 0.0f });
                modelGameObject->GetTransform3d()->SetLocalRotation({ 90.0f, 0.0f, 0.0f });
                modelGameObject->GetTransform3d()->SetLocalScale({ 0.00025f, 0.00025f, 0.00025f });

#ifndef IS_SERVER
                hurtSoundObject = GameObjectManager::GetInstance().Register(GameObject::Create("hurt_sound"), GetGameObject()->GetAbsolutePath());

                hurtSoundObject->AddComponent(SoundClip::Create({ "Blaster", "Sound/PlayerPain.wav" }));

                stepSoundObject = GameObjectManager::GetInstance().Register(GameObject::Create("step_sound"), GetGameObject()->GetAbsolutePath());

                stepSoundObject->AddComponent(SoundClip::Create({ "Blaster", "Sound/Step.wav" }));
#endif

                if (team == Team::RED)
                    modelGameObject->AddComponent(Model::Create({ "Blaster", "Model/MTF2_Red.fbx" }, true));
                else
                    modelGameObject->AddComponent(Model::Create({ "Blaster", "Model/MTF2_Blue.fbx" }, true));

                InputManager::GetInstance().SetMouseMode(MouseMode::LOCKED);

#ifndef IS_SERVER
                itemSoundObject = GameObjectManager::GetInstance().Register(GameObject::Create("item_sound"), cameraGameObject->GetAbsolutePath());

				itemSoundObject->GetTransform3d()->SetLocalPosition({ 0.0f, 0.0f, 5.0f });

                hudRoot = UIBuilder::NewMenu("ui_hud_" + Blaster::Client::Network::ClientNetwork::GetInstance().GetStringId())
                        .AddElement<UIElementImage>("ui_crosshair")
                            .CallAndThen<&Component::GetGameObject>([&](std::shared_ptr<GameObject> gameObject)
                                {
                                    gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::CENTER_Y | Transform2d::Anchor::CENTER_X);
                                })
                            .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.crosshair").value())
                            .Call<&UIElementImage::Generate>()
                        .MoveDown()
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

                pauseMenuRoot = UIBuilder::NewMenu("ui_pause_" + Blaster::Client::Network::ClientNetwork::GetInstance().GetStringId())
                        .AddElement<UIElementImage>()
                            .CallAndThen<&Component::GetGameObject>([&](std::shared_ptr<GameObject> gameObject)
                                {
                                    gameObject->GetTransform2d()->SetStretch(Transform2d::Stretch::TOP | Transform2d::Stretch::BOTTOM | Transform2d::Stretch::RIGHT | Transform2d::Stretch::LEFT);
                                })
                            .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.menu_background").value())
                            .Call<&UIElementImage::Generate>()
                        .MoveDown()
                    .Finish();

                pauseMenuRoot->SetLocallyActive(false);

                deathMenuRoot = UIBuilder::NewMenu("ui_death_" + Blaster::Client::Network::ClientNetwork::GetInstance().GetStringId())
                        .AddElement<UIElementImage>("ui_background")
                            .CallAndThen<&Component::GetGameObject>([&](std::shared_ptr<GameObject> gameObject)
                            {
                                gameObject->GetTransform2d()->SetStretch(Transform2d::Stretch::TOP | Transform2d::Stretch::BOTTOM | Transform2d::Stretch::RIGHT | Transform2d::Stretch::LEFT);
                            })
                            .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.death_background").value())
                            .Call<&UIElementImage::Generate>()
                        .MoveDown()
                        .AddElement<UIElementText>("ui_death_text")
                            .CallAndThen<&Component::GetGameObject>([&](std::shared_ptr<GameObject> gameObject)
                            {
                                gameObject->GetTransform2d()->SetPosition({ 0.0f, 40.0f });
                                gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::TOP | Transform2d::Anchor::CENTER_X);
                            })
                            .Call<&UIElementText::SetFont>(UIElementText::Font::Create({ "Blaster", "Font/DS-DIGIB.TTF" }, 88, 0, 8))
                            .Call<&UIElementText::SetText>("YOU DIED!")
                            .Call<&UIElementText::Generate>()
                        .MoveDown()
                        .AddLayout<UILayoutStack>("ui_button_layout", UILayoutStack::Orientation::VERTICAL, Vector<float, 2>{ 768.0f, 96.0f }, Vector<float, 2>{ 4.0f, 4.0f }, 10.0f)
                            .CallAndThen<&Component::GetGameObject>([&](std::shared_ptr<GameObject> gameObject)
                            {
                                gameObject->GetTransform2d()->SetDimensions({ 768.0f, 288.0f });
                            })
                            .AddElement<UIElementButton>("ui_respawn_button")
                                .Call<&UIElementButton::SetOnClick>([this, absolutePath = GetGameObject()->GetAbsolutePath()]
                                {
                                    Blaster::Client::Network::ClientNetwork::GetInstance().Send(PacketType::C2S_EntityPlayer_Respawn, RespawnCommand{ absolutePath });

                                    deathMenuRoot->SetLocallyActive(false);
                                })
                                .AddElement<UIElementImage>("ui_image")
                                    .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.button_default").value())
                                    .Call<&UIElementImage::Generate>()
                                .MoveDown()
                                .AddElement<UIElementText>("ui_text")
                                    .Call<&UIElementText::SetFont>(UIElementText::Font::Create({ "Blaster", "Font/DS-DIGIB.TTF" }, 48, 0, 8))
                                    .Call<&UIElementText::SetText>("RESPAWN")
                                    .Call<&UIElementText::Generate>()
                                .MoveDown()
                            .MoveDown()
                            .AddElement<UIElementButton>("ui_quit_button")
                                .Call<&UIElementButton::SetOnClick>([]
                                {
                                    std::terminate();
                                })
                                .AddElement<UIElementImage>("ui_image")
                                    .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.button_default").value())
                                    .Call<&UIElementImage::Generate>()
                                .MoveDown()
                                .AddElement<UIElementText>("ui_text")
                                    .Call<&UIElementText::SetFont>(UIElementText::Font::Create({ "Blaster", "Font/DS-DIGIB.TTF" }, 48, 0, 8))
                                    .Call<&UIElementText::SetText>("RAGE QUIT")
                                    .Call<&UIElementText::Generate>()
                                .MoveDown()
                            .MoveDown()
                        .MoveDown()
                    .Finish();

                deathMenuRoot->SetLocallyActive(false);
#endif
            }
        }

        void Update() override
        {
            if (!GetGameObject()->IsLocallyControlled())
                return;

#ifndef IS_SERVER
            if (!GameObjectManager::GetInstance().Has(std::string(team == Team::RED ? "red" : "blue") + "_beacon") && GameObjectManager::GetInstance().Has(deathMenuRoot->GetAbsolutePath() + ".ui_button_layout.ui_respawn_button"))
                GameObjectManager::GetInstance().Unregister(deathMenuRoot->GetAbsolutePath() + ".ui_button_layout.ui_respawn_button");
#endif
                
            if (currentHealth <= 0)
            {
#ifndef IS_SERVER
                deathMenuRoot->SetLocallyActive(true);
#endif
                auto animator = modelGameObject->GetComponent<Animator>().value();

                if (!hasPlayedDeath)
                {
                    animator->Play("mtf2.death", 0.2f, 1.0f, WrapMode::ONCE);

                    hasPlayedDeath = true;
                }
            }
            
            modelGameObject->SetLocallyActive(false);

            GameObjectManager::GetInstance().Get(GetGameObject()->GetAbsolutePath() + ".model").value()->GetTransform3d()->SetLocalRotation({ 90.0f, camera->GetGameObject()->GetTransform3d()->GetLocalRotation().y(), 0.0f });
            
            UpdateControls();
            UpdateMouselook();
            UpdateMovement();
            UpdateViewModel();
        }

        std::string GetRegistryName() const override
        {
            return "entity_player";
        }

        Team GetTeam() const override
        {
            return team;
        }

        std::uint8_t GetCurrentHealth() const override
        {
            return currentHealth;
        }

        std::uint8_t GetMaximumHealth() const override
        {
            return 100.0f;
        }

        void SetCurrentHealth(std::uint8_t currentHealth)
        {
            this->currentHealth = currentHealth;

            Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(GetGameObject(), typeid(EntityPlayer));
        }

        void DealDamage(std::uint8_t damage) override
        {
            if ((int(currentHealth) - damage) <= 0)
                currentHealth = 0;
            else
                currentHealth -= abs(damage);

            Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(GetGameObject(), typeid(EntityPlayer));
        }

        void HealDamage(std::uint8_t damage) override
        {
            currentHealth += abs(damage);

            Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(GetGameObject(), typeid(EntityPlayer));
        }

        std::optional<std::shared_ptr<GameObject>> GetEntityModel() const override
        {
            return modelGameObject;
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
            archive& boost::serialization::base_object<Component>(*this);

#ifndef IS_SERVER
            const std::uint8_t before = currentHealth;
#endif

            archive & BOOST_SERIALIZATION_NVP(currentHealth);
            archive & BOOST_SERIALIZATION_NVP(team);
            archive & BOOST_SERIALIZATION_NVP(hotbar);

#ifndef IS_SERVER
            if constexpr (Archive::is_loading::value)
            {
                if (hurtSoundObject && currentHealth < before)
                {
                    auto soundClip = hurtSoundObject->GetComponent<SoundClip>().value();

                    soundClip->SetSpatial(false);
                    soundClip->Play();
                }
            }
#endif
        }


        void UpdateControls()
        {
#ifndef IS_SERVER
            if (InputManager::GetInstance().GetKeyState(KeyCode::ESCAPE, KeyState::PRESSED))
                pauseMenuRoot->SetLocallyActive(!pauseMenuRoot->IsLocallyActive());

            if (IsMenuActive())
            {
                InputManager::GetInstance().SetMouseMode(MouseMode::FREE);

                return;
            }
            else
                InputManager::GetInstance().SetMouseMode(MouseMode::LOCKED);
#endif

            if (InputManager::GetInstance().GetKeyState(KeyCode::C, KeyState::PRESSED))
                std::cout << "Current Position: " << GetGameObject()->GetTransform3d()->GetWorldPosition() << std::endl;

            if (InputManager::GetInstance().GetKeyState(KeyCode::V, KeyState::PRESSED))
                hotbar.GetCurrentSlot() = ItemRegistry::GetInstance().Get(std::string("item_assault_rifle")).value()->GetId();

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
            
            if (InputManager::GetInstance().GetKeyState(KeyCode::X, KeyState::PRESSED))
                PhysicsWorld::GetInstance().ToggleDrawDebug();
#endif

            if (InputManager::GetInstance().GetMouseState(MouseCode::LEFT, MouseState::PRESSED))
            {
                auto item = ItemRegistry::GetInstance().Get(hotbar.GetCurrentSlot()).value();

#ifndef IS_SERVER
                if (item->GetSoundPath())
                {
                    if (itemSoundObject->HasComponent<SoundClip>())
                    {
                        itemSoundObject->GetComponent<SoundClip>().value()->GetPath() == item->GetSoundPath().value();
                        itemSoundObject->GetComponent<SoundClip>().value()->Play();
                    }
                    else
                    {
                        itemSoundObject->RemoveComponent<SoundClip>();

                        itemSoundObject->AddComponent(SoundClip::Create(item->GetSoundPath().value(), false, true, true));
                        itemSoundObject->GetComponent<SoundClip>().value()->Play();
                    }
                }
#endif

                constexpr float kRayDistance = 100.f;

                const auto cameraGameObject = camera->GetGameObject();
                const auto origin = cameraGameObject->GetTransform3d()->GetWorldPosition();
                const auto direction = cameraGameObject->GetTransform3d()->GetForward();

                auto selfControlOptional = GetGameObject()->GetComponent<CharacterController>();
                auto* self = dynamic_cast<PhysicsBody*>(selfControlOptional ? selfControlOptional->get() : nullptr);

                const btCollisionObject* selfObj = self ? self->GetCollisionObject() : nullptr;

                auto hit = Raycast::Fire(origin, direction, kRayDistance, selfObj);

                if (!hit.success)
                    return;

                auto* other = static_cast<PhysicsBody*>(hit.object->getUserPointer());

                if (self != other && other->GetGameObject()->HasComponent<EntityBase>())
                    item->OnUsed(this, other->GetGameObject()->GetComponent<EntityBase>()->get(), MouseCode::LEFT);
            }
        }

        void UpdateMouselook() const
        {
#ifndef IS_SERVER
            if (IsMenuActive())
                return;
#endif

            Vector<float, 2> mouseDelta = InputManager::GetInstance().GetMouseDelta();

            const auto transform = camera->GetGameObject()->GetTransform3d();
            Vector<float, 3> rotation = transform->GetLocalRotation();

            rotation.y() -= mouseDelta.x() * mouseSensitivity;
            rotation.x() += mouseDelta.y() * mouseSensitivity;

            transform->SetLocalRotation(rotation);
        }

        void UpdateMovement()
        {
#ifndef IS_SERVER
            if (IsMenuActive())
                return;
#endif

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

            if (InputManager::GetInstance().GetKeyState(KeyCode::W, KeyState::HELD)) direction += forward;
            if (InputManager::GetInstance().GetKeyState(KeyCode::S, KeyState::HELD)) direction -= forward;
            if (InputManager::GetInstance().GetKeyState(KeyCode::D, KeyState::HELD)) direction += right;
            if (InputManager::GetInstance().GetKeyState(KeyCode::A, KeyState::HELD)) direction -= right;

            const bool hasMoveInput = Vector<float, 3>::LengthSquared(direction) > epsilon;

            if (hasMoveInput)
            {
                direction = Vector<float, 3>::Normalize(direction);
                controller->SetWalkDirection(direction * GetMovementSpeed());
            }
            else
                controller->SetWalkDirection({ 0.0f, 0.0f, 0.0f });

#ifndef IS_SERVER
            {
                const float deltaTime = Blaster::Independent::Utility::Time::GetInstance().GetDeltaTime();

                if (hasMoveInput && controller->OnGround())
                {
                    stepTimer -= deltaTime;

                    if (stepTimer <= 0.f && stepSoundObject)
                    {
                        if (auto soundClip = stepSoundObject->GetComponent<SoundClip>())
                        {
                            soundClip.value()->Stop();
                            soundClip.value()->Play();
                        }

                        stepTimer = 0.45f;
                    }
                }
                else
                    stepTimer = std::min(stepTimer, 0.1f);
            }
#endif

            if (hasMoveInput)
            {
                if (currentViewModelItem == 0)
                {
                    if (!animator->IsPlaying("mtf2.walk"))
                        animator->Play("mtf2.walk", blendTime, 1.8f * Vector<float, 3>::LengthSquared(direction));
                }
                else
                {
                    if (!animator->IsPlaying("mtf2.walk_hold"))
                        animator->Play("mtf2.walk_hold", blendTime, 1.8f * Vector<float, 3>::LengthSquared(direction));
                }
            }
            else
            {
                if (currentViewModelItem == 0)
                {
                    if (!animator->IsPlaying("mtf2.idle"))
                        animator->Play("mtf2.idle", blendTime);
                }
                else
                {
                    if (!animator->IsPlaying("mtf2.idle_hold"))
                        animator->Play("mtf2.idle_hold", blendTime);
                }
            }

            if (InputManager::GetInstance().GetKeyState(KeyCode::SPACE, KeyState::PRESSED) && controller->OnGround())
                controller->Jump();
        }


        void UpdateViewModel()
        {
#ifndef IS_SERVER
            PresentHealthIfChanged();
            PresentHotbarIfChanged();
            PresentViewModelIfChanged();
#endif
        }

#ifndef IS_SERVER
        void PresentHealthIfChanged()
        {
            if (lastPresentedHealth == currentHealth)
                return;

            const bool shouldPlayHurt = (lastPresentedHealth != 255) && (currentHealth < lastPresentedHealth);

            lastPresentedHealth = currentHealth;

            healthText->SetText("Current Health: " + std::to_string(currentHealth));
            healthText->Generate();

            if (shouldPlayHurt && hurtSoundObject)
            {
                if (auto clip = hurtSoundObject->GetComponent<SoundClip>())
                {
                    clip.value()->SetSpatial(false);
                    clip.value()->Play();
                }
            }
        }

        void PresentHotbarIfChanged()
        {
            for (std::size_t i = 0; i < hotbar.slots.size(); ++i)
            {
                const uint32_t itemId = hotbar.slots[i];

                if (itemId == lastSlotIds[i])
                    continue;

                lastSlotIds[i] = itemId;

                const auto item = ItemRegistry::GetInstance().Get(itemId).value();
                const auto tex = TextureManager::GetInstance().Get(item->GetTextureName()).value();

                auto slotImage = hotbarSlotImageList[i];

                slotImage->SetTexture(tex);
                slotImage->Generate();
            }
        }

        void PresentViewModelIfChanged()
        {
            const uint32_t wanted = hotbar.GetCurrentSlot();
            
            const std::string viewModelPath = camera->GetGameObject()->GetAbsolutePath() + ".view_model";
            const std::string worldModelPath = GetGameObject()->GetAbsolutePath() + ".world_model";

            std::shared_ptr<GameObject> viewModelObject = GameObjectManager::GetInstance().Has(viewModelPath) ? GameObjectManager::GetInstance().Get(viewModelPath).value() : nullptr;
            std::shared_ptr<GameObject> worldModelObject = GameObjectManager::GetInstance().Has(worldModelPath) ? GameObjectManager::GetInstance().Get(worldModelPath).value() : nullptr;

            if (worldModelObject)
                worldModelObject->GetTransform3d()->SetLocalRotation({ 0.0f, modelGameObject->GetTransform3d()->GetLocalRotation().y(), 0.0f});

            if (wanted != currentViewModelItem)
            {
                if (currentViewModelItem != 0)
                {
                    if (GameObjectManager::GetInstance().Has(viewModelPath))
                        GameObjectManager::GetInstance().Unregister(viewModelPath);

                    if (GameObjectManager::GetInstance().Has(worldModelPath))
                        GameObjectManager::GetInstance().Unregister(worldModelPath);
                }

                currentViewModelItem = wanted;

                if (wanted == 0)
                    return;

                auto item = ItemRegistry::GetInstance().Get(wanted).value();

                if (!item->GetModelPath())
                    return;
                
                viewModelObject = GameObjectManager::GetInstance().Register(GameObject::Create("view_model", true), camera->GetGameObject()->GetAbsolutePath());

                viewModelBasePosition = item->GetModelViewPosition().has_value() ? item->GetModelViewPosition().value() : Vector<float, 3>{ 0.0f, 0.0f, 0.0f };

                viewModelObject->GetTransform3d()->SetLocalPosition(viewModelBasePosition);
                viewModelObject->GetTransform3d()->SetLocalRotation(item->GetModelViewRotation().has_value() ? item->GetModelViewRotation().value() : Vector<float, 3>{ 0.0f, 0.0f, 0.0f });
                viewModelObject->AddComponent(Model::Create(item->GetModelPath().value()));

                worldModelObject = GameObjectManager::GetInstance().Register(GameObject::Create("world_model"), GetGameObject()->GetAbsolutePath());

                worldModelObject->GetTransform3d()->SetLocalPosition({ 1.0f, 9.0f, 3.5f });
                worldModelObject->GetTransform3d()->SetLocalScale({ 1.0f, 1.0f, 1.0f });
                worldModelObject->GetTransform3d()->SetLocalPivot(modelGameObject->GetTransform3d()->GetLocalPosition());

                worldModelObject->AddComponent(Model::Create(item->GetModelPath().value()));

                worldModelObject->SetLocallyActive(false);
                
                for (auto child : viewModelObject->GetChildMap() | std::views::values)
                {
                    if (child->HasComponent<Mesh<ModelVertex>>())
                        child->GetComponent<Mesh<ModelVertex>>().value()->SetRenderInFront(true);
                }
            }

            if (!viewModelObject)
                return;

            const float deltaTime = Time::GetInstance().GetDeltaTime();

            constexpr float kSwayFactor = 0.068f;
            constexpr float kSwaySmooth = 9.0f;

            constexpr float kBreathAmp = 0.015f;
            constexpr float kBreathFreq = 1.2f;

            constexpr float kWalkAmp = 0.125f;
            constexpr float kWalkFreq = 1.7f;
            constexpr float kFreqClamp = 0.2f;

            constexpr float kBobSmooth = 7.5f;

            const Vector<float, 3> camEuler = camera->GetGameObject()->GetTransform3d()->GetLocalRotation();

            Vector<float, 2> cameraAngles{ camEuler.x(), camEuler.y() };
            Vector<float, 2> cameraAnglesDelta = cameraAngles - lastCameraAngles;

            auto wrap = [](float a)
                {
                    a = std::fmod(a + 180.f, 360.f);

                    if (a < 0.f)
                        a += 360.f;

                    return a - 180.f;
                };

            cameraAnglesDelta.x() = wrap(cameraAnglesDelta.x());
            cameraAnglesDelta.y() = wrap(cameraAnglesDelta.y());

            lastCameraAngles = cameraAngles;

            Vector<float, 3> targetSway = { -cameraAnglesDelta.y() * kSwayFactor, cameraAnglesDelta.x() * kSwayFactor, 0.f };

            swayOffset = Vector<float, 3>::Lerp(swayOffset, targetSway, std::clamp(kSwaySmooth * deltaTime, 0.f, 1.f));

            auto controller = GetGameObject()->GetComponent<CharacterController>().value();
            const float speed = Vector<float, 3>::Magnitude(controller->GetWalkDirection());

            const bool moving = speed > 0.1f;
            const float amp = moving ? kWalkAmp : kBreathAmp;
            const float baseFreq = moving ? kWalkFreq : kBreathFreq;
            const float freq = std::max(baseFreq * (moving ? speed : 1.f), kFreqClamp);

            bobTimer += deltaTime;
            
            const float phase = bobTimer * freq * 2.f * std::numbers::pi_v<float>;

            Vector<float, 3> targetBob = { std::sin(phase + std::numbers::pi_v<float> / 2.f) * amp * 0.25f, std::sin(phase) * amp, std::cos(phase) * amp * 0.6f };

            bobOffset = Vector<float, 3>::Lerp(bobOffset, targetBob, std::clamp(kBobSmooth * deltaTime, 0.f, 1.f));

            viewModelObject->GetTransform3d()->SetLocalPosition(viewModelBasePosition + swayOffset + bobOffset);
        }

        bool IsMenuActive() const
        {
            return pauseMenuRoot->IsLocallyActive() || deathMenuRoot->IsLocallyActive();
        }
#endif

        std::shared_ptr<Camera> camera;
        std::shared_ptr<GameObject> modelGameObject;

        Team team;

#ifndef IS_SERVER
        float stepTimer = 0.f;

        std::shared_ptr<GameObject> pauseMenuRoot = nullptr;
        std::shared_ptr<GameObject> deathMenuRoot = nullptr;

        std::shared_ptr<GameObject> hudRoot = nullptr;
        std::shared_ptr<UIElementText> healthText = nullptr;

        std::shared_ptr<GameObject> hotbarSelector = nullptr;
        
        std::vector<std::shared_ptr<UIElementImage>> hotbarSlotImageList;

        std::shared_ptr<GameObject> itemSoundObject = nullptr;
        std::shared_ptr<GameObject> hurtSoundObject = nullptr;
        std::shared_ptr<GameObject> stepSoundObject = nullptr;
#endif

        Hotbar hotbar{};

        bool hasPlayedDeath = false;

        std::uint8_t currentHealth;

        std::uint8_t lastPresentedHealth = 255;
        std::array<std::uint32_t, 5> lastSlotIds{};
        std::uint32_t currentViewModelItem = 0;
        Vector<float, 3> viewModelBasePosition{ 0,0,0 };

        Vector<float, 2> lastCameraAngles{ 0,0 };
        Vector<float, 3> swayOffset{ 0,0,0 };

        float bobTimer = 0.f;
        Vector<float, 3> bobOffset{ 0,0,0 };

        constexpr static float blendTime = 0.20f;

        constexpr static float mouseSensitivity = 0.1f;

        DESCRIBE_AND_REGISTER(EntityPlayer, (EntityBase), (), (), (team, hotbar, currentHealth))

    };
}

REGISTER_COMPONENT(Blaster::Server::Entity::Entities::EntityPlayer, 57854)