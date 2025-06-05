#pragma once

#include <type_traits>
#include <vector>
#include <algorithm>
#include <functional>

#define BUILDABLE_PROPERTY(name, type, container) \
type name = type(); \
public: static constexpr auto name##Pointer = &container::name; \
using name##Setter = Setter<container, type, container::name##Pointer>;

namespace Blaster::Independent::Utility
{
    template <typename Class, typename MemberType, MemberType Class::* MemberPtr>
    struct Setter
    {
        using ValueType = MemberType;
        ValueType value;

        void operator()(Class& obj) const
        {
            obj.*MemberPtr = value;
        }
    };

    template <typename T>
    class Builder
    {

    public:

        static Builder New() { return Builder(); }

        template <typename SetterType>
        Builder& Set(SetterType setter)
        {
            static_assert(std::is_invocable_v<SetterType, T&>, "SetterType must be callable with T&");

            setters.push_back(([setter](T& obj) { setter(obj); }));
            return *this;
        }

        void Build(T& obj) const
        {
            std::for_each(setters.begin(), setters.end(), [&obj](std::function<void(T&)> setter) { setter(obj); });
        }

    private:

        Builder() = default;

        std::vector<std::function<void(T&)>> setters;

    };
}