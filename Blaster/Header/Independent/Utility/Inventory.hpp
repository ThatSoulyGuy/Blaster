#pragma once

#include "Independent/ECS/GameObject.hpp"

namespace Blaster::Independent::Utility
{
    class Inventory final : public Component
    {

    public:

        void SetSlotAt(const Vector<std::uint8_t, 2>& position, std::uint32_t fillType = 0)
        {
            const auto index = ToIndex(position);

            if (!index.has_value())
            {
                std::cerr << "Component 'Inventory' on game object '" << GetGameObject()->GetAbsolutePath() << "' does not contain slot at " << position << "!" << std::endl;
                return;
            }

            data[index.value()] = fillType;
        }

        [[nodiscard]]
        std::optional<std::uint32_t> GetSlotAt(const Vector<std::uint8_t, 2>& position) const
        {
            const auto index = ToIndex(position);

            if (!index.has_value())
            {
                std::cerr << "Component 'Inventory' on game object '" << GetGameObject()->GetAbsolutePath() << "' does not contain slot at " << position << "!" << std::endl;
                return std::nullopt;
            }

            return std::make_optional(data[index.value()]);
        }

        static std::shared_ptr<Inventory> Create(const Vector<std::uint8_t, 2>& dimensions, std::uint32_t fillType = 0)
        {
            std::shared_ptr<Inventory> result(new Inventory());

            result->dimensions = dimensions;
            result->data.assign(static_cast<std::size_t>(dimensions.x() * dimensions.y()), fillType);

            return result;
        }

    private:

        Inventory() = default;

        [[nodiscard]]
        std::optional<std::size_t> ToIndex(const Vector<std::uint8_t, 2>& position) const noexcept
        {
            if (position.x() >= dimensions.x() || position.y() >= dimensions.y())
                return std::nullopt;
            
            return static_cast<std::size_t>(position.y() * dimensions.x() + position.x());
        }

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);
            
            archive & BOOST_SERIALIZATION_NVP(dimensions);
            archive & BOOST_SERIALIZATION_NVP(data);
        }

        Vector<std::uint8_t, 2>  dimensions{ 0, 0 };
        std::vector<std::uint32_t> data;

        DESCRIBE_AND_REGISTER(Inventory, (Component), (), (), (dimensions, data))
    };
}

REGISTER_COMPONENT(Blaster::Independent::Utility::Inventory, 20492)