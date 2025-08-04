#pragma once

#include "Client/UI/UILayout.hpp"
#include "Independent/Math/Rect.hpp"

using namespace Blaster::Independent::Math;

namespace Blaster::Client::UI::Layouts
{
    class UILayoutStack final : public UILayout
    {

    public:

        enum class Orientation
        {
            Horizontal, Vertical
        };

        UILayoutStack(const UILayoutStack&) = delete;
        UILayoutStack(UILayoutStack&&) = delete;
        UILayoutStack& operator=(const UILayoutStack&) = delete;
        UILayoutStack& operator=(UILayoutStack&&) = delete;

        Vector<float, 2> GetMeasurement() const override
        {
            Vector<float, 2> result = { 0.0f, 0.0f };

            for (auto& child : GetGameObject()->GetChildMap() | std::views::values)
            {
                Vector<float, 2> childDimensions;

                if (auto childLayout = child->GetComponent<UILayout>(); childLayout)
                    childDimensions = childLayout.value()->GetMeasurement();
                else
                    childDimensions = child->GetTransform2d()->GetWorldRect().second - child->GetTransform2d()->GetWorldRect().first;

                if (orientation == Orientation::Vertical)
                {
                    result.x() = std::max(result.x(), childDimensions.x());
                    result.y() += childDimensions.y() + spacing;
                }
                else
                {
                    result.y() = std::max(result.y(), childDimensions.y());
                    result.x() += childDimensions.x() + spacing;
                }
            }

            if (orientation == Orientation::Vertical && result.y() > 0)
                result.y() -= spacing;

            if (orientation == Orientation::Horizontal && result.x() > 0)
                result.x() -= spacing;

            result += padding * 2.f;

            return result;
        }

        void Arrange(const Rect<float>& parentRect) override
        {
            const auto avail = parentRect.GetDimensions() - padding * 2.f;
            Vector<float, 2> cursor = parentRect.GetMin() + padding;


            for (auto& child : GetGameObject()->GetChildMap() | std::views::values)
            {
                Vector<float, 2> childDimensions;

                if (auto childLayout = child->GetComponent<UILayout>(); childLayout)
                    childDimensions = childLayout.value()->GetMeasurement();
                else
                    childDimensions = child->GetTransform2d()->GetWorldRect().second - child->GetTransform2d()->GetWorldRect().first;

                Rect<float> rect;

                if (orientation == Orientation::Vertical)
                {
                    rect = { cursor, cursor + Vector<float, 2>{ avail.x(), childDimensions.y() } };
                    cursor.y() += childDimensions.y() + spacing;
                }
                else
                {
                    rect = { cursor, cursor + Vector<float, 2>{ childDimensions.x(), avail.y() } };
                    cursor.x() += childDimensions.x() + spacing;
                }

                auto transform = child->GetTransform2d();

                transform->SetDimensions(rect.GetMin());

                if (auto childLayout = child->GetComponent<UILayout>(); childLayout)
                    childLayout.value()->Arrange(rect);
            }
        }

        static std::shared_ptr<UILayoutStack> Create(const Orientation& orientation, const Vector<float, 2>& padding, float spacing)
        {
            std::shared_ptr<UILayoutStack> result(new UILayoutStack());

            result->orientation = orientation;
            result->padding = padding;
            result->spacing = spacing;

            return result;
        }

    private:

        UILayoutStack() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);
        }

        Orientation orientation = Orientation::Vertical;
        Vector<float, 2> padding = { 4.0f, 4.0f };

        float spacing = 4.0f;

        DESCRIBE_AND_REGISTER(UILayoutStack, (UILayout), (), (), ())

    };
}

REGISTER_COMPONENT(Blaster::Client::UI::Layouts::UILayoutStack, 71629)