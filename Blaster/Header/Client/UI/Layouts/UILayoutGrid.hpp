#pragma once

#include "Client/UI/UILayout.hpp"
#include "Independent/Math/Rect.hpp"

using namespace Blaster::Independent::Math;

namespace Blaster::Client::UI::Layouts
{
    class UILayoutGrid final : public UILayout
    {

    public:

        UILayoutGrid(const UILayoutGrid&) = delete;
        UILayoutGrid(UILayoutGrid&&) = delete;
        UILayoutGrid& operator=(const UILayoutGrid&) = delete;
        UILayoutGrid& operator=(UILayoutGrid&&) = delete;

        Vector<float, 2> GetMeasurement() const override
        {
            const std::size_t childCount = GetGameObject()->GetChildMap().size();
            const std::size_t rows = (childCount + columns - 1) / columns;

            const float width = margin.x() * 2 + columns * slotSize.x() + (columns - 1) * cellSpacing.x();

            const float height = margin.y() * 2 + rows * slotSize.y() + (rows - 1) * cellSpacing.y();

            return { width, height };
        }

        void Arrange(const Rect<float>& rect) override
        {
            const auto& children = GetGameObject()->GetChildMap();
            std::size_t index = 0;

            for (const auto& child : children | std::views::values)
            {
                const std::size_t col = index % columns;
                const std::size_t row = index / columns;

                const float x = rect.GetMin().x() + margin.x() + col * (slotSize.x() + cellSpacing.x());
                const float y = rect.GetMin().y() + margin.y() + row * (slotSize.y() + cellSpacing.y());

                auto transform = child->GetTransform2d();

                transform->SetPosition({ x, y });
                transform->SetDimensions(slotSize);
                transform->SetAnchors(Transform2d::Anchor::TOP | Transform2d::Anchor::LEFT);

                ++index;
            }
        }

        static std::shared_ptr<UILayoutGrid> Create(const Vector<float, 2>& slotSize, const Vector<float, 2>& cellSpacing, const Vector<float, 2>& margin, std::uint32_t columns)
        {
            std::shared_ptr<UILayoutGrid> result(new UILayoutGrid());

            result->slotSize = slotSize;
            result->cellSpacing = cellSpacing;
            result->margin = margin;
            result->columns = columns;

            return result;
        }

    private:

        UILayoutGrid() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);
        }

        Vector<float, 2> slotSize{ 64,64 };
        Vector<float, 2> cellSpacing{ 8,8 };
        Vector<float, 2> margin{ 0,0 };

        std::uint8_t columns{ 1 };

        DESCRIBE_AND_REGISTER(UILayoutGrid, (UILayout), (), (), (slotSize, cellSpacing, margin, columns))

    };
}

REGISTER_COMPONENT(Blaster::Client::UI::Layouts::UILayoutGrid, 11629)