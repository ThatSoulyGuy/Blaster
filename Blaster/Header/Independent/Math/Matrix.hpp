#pragma once

#include <sstream>
#include <numbers>
#include <boost/serialization/array.hpp>
#include <boost/serialization/nvp.hpp>
#include "Independent/Math/Vector.hpp"
#include "Independent/Utility/BulletSterilized.hpp"

namespace Blaster::Independent::Math
{
	template <typename Container>
	concept DoubleArrayType = requires(Container a)
	{
		typename Container::value_type;
		requires ArrayType<typename Container::value_type>;

		{ a.begin() } -> std::input_or_output_iterator;
		{ a.end() } -> std::input_or_output_iterator;
	};

	template <Arithmetic T, size_t R, size_t C>
	class Matrix final
	{

	public:

		using value_type = T;

		Matrix() = default;

		Matrix(std::initializer_list<std::initializer_list<T>> init)
		{
			assert(init.size() == C && "Outer list must have C columns");
			size_t col = 0;

			for (const auto& columnList : init)
			{
				assert(columnList.size() == R && "Each column must have R elements (rows)");
				size_t row = 0;

				for (T val : columnList)
				{
					data[col][row] = val;
					++row;
				}

				++col;
			}
		}

		template <ArrayType U>
		Matrix(const U& input)
		{
			assert(std::distance(input.begin(), input.end()) == C);
			size_t col = 0;

			for (const auto& colData : input)
			{
				assert(std::distance(colData.begin(), colData.end()) == R);
				size_t row = 0;

				for (auto val : colData)
				{
					data[col][row] = val;
					++row;
				}

				++col;
			}
		}

		std::array<T, R>& operator[](size_t col)
		{
			return data[col];
		}
		const std::array<T, R>& operator[](size_t col) const
		{
			return data[col];
		}

		template <ArrayType U>
		Matrix& operator=(const U& rhs)
		{
			assert(std::distance(rhs.begin(), rhs.end()) == C);
			size_t col = 0;

			for (auto& colData : rhs)
			{
				assert(std::distance(colData.begin(), colData.end()) == R);
				size_t row = 0;

				for (auto val : colData)
				{
					data[col][row] = val;
					++row;
				}

				++col;
			}

			return *this;
		}

		Matrix& operator=(std::initializer_list<std::initializer_list<T>> rhs)
		{
			assert(rhs.size() == C);

			size_t col = 0;

			for (const auto& colList : rhs)
			{
				assert(colList.size() == R);

				size_t row = 0;

				for (auto val : colList)
				{
					data[col][row] = val;
					++row;
				}

				++col;
			}

			return *this;
		}

		Matrix operator+(const Matrix& rhs) const
		{
			Matrix result;

			for (size_t c = 0; c < C; c++)
			{
				for (size_t r = 0; r < R; r++)
					result[c][r] = data[c][r] + rhs[c][r];
			}

			return result;
		}
		Matrix& operator+=(const Matrix& rhs)
		{
			for (size_t c = 0; c < C; c++)
			{
				for (size_t r = 0; r < R; r++)
					data[c][r] += rhs[c][r];
			}

			return *this;
		}

		Matrix operator-(const Matrix& rhs) const
		{
			Matrix result;

			for (size_t c = 0; c < C; c++)
			{
				for (size_t r = 0; r < R; r++)
					result[c][r] = data[c][r] - rhs[c][r];
			}

			return result;
		}
		Matrix& operator-=(const Matrix& rhs)
		{
			for (size_t c = 0; c < C; c++)
			{
				for (size_t r = 0; r < R; r++)
					data[c][r] -= rhs[c][r];
			}

			return *this;
		}

		Matrix operator*(T scalar) const
		{
			Matrix result;

			for (size_t c = 0; c < C; c++)
			{
				for (size_t r = 0; r < R; r++)
					result[c][r] = data[c][r] * scalar;
			}

			return result;
		}

		Vector<T, R> operator*(const Vector<T, C>& v) const
		{
			Vector<T, R> out{};

			for (size_t i = 0; i < R; ++i)
			{
				T sum = T(0);

				for (size_t k = 0; k < C; ++k)
					sum += data[k][i] * v[k];

				out[i] = sum;
			}
			return out;
		}

		Matrix& operator*=(T scalar)
		{
			for (size_t c = 0; c < C; c++)
			{
				for (size_t r = 0; r < R; r++)
					data[c][r] *= scalar;
			}

			return *this;
		}

		template <size_t K>
		Matrix<T, R, K> operator*(const Matrix<T, C, K>& rhs) const
		{
			Matrix<T, R, K> result;

			for (size_t j = 0; j < K; j++)
			{
				for (size_t i = 0; i < R; i++)
				{
					T sum = T(0);
					for (size_t k = 0; k < C; k++)
						sum += data[k][i] * rhs.data[j][k];
					
					result.data[j][i] = sum;
				}
			}

			return result;
		}

		Matrix& operator*=(const Matrix& rhs) requires (R == C)
		{
			*this = (*this) * rhs;
			return *this;
		}

		static Matrix Identity() requires (R == C)
		{
			Matrix result;
			for (size_t c = 0; c < C; c++)
			{
				for (size_t r = 0; r < R; r++)
					result[c][r] = (r == c) ? T(1) : T(0);
			}
			return result;
		}

		static Matrix<T, C, R> Transpose(const Matrix& a)
		{
			Matrix<T, C, R> out;

			for (size_t c = 0; c < C; ++c)
			{
				for (size_t r = 0; r < R; ++r)
					out[c][r] = a.data[r][c];
			}

			return out;
		}

		static Matrix<T, 4, 4> Perspective(T fovRadians, T aspect, T nearPlane, T farPlane)
		{
			T tanHalfFov = std::tan(fovRadians / T(2));

			return Matrix<T, 4, 4>(
			{
				{ 1 / (aspect * tanHalfFov), 0, 0, 0 },
				{ 0, 1 / tanHalfFov, 0, 0 },
				{ 0, 0, -(farPlane + nearPlane) / (farPlane - nearPlane), -1 },
				{ 0, 0, -(2 * farPlane * nearPlane) / (farPlane - nearPlane), 0 }
			});
		}

		static Matrix<T, 4, 4> Orthographic(T left, T right, T bottom, T top, T nearPlane, T farPlane)
		{
			return Matrix<T, 4, 4>(
			{
				{ 2 / (right - left), 0, 0, 0 },
				{ 0, 2 / (top - bottom), 0, 0 },
				{ 0, 0, -2 / (farPlane - nearPlane), 0 },
				{ -(right + left) / (right - left), -(top + bottom) / (top - bottom), -(farPlane + nearPlane) / (farPlane - nearPlane), 1 }
			});
		}

		static Matrix<T, 4, 4> Translation(const Vector<T, 3>& translation)
		{
			Matrix<T, 4, 4> result = Identity();

			result.data[3][0] = translation.x();
			result.data[3][1] = translation.y();
			result.data[3][2] = translation.z();

			return result;
		}

		static Matrix<T, 4, 4> Scale(const Vector<T, 3>& scale)
		{
			return Matrix<T, 4, 4>(
			{
				{ scale.x(), 0,         0,         0 },
				{ 0,         scale.y(), 0,         0 },
				{ 0,         0,         scale.z(), 0 },
				{ 0,         0,         0,         1 }
			});
		}

		static Matrix<T, 4, 4> RotationX(T angleRadians)
		{
			T c = std::cos(angleRadians);
			T s = std::sin(angleRadians);

			return Matrix<T, 4, 4>(
			{
				{ 1,  0, 0, 0 },
				{ 0,  c, s, 0 },
				{ 0, -s, c, 0 },
				{ 0,  0, 0, 1 }
			});
		}

		static Matrix<T, 4, 4> RotationY(T angleRadians)
		{
			T c = std::cos(angleRadians);
			T s = std::sin(angleRadians);

			return Matrix<T, 4, 4>(
			{
				{  c, 0, -s, 0 },
				{  0, 1,  0, 0 },
				{  s, 0,  c, 0 },
				{  0, 0,  0, 1 }
			});
		}

		static Matrix<T, 4, 4> RotationZ(T angleRadians)
		{
			T c = std::cos(angleRadians);
			T s = std::sin(angleRadians);

			return Matrix<T, 4, 4>(
			{
				{ c,  s, 0, 0 },
				{ -s, c, 0, 0 },
				{ 0,  0, 1, 0 },
				{ 0,  0, 0, 1 }
			});
		}

		static Matrix<T, 4, 4> LookAt(const Vector<T, 3>& eye, const Vector<T, 3>& center, const Vector<T, 3>& up)
		{
			auto fwd = Vector<T, 3>::Normalize(eye - center);
			auto right = Vector<T, 3>::Normalize(Vector<T, 3>::Cross(up, fwd));
			auto realUp = Vector<T, 3>::Cross(fwd, right);

			return Matrix<T, 4, 4>(
			{
				{ right.x(),   realUp.x(),   fwd.x(),   T(0) },
				{ right.y(),   realUp.y(),   fwd.y(),   T(0) },
				{ right.z(),   realUp.z(),   fwd.z(),   T(0) },
				{ -Vector<T,3>::Dot(right,eye), -Vector<T,3>::Dot(realUp,eye), -Vector<T,3>::Dot(fwd,eye), T(1) }
			});
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

		static void Decompose(const Matrix& matrix, Vector<T, 3>& outPos, Vector<T, 3>& outRotDeg, Vector<T, 3>& outScale)
		{
			static_assert(R == 4 && C == 4, "Decompose only defined for 4x4");

			outPos = { matrix.data[3][0], matrix.data[3][1], matrix.data[3][2] };

			Vector<T, 3> col0{ matrix.data[0][0], matrix.data[0][1], matrix.data[0][2] };
			Vector<T, 3> col1{ matrix.data[1][0], matrix.data[1][1], matrix.data[1][2] };
			Vector<T, 3> col2{ matrix.data[2][0], matrix.data[2][1], matrix.data[2][2] };

			outScale = { Vector<T,3>::Magnitude(col0), Vector<T,3>::Magnitude(col1), Vector<T,3>::Magnitude(col2) };

			if (outScale.x() == 0 || outScale.y() == 0 || outScale.z() == 0)
			{
				outRotDeg = { 0, 0, 0 };

				return;
			}

			col0 /= outScale.x();
			col1 /= outScale.y();
			col2 /= outScale.z();

			T pitch, yaw, roll;

			constexpr T pi = std::numbers::pi_v<T>;

			pitch = std::asin(std::clamp(-col2.y(), T(-1), T(1)));

			if (std::fabs(std::cos(pitch)) > T(1e-6))
			{
				roll = std::atan2(col2.z(), col2.x());
				yaw = std::atan2(col1.y(), col0.y());
			}
			else
			{
				roll = 0;
				yaw = std::atan2(-col0.z(), col1.x());
			}

			constexpr T rad2deg = T(180) / pi;

			outRotDeg = { roll * rad2deg, pitch * rad2deg, yaw * rad2deg };
		}

		static Matrix Inverse(const Matrix& m) requires (R == C)
		{
			constexpr std::size_t N = R;

			Matrix<T, N, N> a = m;
			Matrix<T, N, N> inv = Identity();

			for (std::size_t col = 0; col < N; ++col)
			{
				std::size_t pivot = col;

				T maxAbs = std::abs(a[col][col]);

				for (std::size_t row = col + 1; row < N; ++row)
				{
					T v = std::abs(a[col][row]);

					if (v > maxAbs)
					{
						maxAbs = v;
						pivot = row;
					}
				}

				if (maxAbs == T(0))
					throw std::runtime_error("Matrix::Inverse singular matrix");

				if (pivot != col)
				{
					for (std::size_t c = 0; c < N; ++c)
						std::swap(a[c][col], a[c][pivot]);

					for (std::size_t c = 0; c < N; ++c)
						std::swap(inv[c][col], inv[c][pivot]);
				}

				T diag = a[col][col];
				for (std::size_t c = 0; c < N; ++c)
				{
					a[c][col] /= diag;
					inv[c][col] /= diag;
				}

				for (std::size_t row = 0; row < N; ++row)
				{
					if (row == col)
						continue;

					T f = a[col][row];

					if (f == T(0))
						continue;

					for (std::size_t c = 0; c < N; ++c)
					{
						a[c][row] -= f * a[c][col];
						inv[c][row] -= f * inv[c][col];
					}
				}
			}
			return inv;
		}

		inline btTransform ToBtTransform(const Matrix<float, 4, 4>& M)
		{
			btMatrix3x3 basis
			(
				M[0][0], M[1][0], M[2][0],
				M[0][1], M[1][1], M[2][1],
				M[0][2], M[1][2], M[2][2]
			);

			btVector3 origin(M[3][0], M[3][1], M[3][2]);

			btTransform result;

			result.setBasis(basis);
			result.setOrigin(origin);

			return result;
		}

		inline Matrix<float, 4, 4> FromBtTransform(const btTransform& t)
		{
			const btMatrix3x3& b = t.getBasis();
			const btVector3& o = t.getOrigin();

			return Matrix<float, 4, 4>(
			{
				{ b[0][0], b[1][0], b[2][0], 0.f },
				{ b[0][1], b[1][1], b[2][1], 0.f },
				{ b[0][2], b[1][2], b[2][2], 0.f },
				{ o.x(), o.y(), o.z(), 1.f }
			});
		}

		static constexpr size_t Rows()
		{
			return R;
		}

		static constexpr size_t Columns()
		{
			return C;
		}

		template <class Archive>
		void serialize(Archive& ar, const unsigned)
		{
			ar & boost::serialization::make_nvp("data", data);
		}

	private:

		std::array<std::array<T, R>, C> data;

		template <Arithmetic U, size_t V, size_t X>
		friend bool operator==(const Matrix<U, V, X>&, const Matrix<U, V, X>&);

		template <Arithmetic U, size_t V, size_t X>
		friend bool operator!=(const Matrix<U, V, X>&, const Matrix<U, V, X>&);
	};

	template <Arithmetic T, size_t R, size_t C>
	bool operator==(const Matrix<T, R, C>& lhs, const Matrix<T, R, C>& rhs)
	{
		for (size_t col = 0; col < C; col++)
		{
			for (size_t row = 0; row < R; row++)
			{
				if (lhs.data[col][row] != rhs.data[col][row])
					return false;
			}
		}

		return true;
	}

	template <Arithmetic T, size_t R, size_t C>
	bool operator!=(const Matrix<T, R, C>& lhs, const Matrix<T, R, C>& rhs)
	{
		return !(lhs == rhs);
	}

	template <Arithmetic T, size_t R, size_t C>
	std::ostream& operator<<(std::ostream& os, const Matrix<T, R, C>& m)
	{
		for (size_t r = 0; r < R; ++r)
		{
			os << '[';

			for (size_t c = 0; c < C; ++c)
			{
				os << m[c][r];

				if (c + 1 < C)
					os << ", ";
			}

			os << ']';

			if (r + 1 < R)
				os << '\n';
		}
		return os;
	}
}

namespace std
{
	inline void HashCombine(std::size_t& seed, const std::size_t value) noexcept
	{
		constexpr std::size_t magic = 0x9e3779b97f4a7c15ULL;
		seed ^= value + magic + (seed << 6) + (seed >> 2);
	}

	template <Blaster::Independent::Math::Arithmetic T, size_t R, size_t C>
	struct hash<Blaster::Independent::Math::Matrix<T, R, C>>
	{
		std::size_t operator()(const Blaster::Independent::Math::Matrix<T, R, C>& m) const noexcept
		{
			std::size_t seed = 0;

			for (size_t c = 0; c < C; ++c)
			{
				for (size_t r = 0; r < R; ++r)
					HashCombine(seed, std::hash<T>{}(m[c][r]));
			}

			return seed;
		}
	};
}

template <Blaster::Independent::Math::Arithmetic T, size_t R, size_t C>
struct std::formatter<Blaster::Independent::Math::Matrix<T, R, C>> : std::formatter<std::string>
{
	auto format(const Blaster::Independent::Math::Matrix<T, R, C>& mat, std::format_context& ctx)
	{
		std::ostringstream oss;

		oss << mat;

		return std::formatter<std::string>::format(oss.str(), ctx);
	}
};