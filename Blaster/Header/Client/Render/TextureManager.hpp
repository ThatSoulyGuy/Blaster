#pragma once

#include <iostream>
#include "Client/Render/Texture.hpp"
#include "Independent/Utility/SingletonManager.hpp"

namespace Blaster::Client::Render
{
	class TextureManager final : public SingletonManager<std::shared_ptr<Texture>, const std::string&>
	{

	public:

		TextureManager(const TextureManager&) = delete;
		TextureManager(TextureManager&&) = delete;
		TextureManager& operator=(const TextureManager&) = delete;
		TextureManager& operator=(TextureManager&&) = delete;

		std::shared_ptr<Texture> Register(std::shared_ptr<Texture> object) override
		{
			auto name = object->GetName();

			if (textureMap.contains(name))
			{
				std::cerr << "Texture map already has texture '" << name << "'!";
				return nullptr;
			}

			object->isRegistered = true;

			textureMap.insert({ name, std::move(object) });

			return textureMap[name];
		}

		void Unregister(const std::string& name) override
		{
			if (!textureMap.contains(name))
			{
				std::cerr << "Texture map doesn't have texture '" << name << "'!";
				return;
			}

			textureMap.erase(name);
		}

		bool Has(const std::string& name) const override
		{
			return textureMap.contains(name);
		}

		std::optional<std::shared_ptr<Texture>> Get(const std::string& name) override
		{
			if (!textureMap.contains(name))
			{
				std::cerr << "Texture map doesn't have texture '" << name << "'!";
				return std::nullopt;
			}

			return std::make_optional(textureMap[name]);
		}

		std::vector<std::shared_ptr<Texture>> GetAll() const override
		{
			std::vector<std::shared_ptr<Texture>> result(textureMap.size());

			std::ranges::transform(textureMap, result.begin(), [](const auto& pair) { return pair.second; });

			return result;
		}

		static TextureManager& GetInstance()
		{
			std::call_once(initializationFlag, [&]()
			{
				instance = std::unique_ptr<TextureManager>(new TextureManager());
			});

			return *instance;
		}

	private:

		TextureManager() = default;

		std::unordered_map<std::string, std::shared_ptr<Texture>> textureMap;

		static std::once_flag initializationFlag;
		static std::unique_ptr<TextureManager> instance;

	};

	std::once_flag TextureManager::initializationFlag;
	std::unique_ptr<TextureManager> TextureManager::instance;
}