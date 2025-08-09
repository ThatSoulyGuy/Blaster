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
    class UIElementButton : public UIElement
    {

    public:

        UIElementButton(const UIElementButton&) = delete;
        UIElementButton(UIElementButton&&) = delete;
        UIElementButton& operator=(const UIElementButton&) = delete;
        UIElementButton& operator=(UIElementButton&&) = delete;

        void SetOnClick(std::function<void()>&& callback)
        {
            onClick = std::move(callback);
        }

        void SetOnHover(std::function<void()>&& callback)
        {
            onHover = std::move(callback);
        }

        void RenderUI() override
        {
            const auto [min, max] = GetGameObject()->GetTransform2d()->GetWorldRect();

            const Vector<float, 2> mouse = { float(InputManager::GetInstance().GetMousePosition().x()), float(InputManager::GetInstance().GetMousePosition().y()) };

            const bool inside = (mouse.x() >= min.x() && mouse.x() <= max.x() && mouse.y() >= min.y() && mouse.y() <= max.y());

            if (inside && !hovered)
            {
                hovered = true;

                if (onHover)
                    onHover();
            }
            else if (!inside && hovered)
                hovered = false;

            if (inside && InputManager::GetInstance().GetMouseState(MouseCode::LEFT, MouseState::PRESSED))
            {
                if (onClick)
                    onClick();
            }
        }

        bool IsHovered() const noexcept
        {
            return hovered;
        }

        std::optional<std::shared_ptr<Shader>> GetShader() const override
        {
            return std::nullopt;
        }

        void Generate() override { }

        static std::shared_ptr<UIElementButton> Create()
        {
            return std::shared_ptr<UIElementButton>(new UIElementButton());
        }

    private:

        UIElementButton() = default;

        friend class Blaster::Independent::ECS::ComponentFactory;
        friend class boost::serialization::access;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);
        }

        bool hovered = false;

        std::function<void()> onClick;
        std::function<void()> onHover;

        DESCRIBE_AND_REGISTER(UIElementButton, (UIElement), (), (), ())
    };
}

REGISTER_COMPONENT(Blaster::Client::UI::Elements::UIElementButton, 21792)