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
            HORIZONTAL,
            VERTICAL
        };

        UILayoutStack(const UILayoutStack&) = delete;
        UILayoutStack(UILayoutStack&&) = delete;
        UILayoutStack& operator=(const UILayoutStack&) = delete;
        UILayoutStack& operator=(UILayoutStack&&) = delete;

        Vector<float, 2> GetMeasurement() const override
        {
            const std::size_t childCount = GetGameObject()->GetChildMap().size();

            if (childCount == 0)
                return fixedSize + padding * 2.0f;

            Vector<float, 2> result{ 0.0f, 0.0f };

            if (orientation == Orientation::VERTICAL)
            {
                result.x() = fixedSize.x();
                result.y() = fixedSize.y() * float(childCount) + spacing * float(childCount - 1);
            }
            else
            {
                result.x() = fixedSize.x() * float(childCount) + spacing * float(childCount - 1);
                result.y() = fixedSize.y();
            }

            result += padding * 2.0f;

            return result;
        }

        void Arrange(const Rect<float>& parentRect) override
        {
            Vector<float, 2> cursor = parentRect.GetMin() + padding;

            for (auto& child : GetGameObject()->GetChildMap() | std::views::values)
            {
                Rect<float> rect{ cursor, cursor + fixedSize };

                auto transform = child->GetTransform2d();

                transform->SetPosition(rect.GetMin());
                transform->SetDimensions(fixedSize);
                transform->SetAnchors(Transform2d::Anchor::TOP | Transform2d::Anchor::LEFT);

                if (auto childLayout = child->GetComponent<UILayout>(); childLayout)
                    childLayout.value()->Arrange(rect);

                if (orientation == Orientation::VERTICAL)
                    cursor.y() += fixedSize.y() + spacing;
                else
                    cursor.x() += fixedSize.x() + spacing;
            }
        }

        static std::shared_ptr<UILayoutStack> Create(const Orientation& orientation, const Vector<float, 2>& fixedSize, const Vector<float, 2>& padding, float spacing)
        {
            std::shared_ptr<UILayoutStack> result(new UILayoutStack());

            result->orientation = orientation;
            result->fixedSize = fixedSize;
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

        Orientation orientation = Orientation::VERTICAL;
        Vector<float, 2> fixedSize = { 128.0f, 64.0f };
        Vector<float, 2> padding = { 4.0f, 4.0f };

        float spacing = 4.0f;

        DESCRIBE_AND_REGISTER(UILayoutStack, (UILayout), (), (), ())

    };
}

REGISTER_COMPONENT(Blaster::Client::UI::Layouts::UILayoutStack, 71629)