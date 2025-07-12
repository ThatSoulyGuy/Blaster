#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <boost/preprocessor/cat.hpp>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "DbgHelp.lib")
#else
#include <cxxabi.h>
#include <cstdlib>
#endif

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

    inline void Add(std::size_t id, const std::string& name)
    {
        auto& [idToName, nameToId, guard] = GetMaps();

        std::lock_guard lk(guard);

#ifdef _MSC_VER
        std::string keyword = "class ";
#else
        std::string keyword;
#endif

        idToName.try_emplace(id, keyword + name);
        nameToId.try_emplace(keyword + name, id);
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
        [[nodiscard]]
        static std::string DemangleName(const char* encodedName)
        {
#if defined(_MSC_VER)
            char buffer[1024] = {};

            if(UnDecorateSymbolName(encodedName, buffer, static_cast<DWORD>(std::size(buffer)), UNDNAME_COMPLETE | UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_MEMBER_TYPE | UNDNAME_NO_THISTYPE))
                return buffer;

            throw std::runtime_error("Failed to demangle name");
#else
            int status = 0;

            const std::unique_ptr<char, void(*)(void*)> result{ abi::__cxa_demangle(encodedName, nullptr, nullptr, &status), std::free };

            return status == 0 ? result.get() : throw std::runtime_error("Failed to demangle name");;
#endif
        }

        BLASTER_USED
        inline static const bool registered = []()
            {
                Add(TypeIdFromType<T>::value, DemangleName(typeid(T).name()));
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