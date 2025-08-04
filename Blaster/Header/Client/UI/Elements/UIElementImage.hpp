#pragma once

#include <functional>
#include <string>
#include <array>
#include <utility>
#include <boost/serialization/string.hpp>
#include "Client/Core/InputManager.hpp"
#include "Client/Render/Shader.hpp"
#include "Client/Render/Texture.hpp"
#include "Client/UI/UIElement.hpp"

using namespace Blaster::Client::Render;

namespace Blaster::Client::UI::Elements
{
    class UIElementImage : public UIElement
    {

    public:

        UIElementImage(const UIElementImage&) = delete;
        UIElementImage(UIElementImage&&) = delete;
        UIElementImage& operator=(const UIElementImage&) = delete;
        UIElementImage& operator=(UIElementImage&&) = delete;

        void SetTexture(const std::shared_ptr<Texture>& texture)
        {
            if (GetGameObject()->HasComponent<Texture>())
                GetGameObject()->RemoveComponent<Texture>();

            GetGameObject()->AddComponent(texture);
        }

        std::optional<std::shared_ptr<Texture>> GetTexture() const
        {
            return GetGameObject()->GetComponent<Texture>();
        }

        void Generate() override
        {
            if (!GetGameObject()->HasComponent<Texture>())
                return;
            
            std::vector<UIVertex> vertices
            {
                { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } },
                { { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f } },
                { { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },
                { { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
            };

            std::vector<uint32_t> indices
            {
                0, 1, 2,
                1, 3, 2
            };

            const float width = float(GetGameObject()->GetComponent<Texture>().value()->GetDimensions().x());
            const float height = float(GetGameObject()->GetComponent<Texture>().value()->GetDimensions().y());

            auto mesh = GetMesh();

            mesh->SetVertices(vertices);
            mesh->SetIndices(indices);

            mesh->Generate();

            GetGameObject()->GetTransform2d()->SetDimensions({ width, height });
        }

        void RenderUI() override
        {
            if (auto mesh = GetMesh())
            {
                mesh->QueueShaderCall("uTexture", 0);

                mesh->QueueRenderCall([&]
                    {
                        GetGameObject()->GetComponent<Texture>().value()->Bind(0);
                    });
            }
        }

        std::optional<std::shared_ptr<Shader>> GetShader() const override
        {
            return ShaderManager::GetInstance().Get("blaster.textured_ui");
        }

        static std::shared_ptr<UIElementImage> Create()
        {
            return std::shared_ptr<UIElementImage>(new UIElementImage());
        }

    private:

        UIElementImage() = default;

        friend class Blaster::Independent::ECS::ComponentFactory;
        friend class boost::serialization::access;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);
        } 
        
        DESCRIBE_AND_REGISTER(UIElementImage, (UIElement), (), (), ())
    };
}

REGISTER_COMPONENT(Blaster::Client::UI::Elements::UIElementImage, 28712)