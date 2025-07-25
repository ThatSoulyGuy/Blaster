#pragma once

#include <algorithm>
#include <ostream>
#include "Independent/Math/Vector.hpp"

namespace Blaster::Independent::Math
{
    template <Arithmetic T>
    class Rect
    {

    public:

        [[nodiscard]]
        Vector<T, 2> GetMin() const
        {
            return min;
        }

        void SetMin(const Vector<T, 2>& min)
        {
            this->min = min;
        }

        [[nodiscard]]
        Vector<T, 2> GetMax() const
        {
            return max;
        }

        void SetMax(const Vector<T, 2>& max)
        {
            this->max = max;
        }

        [[nodiscard]]
        Vector<T, 2> GetDimensions() const
        {
            return max - min;
        }

        [[nodiscard]]
        Vector<T, 2> GetCenter() const
        {
            return (min + max) * T(0.5);
        }

        [[nodiscard]]
        bool IsEmpty() const
        {
            return (max.x() <= min.x()) || (max.y() <= min.y());
        }

        [[nodiscard]]
        bool Contains(Vector<T, 2> p) const
        {
            return p.x() >= min.x() && p.y() >= min.y() && p.x() < max.x() && p.y() < max.y();
        }

        [[nodiscard]]
        bool Intersects(const Rect& other) const
        {
            return !(other.min.x() >= max.x() || other.max.x() <= min.x() || other.min.y() >= max.y() || other.max.y() <= min.y());
        }

        Rect& Translate(Vector<T, 2> delta)
        {
            min += delta;
            max += delta;
        
            return *this;
        }

        Rect& Inflate(Vector<T, 2> amt)
        {
            min -= amt;
            max += amt;
        
            return *this;
        }

        static Rect Union(const Rect& a, const Rect& b)
        {
            return FromMinMax({ std::min(a.min.x(),b.min.x()), std::min(a.min.y(), b.min.y()) }, { std::max(a.max.x(), b.max.x()), std::max(a.max.y(), b.max.y()) });
        }

        static Rect Intersection(const Rect& a, const Rect& b)
        {
            return FromMinMax({ std::max(a.min.x(),b.min.x()), std::max(a.min.y(), b.min.y()) }, { std::min(a.max.x(), b.max.x()), std::min(a.max.y(), b.max.y()) });
        }

        static Rect FromMinMax(const Vector<T, 2>& min, const Vector<T, 2>& max)
        {
            return Rect{ std::min(min.x(), max.x()), std::min(min.y(), max.y()), std::max(min.x(), max.x()), std::max(min.y(), max.y()) };
        }

        static Rect FromPositionAndSize(const Vector<T, 2>& position, const Vector<T, 2>& size)
        {
            return Rect{ position.x(), position.y(), position.x() + size.x(), position.y() + size.y() };
        }

    private:

        friend class boost::serialization::access;

        Rect(T x0, T y0, T x1, T y1)
        {
            min = { x0, y0 };
            max = { x1, y1 };
        }

        template <typename Archive>
        void serialize(Archvie& archive, const unsigned)
        {
            archive & BOOST_SERIALIZATION_NVP(min);
            archive & BOOST_SERIALIZATION_NVP(max);
        }

        friend std::ostream& operator<<(std::ostream& stream, const Rect& rect)
        {
            return stream << "[" << rect.min << ", " << rect.max << "]";
        }

        Vector<T, 2> min{ 0.0f, 0.0f };
        Vector<T, 2> max{ 0.0f, 0.0f };

        static inline const bool _autoExport = AutoExporter<Rect>::touched;
    };
}