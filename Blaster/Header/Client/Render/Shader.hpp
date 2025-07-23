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
			const GLint location = glGetUniformLocation(id, name.c_str());

			glUniform1i(location, value);
		}

		void SetUniform(const std::string& name, const bool value) const
		{
			SetUniform(name, static_cast<int>(value));
		}

		void SetUniform(const std::string& name, const float value) const
		{
			const GLint location = glGetUniformLocation(id, name.c_str());

			glUniform1f(location, value);
		}

		void SetUniform(const std::string& name, const Vector<float, 2>& value) const
		{
			const GLint location = glGetUniformLocation(id, name.c_str());

			glUniform2f(location, value[0], value[1]);
		}

		void SetUniform(const std::string& name, const Vector<float, 3>& value) const
		{
			const GLint location = glGetUniformLocation(id, name.c_str());

			glUniform3f(location, value[0], value[1], value[2]);
		}

		void SetUniform(const std::string& name, const Matrix<float, 4, 4>& value) const
		{
			const GLint location = glGetUniformLocation(id, name.c_str());

			glUniformMatrix4fv(location, 1, GL_FALSE, &value[0][0]);
		}

		void SetUniform(const std::string& name, const std::vector<Matrix<float, 4, 4>>& value) const
		{
			std::string query = name;

			if (query.find('[') == std::string::npos)
				query += "[0]";

			const auto data = value.data();
			const auto count = value.size();

			if (!data || count <= 0)
				return;

			const GLint location = glGetUniformLocation(id, query.c_str());

			if (location == -1)
				return;

			glUniformMatrix4fv(location, count, GL_FALSE, reinterpret_cast<const GLfloat*>(&data[0][0]));
		}

		[[nodiscard]]
		std::string GetName() const
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
		
		Shader() = default;

		friend class Blaster::Independent::ECS::ComponentFactory;
		friend class boost::serialization::access;

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
		}

		template <class Archive>
		void serialize(Archive& archive, const unsigned)
		{
			archive & boost::serialization::base_object<Component>(*this);

			archive & boost::serialization::make_nvp("id", id);
			archive & boost::serialization::make_nvp("name", name);
			archive & boost::serialization::make_nvp("localPath", localPath);
			archive & boost::serialization::make_nvp("isGenerated", isGenerated);
			archive & boost::serialization::make_nvp("vertexPath", vertexPath);
			archive & boost::serialization::make_nvp("fragmentPath", fragmentPath);
			archive & boost::serialization::make_nvp("vertexData", vertexData);
			archive & boost::serialization::make_nvp("fragmentData", fragmentData);
		}

		unsigned int id { 0 };

		std::string name;

		AssetPath localPath;

		bool isGenerated{};

		AssetPath vertexPath, fragmentPath;
		std::string vertexData, fragmentData;

		DESCRIBE_AND_REGISTER(Shader, (Component), (), (), (id, name, localPath, isGenerated, vertexPath, fragmentPath, vertexData, fragmentData))

	};
}

REGISTER_COMPONENT(Blaster::Client::Render::Shader, 39247)