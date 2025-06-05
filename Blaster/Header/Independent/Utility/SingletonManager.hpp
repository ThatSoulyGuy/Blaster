#pragma once

#include <optional>
#include <vector>

namespace Blaster::Independent::Utility
{
    template <typename T, typename A>
    class SingletonManager
    {

    public:

        virtual ~SingletonManager() { }

        virtual T Register(T) = 0;
        virtual void Unregister(A) = 0;
        virtual bool Has(A) const = 0;
        virtual std::optional<T> Get(A) = 0;
        virtual std::vector<T> GetAll() const = 0;

    };
}