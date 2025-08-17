#pragma once

#include <memory>
#include <glad/glad.h>
#include "Independent/ComponentRegistry.hpp"
#include "Independent/ECS/GameObject.hpp"
#include "Independent/ECS/ComponentFactory.hpp"
#include "Independent/Utility/AssetPath.hpp"
#include "Independent/Utility/FileHelper.hpp"

using namespace Blaster::Independent::ECS;
using namespace Blaster::Independent::Utility;

namespace Blaster::Client::Render
{
	class Shader final : public Component
	{

	public:

		Shader(const Shader&) = delete;
		Shader(Shader&&) = delete;
		Shader& operator=(const Shader&) = delete;
		Shader& operator=(Shader&&) = delete;

		void Bind() const
		{
			glUseProgram(id);
		}

		void SetUniform(const std::string& name, const int value) const
		{
			const GLint location = GetUniformLocationCached(name);
			if (location == -1) return;

			// Value cache: skip if identical
			CacheValue newVal = value;
			if (IsSame(name, newVal)) return;

			glUniform1i(location, value);
			UpdateCache(name, std::move(newVal));
		}

		void SetUniform(const std::string& name, const bool value) const
		{
			SetUniform(name, static_cast<int>(value));
		}

		void SetUniform(const std::string& name, const float value) const
		{
			const GLint location = GetUniformLocationCached(name);

			if (location == -1)
				return;

			CacheValue newVal = value;

			if (IsSame(name, newVal))
				return;

			glUniform1f(location, value);
			UpdateCache(name, std::move(newVal));
		}

		void SetUniform(const std::string& name, const Vector<float, 2>& value) const
		{
			const GLint location = GetUniformLocationCached(name);

			if (location == -1)
				return;

			CacheValue newVal = std::array<float, 2>{ value[0], value[1] };

			if (IsSame(name, newVal))
				return;

			glUniform2f(location, value[0], value[1]);
			UpdateCache(name, std::move(newVal));
		}

		void SetUniform(const std::string& name, const Vector<float, 3>& value) const
		{
			const GLint location = GetUniformLocationCached(name);

			if (location == -1)
				return;

			CacheValue newVal = std::array<float, 3>{ value[0], value[1], value[2] };

			if (IsSame(name, newVal))
				return;

			glUniform3f(location, value[0], value[1], value[2]);
			UpdateCache(name, std::move(newVal));
		}

		void SetUniform(const std::string& name, const Matrix<float, 4, 4>& value) const
		{
			const GLint location = GetUniformLocationCached(name);

			if (location == -1)
				return;

			CacheValue newVal = value;

			if (IsSame(name, newVal))
				return;

			glUniformMatrix4fv(location, 1, GL_FALSE, &value[0][0]);
			UpdateCache(name, std::move(newVal));
		}

		void SetUniform(const std::string& name, const std::vector<Matrix<float, 4, 4>>& value) const
		{
			std::string query = NormalizeArrayName(name);

			const auto data = value.data();
			const auto count = value.size();

			if (!data || count == 0)
				return;

			const GLint location = GetUniformLocationCached(query);

			if (location == -1)
				return;

			Mat4ArraySig sig;

			sig.count = count;
			sig.hash = FNV1a(reinterpret_cast<const uint8_t*>(&data[0][0]), count * 16u * sizeof(float));

			CacheValue newVal = sig;

			if (IsSame(query, newVal))
				return;

			glUniformMatrix4fv(location, static_cast<GLsizei>(count), GL_FALSE, reinterpret_cast<const GLfloat*>(&data[0][0]));

			UpdateCache(query, std::move(newVal));
		}

		[[nodiscard]]
		std::string GetRegistryName() const
		{
			return name;
		}

		static std::shared_ptr<Shader> Create(const std::string& name, const AssetPath& localPath)
		{
			std::shared_ptr<Shader> result(new Shader());

			result->name = name;
			result->localPath = localPath;
			result->vertexPath = { { localPath.GetDomain() }, std::format("{}Vertex.glsl", localPath.GetLocalPath()) };
			result->fragmentPath = { { localPath.GetDomain() }, std::format("{}Fragment.glsl", localPath.GetLocalPath()) };
			result->vertexData = FileHelper::ReadFile(result->vertexPath);
			result->fragmentData = FileHelper::ReadFile(result->fragmentPath);

			result->Generate();

			return result;
		}

	private:

		struct Mat4ArraySig
		{
			size_t count{};
			uint64_t hash{};

			bool operator==(const Mat4ArraySig& o) const noexcept
			{
				return count == o.count && hash == o.hash;
			}
		};

		using CacheValue = std::variant<int, float, std::array<float, 2>, std::array<float, 3>, Matrix<float, 4, 4>, Mat4ArraySig>;
		
		Shader() = default;

		friend class Blaster::Independent::ECS::ComponentFactory;
		friend class boost::serialization::access;

		template <class Archive>
		void serialize(Archive& archive, const unsigned)
		{
			archive& boost::serialization::base_object<Component>(*this);

			archive& boost::serialization::make_nvp("id", id);
			archive& boost::serialization::make_nvp("name", name);
			archive& boost::serialization::make_nvp("localPath", localPath);
			archive& boost::serialization::make_nvp("isGenerated", isGenerated);
			archive& boost::serialization::make_nvp("vertexPath", vertexPath);
			archive& boost::serialization::make_nvp("fragmentPath", fragmentPath);
			archive& boost::serialization::make_nvp("vertexData", vertexData);
			archive& boost::serialization::make_nvp("fragmentData", fragmentData);
		}

		void Generate()
		{
			if (isGenerated)
				return;

			const GLuint vertex = glCreateShader(GL_VERTEX_SHADER);

			{
				const char* source = vertexData.c_str();

				glShaderSource(vertex, 1, &source, nullptr);
				glCompileShader(vertex);

				GLint status;

				glGetShaderiv(vertex, GL_COMPILE_STATUS, &status);

				if (status != GL_TRUE)
				{
					char buffer[512];

					glGetShaderInfoLog(vertex, 512, nullptr, buffer);

					std::cerr << "[Shader] Vertex compile error: " << buffer << "\n";
				}
			}

			const GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);

			{
				const char* source = fragmentData.c_str();

				glShaderSource(fragment, 1, &source, nullptr);
				glCompileShader(fragment);

				GLint status;

				glGetShaderiv(fragment, GL_COMPILE_STATUS, &status);

				if (status != GL_TRUE)
				{
					char buffer[512];

					glGetShaderInfoLog(fragment, 512, nullptr, buffer);

					std::cerr << "[Shader] Fragment compile error: " << buffer << "\n";
				}
			}

			const GLuint program = glCreateProgram();

			glAttachShader(program, vertex);
			glAttachShader(program, fragment);
			glLinkProgram(program);

			{
				GLint status;

				glGetProgramiv(program, GL_LINK_STATUS, &status);

				if (status != GL_TRUE)
				{
					char buffer[512];

					glGetProgramInfoLog(program, 512, nullptr, buffer);

					std::cerr << "[Shader] Link error: " << buffer << "\n";
				}
			}

			glDeleteShader(vertex);
			glDeleteShader(fragment);

			if (id != 0)
				glDeleteProgram(id);

			id = program;

			isGenerated = true;

			uniformLocationCache.clear();
			uniformValueCache.clear();
		}

		GLint GetUniformLocationCached(const std::string& name) const
		{
			auto it = uniformLocationCache.find(name);

			if (it != uniformLocationCache.end())
				return it->second;

			const GLint location = glGetUniformLocation(id, name.c_str());

			uniformLocationCache.emplace(name, location);

			return location;
		}

		bool IsSame(const std::string& name, const CacheValue& v) const
		{
			auto iterator = uniformValueCache.find(name);

			if (iterator == uniformValueCache.end())
				return false;

			return iterator->second == v;
		}

		void UpdateCache(const std::string& name, CacheValue&& v) const
		{
			uniformValueCache[name] = std::move(v);
		}

		static std::string NormalizeArrayName(const std::string& name)
		{
			if (name.find('[') == std::string::npos)
				return name + "[0]";

			return name;
		}

		static uint64_t FNV1a(const uint8_t* data, size_t len)
		{
			uint64_t h = 14695981039346656037ull;

			for (size_t i = 0; i < len; ++i)
			{
				h ^= data[i];
				h *= 1099511628211ull;
			}

			return h;
		}

		unsigned int id { 0 };

		std::string name;

		AssetPath localPath;

		bool isGenerated{};

		AssetPath vertexPath, fragmentPath;
		std::string vertexData, fragmentData;

		mutable std::unordered_map<std::string, GLint> uniformLocationCache;
		mutable std::unordered_map<std::string, CacheValue> uniformValueCache;

		static inline GLuint sCurrentlyBoundProgram = 0;

		DESCRIBE_AND_REGISTER(Shader, (Component), (), (), (id, name, localPath, isGenerated, vertexPath, fragmentPath, vertexData, fragmentData))

	};
}

REGISTER_COMPONENT(Blaster::Client::Render::Shader, 39247)