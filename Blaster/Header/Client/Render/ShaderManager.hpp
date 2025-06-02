#pragma once

#include "Client/Render/Shader.hpp"
#include "Independent/Utility/SingletonManager.hpp"

namespace Blaster::Client::Render
{
	class ShaderManager final : public SingletonManager<std::shared_ptr<Shader>, const std::string&>
	{

	public:

		ShaderManager(const ShaderManager&) = delete;
		ShaderManager(ShaderManager&&) = delete;
		ShaderManager& operator=(const ShaderManager&) = delete;
		ShaderManager& operator=(ShaderManager&&) = delete;

		std::shared_ptr<Shader> Register(std::shared_ptr<Shader> object) override
		{
			auto name = object->GetName();

			if (shaderMap.contains(name))
			{
				std::cerr << "Shader map already has shader '" << name << "'!";
				return nullptr;
			}

			shaderMap.insert({ name, std::move(object) });

			return shaderMap[name];
		}

		void Unregister(const std::string& name) override
		{
			if (!shaderMap.contains(name))
			{
				std::cerr << "Shader map doesn't have shader '" << name << "'!";
				return;
			}

			shaderMap.erase(name);
		}

		bool Has(const std::string& name) const override
		{
			return shaderMap.contains(name);
		}

		std::optional<std::shared_ptr<Shader>> Get(const std::string& name) override
		{
			if (!shaderMap.contains(name))
			{
				std::cerr << "Shader map doesn't have shader '" << name << "'!";
				return std::nullopt;
			}

			return std::make_optional(shaderMap[name]);
		}

		std::vector<std::shared_ptr<Shader>> GetAll() const override
		{
			std::vector<std::shared_ptr<Shader>> result(shaderMap.size());

			std::ranges::transform(shaderMap, result.begin(), [](const auto& pair) { return pair.second; });

			return result;
		}

		static ShaderManager& GetInstance()
		{
			std::call_once(initializationFlag, [&]()
			{
				instance = std::unique_ptr<ShaderManager>(new ShaderManager());
			});

			return *instance;
		}

	private:

		ShaderManager() = default;

		std::unordered_map<std::string, std::shared_ptr<Shader>> shaderMap;

		static std::once_flag initializationFlag;
		static std::unique_ptr<ShaderManager> instance;

	};

	std::once_flag ShaderManager::initializationFlag;
	std::unique_ptr<ShaderManager> ShaderManager::instance;
}