#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <boost/preprocessor/cat.hpp>

#if defined(__clang__) || defined(__GNUC__)
#  define BLASTER_USED [[gnu::used]]
#else
#  define BLASTER_USED [[maybe_unused]]
#endif

#define CONCAT_NAME BOOST_PP_CAT(_Reg_, __COUNTER__)

#define REGISTER_TYPE(TYPE, NumericId)                                         \
namespace Blaster::Independent::Utility {                                      \
    template<>                                                                 \
    struct TypeIdFromType<TYPE> : std::integral_constant<std::size_t, NumericId> \
    {                                                                          \
        BLASTER_USED                                                           \
        inline static const bool registered = []() noexcept {                  \
            ::Blaster::Independent::Utility::Add(NumericId, #TYPE);            \
            return true;                                                       \
        }();                                                                   \
    };                                                                         \
    template<> struct TypeFromId<NumericId> { using Type = TYPE; };            \
}

namespace Blaster::Independent::Utility
{
    template<typename T>
    struct TypeIdFromType;

    template<std::size_t Id>
    struct TypeFromId;

    consteval std::uint64_t HashString(const std::string_view text)
    {
        std::uint64_t h = 14695981039346656037ull;

        for (const char c : text)
        {
            h ^= static_cast<std::uint8_t>(c);
            h *= 1099511628211ull;
        }

        return h;
    }

    struct Maps
    {
        std::unordered_map<std::size_t, std::string> idToName;
        std::unordered_map<std::string, std::size_t> nameToId;
        std::mutex guard;
    };

    inline Maps& GetMaps()
    {
        static Maps m;
        return m;
    }

    inline void Add(std::size_t id, std::string name)
    {
        auto& [idToName, nameToId, guard] = GetMaps();

        std::lock_guard lk(guard);

        idToName.try_emplace(id, "class " + name);
        nameToId.try_emplace("class " + name, id);
    }

    template <typename T>
    consteval std::size_t CalcTypeId()
    {
#ifdef _MSC_VER
        return HashString(__FUNCSIG__);
#else
        return HashString(__PRETTY_FUNCTION__);
#endif
    }

    template <class T>
    struct TypeIdFromType : std::integral_constant<std::size_t, CalcTypeId<T>()>
    {
        BLASTER_USED
        inline static const bool registered = []() noexcept
            {
                Add(TypeIdFromType<T>::value, typeid(T).name());
                return true;
            }();
    };

    class TypeRegistrar
    {

    public:

        template<typename T>
        static consteval std::size_t GetTypeId()
        {
            return TypeIdFromType<T>::value;
        }

        template<std::size_t Id>
        using GetTypeFromId = typename TypeFromId<Id>::Type;

        static std::string GetRuntimeName(const std::size_t id)
        {
            auto& m = GetMaps().idToName;

            const auto it = m.find(id);

            return it != m.end() ? it->second : std::string{};
        }

        static std::optional<std::size_t> GetIdFromRuntimeName(const std::string& name)
        {
            auto& m = GetMaps().nameToId;

            auto it = m.find(name);

            if (it == m.end())
                return std::nullopt;

            return it->second;
        }
    };
}