#pragma once

#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <unordered_map>
#include <typeindex>
#include <memory>

template<class T>
using HasDescription = boost::describe::has_describe_members<T>;

template<class W, class = void>
struct UnwrapDescriptor
{
    using type = W;
};

template<class W>
struct UnwrapDescriptor<W, std::void_t<typename W::type>>
{
    using type = typename W::type;
};

template<class L, class R = L, class = void>
struct HasInequalityOperators : std::false_type { };

template<class L, class R>
struct HasInequalityOperators<L, R, std::void_t<decltype(std::declval<const L&>() != std::declval<const R&>())>> : std::true_type { };

template<class T>
using HasDescription = boost::describe::has_describe_members<T>;

template<class T, class A, class Wrapper>
void CopyIfChanged(T& destination, const A& source, Wrapper)
{
    using M = typename UnwrapDescriptor<Wrapper>::type;

    constexpr auto p = []
    {
        if constexpr (std::is_member_pointer_v<decltype(M::pointer)>)
            return M::pointer;
        else
            return M::pointer();
    }();

    using member_t = std::remove_reference_t<decltype(destination.*p)>;

    if constexpr (HasInequalityOperators<member_t>::value)
    {
        if (destination.*p != source.*p)
            destination.*p = source.*p;
    }
}

template<class T, bool = HasDescription<T>::value>
struct Merger
{
    static void Apply(T&, const T&) { }
};

template<class T>
struct Merger<T, true>
{
    static void Apply(T& destination, const T& source)
    {
        using Members = boost::describe::describe_members<T, boost::describe::mod_any_access | boost::describe::mod_inherited>;

        boost::mp11::mp_for_each<Members>([&](auto W) { CopyIfChanged(destination, source, W); });
    }
};

template<class T>
inline void MergeFields(T& dst, const T& src)
{
    Merger<T>::Apply(dst, src);
}

namespace Detail
{
    using MergeFn = void(*)(void*, const void*);

    inline std::unordered_map<std::type_index, MergeFn>& Table()
    {
        static std::unordered_map<std::type_index, MergeFn> table;

        return table;
    }

    template<class Derived>
    struct Registrar
    {
        static void Thunk(void* d, const void* s)
        {
            MergeFields(*static_cast<Derived*>(d), *static_cast<const Derived*>(s));
        }

        Registrar() { Table().emplace(typeid(Derived), &Thunk); }
    };
}

template<class Base>
void MergeComponents(std::shared_ptr<Base>& destination, const std::shared_ptr<Base>& incoming)
{
    if (!destination || !incoming)
        return;

    if (typeid(*destination) != typeid(*incoming))
        return;

    auto iterator = Detail::Table().find(typeid(*destination));

    if (iterator == Detail::Table().end())
        return;

    iterator->second(destination.get(), incoming.get());
}

#define DESCRIBE_AND_REGISTER(CLASS, BASES, PUB_FNS, PROT_FNS, DATA) \
    BOOST_DESCRIBE_CLASS(CLASS, BASES, PUB_FNS, PROT_FNS, DATA)      \
    private:                                                         \
        static const inline ::Detail::Registrar<CLASS>         \
            _auto_merge_registrar_{};