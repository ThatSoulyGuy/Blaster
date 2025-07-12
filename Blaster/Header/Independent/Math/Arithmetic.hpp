#pragma once

#include <concepts>

namespace Blaster::Independent::Math
{
    template <typename T>
    concept Arithmetic = requires(T x, T y)
    {
        { x + y } -> std::convertible_to<T>;
        { x - y } -> std::convertible_to<T>;
        { x * y } -> std::convertible_to<T>;
        { x / y } -> std::convertible_to<T>;
    };
}