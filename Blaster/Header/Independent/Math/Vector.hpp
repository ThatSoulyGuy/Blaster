#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <format>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <numeric>
#include <boost/serialization/array.hpp>
#include <boost/serialization/access.hpp>

#include "boost/core/demangle.hpp"
#include "boost/serialization/export.hpp"

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

	template <typename Container>
	concept ArrayType = requires(Container a)
	{
		typename Container::value_type;
		requires Arithmetic<typename Container::value_type>;

		{ a.begin() } -> std::input_or_output_iterator;
		{ a.end() } -> std::input_or_output_iterator;
	};

	template<class Concrete>
	struct AutoExporter
	{
		static const boost::archive::detail::extra_detail::guid_initializer<Concrete>& GetInstance()
		{
			static const auto& instance = boost::serialization::singleton<boost::archive::detail::extra_detail::guid_initializer<Concrete>>::get_mutable_instance().export_guid();

			return instance;
		}

		static inline const bool touched = (GetInstance(), true);
	};

	template <Arithmetic T, size_t N> requires (N > 1)
	class Vector final
	{

	public:
		
		using value_type = T;
		
		Vector() = default;

		Vector(std::initializer_list<T> input)
		{
			assert(input.size() == N);
			std::copy(input.begin(), input.end(), data.begin());
		}

		template <ArrayType U>
		Vector(const U& input)
		{
			assert(std::distance(input.begin(), input.end()) == N);
			std::copy(input.begin(), input.end(), data.begin());
		}

		bool operator==(const Vector& operand) const
		{
			return data == operand.data;
		}

		bool operator!=(const Vector& operand) const
		{
			return !(*this == operand);
		}

		template <ArrayType U>
		bool operator==(const U& operand) const
		{
			if (std::distance(operand.begin(), operand.end()) != N)
				return false;
				
			return std::equal(data.begin(), data.end(), operand.begin());
		}

		template <ArrayType U>
		bool operator!=(const U& operand) const
		{
			return !(*this == operand);
		}

		bool operator==(std::initializer_list<T> operand) const
		{
			if (operand.size() != N)
				return false;

			return std::equal(data.begin(), data.end(), operand.begin());
		}

		bool operator!=(std::initializer_list<T> operand) const
		{
			return !(*this == operand);
		}

		template <ArrayType U>
		Vector& operator=(const U& operand)
		{
			assert(std::distance(operand.begin(), operand.end()) == N);
			std::copy(operand.begin(), operand.end(), data.begin());

			return *this;
		}

		Vector& operator=(std::initializer_list<T> operand)
		{
			assert(operand.size() == N);
			std::copy(operand.begin(), operand.end(), data.begin());

			return *this;
		}

		template <ArrayType U>
		Vector operator+(const U& operand) const
		{
			assert(std::distance(operand.begin(), operand.end()) == N);

			Vector result;

			std::transform(data.begin(), data.end(), operand.begin(), result.data.begin(), std::plus<T>());

			return result;
		}

		Vector operator+(std::initializer_list<T> operand) const
		{
			assert(operand.size() == N);

			Vector result;

			std::transform(data.begin(), data.end(), operand.begin(), result.data.begin(), std::plus<T>());

			return result;
		}

		Vector operator+(T operand) const
		{
			Vector result;

			std::transform(data.begin(), data.end(), result.data.begin(), [&](const T& value) { return value + operand; });

			return result;
		}

		template <ArrayType U>
		Vector operator-(const U& operand) const
		{
			assert(std::distance(operand.begin(), operand.end()) == N);

			Vector result;

			std::transform(data.begin(), data.end(), operand.begin(), result.data.begin(), std::minus<T>());

			return result;
		}

		Vector operator-(std::initializer_list<T> operand) const
		{
			assert(operand.size() == N);

			Vector result;

			std::transform(data.begin(), data.end(), operand.begin(), result.data.begin(), std::minus<T>());

			return result;
		}

		Vector operator-(T operand) const
		{
			Vector result;

			std::transform(data.begin(), data.end(), result.data.begin(), [&](const T& value) { return value - operand; });

			return result;
		}

		template <ArrayType U>
		Vector operator*(const U& operand) const
		{
			assert(std::distance(operand.begin(), operand.end()) == N);

			Vector result;

			std::transform(data.begin(), data.end(), operand.begin(), result.data.begin(), std::multiplies<T>());

			return result;
		}

		Vector operator*(std::initializer_list<T> operand) const
		{
			assert(operand.size() == N);

			Vector result;

			std::transform(data.begin(), data.end(), operand.begin(), result.data.begin(), std::multiplies<T>());

			return result;
		}

		Vector operator*(T operand) const
		{
			Vector result;

			std::transform(data.begin(), data.end(), result.data.begin(), [&](const T& value) { return value * operand; });

			return result;
		}

		template <ArrayType U>
		Vector operator/(const U& operand) const
		{
			assert(std::distance(operand.begin(), operand.end()) == N);

			Vector result;

			std::transform(data.begin(), data.end(), operand.begin(), result.data.begin(), std::divides<T>());

			return result;
		}

		Vector operator/(std::initializer_list<T> operand) const
		{
			assert(operand.size() == N);

			Vector result;

			std::transform(data.begin(), data.end(), operand.begin(), result.data.begin(), std::divides<T>());

			return result;
		}

		Vector operator/(T operand) const
		{
			Vector result;

			std::transform(data.begin(), data.end(), result.data.begin(), [&](const T& value) { return value / operand; });

			return result;
		}

		template <ArrayType U>
		Vector& operator+=(const U& operand)
		{
			assert(std::distance(operand.begin(), operand.end()) == N);

			std::transform(data.begin(), data.end(), operand.begin(), data.begin(), std::plus<T>());

			return *this;
		}

		Vector& operator+=(std::initializer_list<T> operand)
		{
			assert(operand.size() == N);

			std::transform(data.begin(), data.end(), operand.begin(), data.begin(), std::plus<T>());

			return *this;
		}

		Vector& operator+=(T operand)
		{
			std::for_each(data.begin(), data.end(), [&](T& value) { value += operand; });

			return *this;
		}

		template <ArrayType U>
		Vector& operator-=(const U& operand)
		{
			assert(std::distance(operand.begin(), operand.end()) == N);

			std::transform(data.begin(), data.end(), operand.begin(), data.begin(), std::minus<T>());

			return *this;
		}

		Vector& operator-=(std::initializer_list<T> operand)
		{
			assert(operand.size() == N);

			std::transform(data.begin(), data.end(), operand.begin(), data.begin(), std::minus<T>());

			return *this;
		}

		Vector& operator-=(T operand)
		{
			std::for_each(data.begin(), data.end(), [&](T& value) { value -= operand; });
			return *this;
		}

		template <ArrayType U>
		Vector& operator*=(const U& operand)
		{
			assert(std::distance(operand.begin(), operand.end()) == N);

			std::transform(data.begin(), data.end(), operand.begin(), data.begin(), std::multiplies<T>());

			return *this;
		}

		Vector& operator*=(std::initializer_list<T> operand)
		{
			assert(operand.size() == N);

			std::transform(data.begin(), data.end(), operand.begin(), data.begin(), std::multiplies<T>());

			return *this;
		}

		Vector& operator*=(T operand)
		{
			std::for_each(data.begin(), data.end(), [&](T& value) { value *= operand; });

			return *this;
		}

		template <ArrayType U>
		Vector& operator/=(const U& operand)
		{
			assert(std::distance(operand.begin(), operand.end()) == N);

			std::transform(data.begin(), data.end(), operand.begin(), data.begin(), std::divides<T>());

			return *this;
		}

		Vector& operator/=(std::initializer_list<T> operand)
		{
			assert(operand.size() == N);

			std::transform(data.begin(), data.end(), operand.begin(), data.begin(), std::divides<T>());

			return *this;
		}

		Vector& operator/=(T operand)
		{
			std::for_each(data.begin(), data.end(), [&](T& value) { value /= operand; });
			return *this;
		}

		T operator[](size_t operand) const
		{
			return data.at(operand);
		}

		T& operator[](size_t operand)
		{
			return data[operand];
		}

		T& x() requires (N > 0)
		{
			return data[0];
		}

		T x() const requires (N > 0)
		{
			return data[0];
		}

		T& y() requires (N > 1)
		{
			return data[1];
		}

		T y() const requires (N > 1)
		{
			return data[1];
		}

		T& z() requires (N > 2)
		{
			return data[2];
		}

		T z() const requires (N > 2)
		{
			return data[2];
		}

		T& w() requires (N > 3)
		{
			return data[3];
		}

		T w() const requires (N > 3)
		{
			return data[3];
		}

		T& a() requires (N > 4)
		{
			return data[4];
		}

		T a() const requires (N > 4)
		{
			return data[4];
		}

		T& b() requires (N > 5)
		{
			return data[5];
		}

		T b() const requires (N > 5)
		{
			return data[5];
		}

		T& c() requires (N > 6)
		{
			return data[6];
		}

		T c() const requires (N > 6)
		{
			return data[6];
		}

		T& d() requires (N > 7)
		{
			return data[7];
		}

		T d() const requires (N > 7)
		{
			return data[7];
		}

		T& e() requires (N > 8)
		{
			return data[8];
		}

		T e() const requires (N > 8)
		{
			return data[8];
		}

		T& f() requires (N > 9)
		{
			return data[9];
		}

		T f() const requires (N > 9)
		{
			return data[9];
		}

		auto begin()
		{
			return data.begin();
		}

		auto end()
		{
			return data.end();
		}

		auto begin() const
		{
			return data.begin();
		}

		auto end() const
		{
			return data.end();
		}

		static constexpr size_t Length()
		{
			return N;
		}

		static T Magnitude(const Vector& first)
		{
			return std::sqrt(std::inner_product(first.data.begin(), first.data.end(), first.data.begin(), T(0)));
		}

		static Vector Normalize(const Vector& first)
		{
			T magnitude = Magnitude(first);

			if (magnitude == T(0))
				return Vector{ };

			Vector result;

			std::transform(first.data.begin(), first.data.end(), result.data.begin(), [&](T value) { return value / magnitude; });

			return result;
		}

		static T Dot(const Vector& first, const Vector& second)
		{
			return std::inner_product(first.data.begin(), first.data.end(), second.data.begin(), T(0));
		}

		static T AngleBetween(const Vector& v1, const Vector& v2)
		{
			T dotProduct = Dot(v1, v2);
			T magnitudeProduct = Magnitude(v1) * Magnitude(v2);

			if (magnitudeProduct == T(0))
				return T(0);

			return std::acos(dotProduct / magnitudeProduct);
		}

		static Vector Cross(const Vector& first, const Vector& second) requires (N == 3)
		{
			Vector result;

			result.data[0] = first.data[1] * second.data[2] - first.data[2] * second.data[1];
			result.data[1] = first.data[2] * second.data[0] - first.data[0] * second.data[2];
			result.data[2] = first.data[0] * second.data[1] - first.data[1] * second.data[0];

			return result;
		}

		static Vector Project(const Vector& first, const Vector& second)
		{
			T dotProduct = Dot(first, second);
			T magnitudeSquared = Dot(second, second);

			if (magnitudeSquared == T(0))
				return Vector{ };

			Vector result;

			std::transform(second.data.begin(), second.data.end(), result.data.begin(), [&](T value) { return (dotProduct / magnitudeSquared) * value; });

			return result;
		}

		static Vector Reflect(const Vector& first, const Vector& normal)
		{
			Vector normalizedNormal = Normalize(normal);

			T dotProduct = Dot(first, normalizedNormal);

			Vector scaledNormal = normalizedNormal * (2 * dotProduct);

			Vector result;

			std::transform(first.data.begin(), first.data.end(), scaledNormal.data.begin(), result.data.begin(), std::minus<T>());

			return result;
		}

		static Vector Lerp(const Vector& first, const Vector& second, T t)
		{
			Vector result;

			std::transform(first.data.begin(), first.data.end(), second.data.begin(), result.data.begin(), [&](T a, T b) { return a + t * (b - a); });

			return result;
		}

		template <ArrayType U>
		operator U() const
		{
			U result;

			if constexpr (requires { result.resize(data.size()); })
			{
				result.resize(data.size());
				std::copy(data.begin(), data.end(), result.begin());
			}
			else
			{
				assert(std::distance(result.begin(), result.end()) == data.size());
				std::copy(data.begin(), data.end(), result.begin());
			}

			return result;
		}

	private:

		friend class boost::serialization::access;

		template <class Archive>
		void serialize(Archive& archive, const unsigned)
		{
			archive & boost::serialization::make_nvp("data", boost::serialization::make_array(data.data(), N));
		}

		static inline const bool _autoExport = AutoExporter<Vector>::touched;

		std::array<T, N> data;

	};

	template <Arithmetic T, size_t N>
	std::ostream& operator<<(std::ostream& stream, const Vector<T, N>& vector)
	{
		stream << std::string("[ ");

		for (size_t i = 0; i < N; ++i)
		{
			stream << vector.begin()[i];

			if (i < N - 1)
				stream << std::string(", ");
		}

		stream << std::string(" ]");

		return stream;
	}
}

namespace std
{
	template <Blaster::Independent::Math::Arithmetic T, size_t N>
	struct hash<Blaster::Independent::Math::Vector<T, N>>
	{
		size_t operator()(const Blaster::Independent::Math::Vector<T, N>& input) const
		{
			size_t hashValue = 0;

			for (const T& val : input)
				hashValue ^= std::hash<T>{}(val)+0x9e3779b9 + (hashValue << 6) + (hashValue >> 2);
			
			return hashValue;
		}
	};

	template <Blaster::Independent::Math::Arithmetic T, size_t N>
	struct formatter<Blaster::Independent::Math::Vector<T, N>> : formatter<T>
	{
		template <typename FormatContext>
		auto format(const Blaster::Independent::Math::Vector<T, N>& input, FormatContext& context)
		{
			auto out = context.out();

			out = std::format_to(out, "{{ ");

			for (size_t i = 0; i < N; ++i)
			{
				out = std::format_to(out, "{}", input.begin()[i]);

				if (i < N - 1)
					out = std::format_to(out, ", ");
			}

			out = std::format_to(out, " }}");

			return out;
		}
	};
}