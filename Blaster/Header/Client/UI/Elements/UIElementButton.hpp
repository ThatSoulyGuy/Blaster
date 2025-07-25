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

namespace Blaster::Client::UI
{
    class UIElementButton : public UIElement
    {

    public:

        using Callback = std::function<void()>;

        void SetOnClick(Callback cb)
        {
            onClick = std::move(cb);
        }

        void SetOnHover(Callback cb)
        {
            onHover = std::move(cb);
        }

        void Update() override
        {
            UIElement::Update();

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
            
            if (inside && InputManager::GetInstance().GetMouseButtonDown(Input::MouseButton::LEFT))
            {
                if (onClick)
                    onClick();
            }
        }

        bool IsHovered() const noexcept
        {
            return hovered;
        }

    private:

        bool hovered = false;

        Callback onClick;
        Callback onHover;

        friend class boost::serialization::access;

        template<typename Archive> void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);
        }

        DESCRIBE_AND_REGISTER(UIElementButton, (UIElement), (), (), ())
    };
}

REGISTER_COMPONENT(Blaster::Client::UI::UIElementImage, 21712)