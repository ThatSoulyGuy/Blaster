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

        void SetAutoSize(bool enabled)
        {
            autoSizeToTexture = enabled;
        }

        void SetUVRect(const Vector<float, 2>& minUV, const Vector<float, 2>& maxUV)
        {
            uvMin = { std::clamp(minUV.x(), 0.0f, 1.0f), std::clamp(minUV.y(), 0.0f, 1.0f) };
            uvMax = { std::clamp(maxUV.x(), 0.0f, 1.0f), std::clamp(maxUV.y(), 0.0f, 1.0f) };
        }

        void SetTint(const Vector<float, 3>& tint)
        {
            if (lastTint != tint)
            {
                this->tint = tint;

                Generate();

                lastTint = tint;
            }
        }

        void Generate() override
        {
            if (!GetGameObject()->HasComponent<Texture>())
                return;

            std::vector<UIVertex> vertices
            {
                { { 0.0f, 0.0f, 0.0f }, { tint.x(), tint.y(), tint.z() }, { uvMin.x(), uvMin.y() } },
                { { 1.0f, 0.0f, 0.0f }, { tint.x(), tint.y(), tint.z() }, { uvMax.x(), uvMin.y() } },
                { { 0.0f, 1.0f, 0.0f }, { tint.x(), tint.y(), tint.z() }, { uvMin.x(), uvMax.y() } },
                { { 1.0f, 1.0f, 0.0f }, { tint.x(), tint.y(), tint.z() }, { uvMax.x(), uvMax.y() } }
            };

            std::vector<uint32_t> indices{ 0, 1, 2, 1, 3, 2 };

            auto mesh = GetMesh();

            mesh->SetVertices(vertices);
            mesh->SetIndices(indices);
            mesh->Generate();

            if (autoSizeToTexture)
                GetGameObject()->GetTransform2d()->SetDimensions({ float(GetGameObject()->GetComponent<Texture>().value()->GetDimensions().x()), float(GetGameObject()->GetComponent<Texture>().value()->GetDimensions().y()) });
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

        Vector<float, 2> uvMin{ 0.0f, 0.0f };
        Vector<float, 2> uvMax{ 1.0f, 1.0f };
        Vector<float, 3> tint{ 1.0f, 1.0f, 1.0f };
        Vector<float, 3> lastTint{ 1.0f, 1.0f, 1.0f };

        bool autoSizeToTexture = true;

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