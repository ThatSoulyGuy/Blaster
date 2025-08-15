#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <algorithm>
#include "Server/Command/CommandBase.hpp"
#include "Independent/Utility/SingletonManager.hpp"

using namespace Blaster::Independent::Utility;

namespace Blaster::Server::Command
{
	class CommandManager final : public SingletonManager<std::shared_ptr<CommandBase>, const std::string&>
	{

	public:

		CommandManager(const CommandManager&) = delete;
		CommandManager(CommandManager&&) = delete;
		CommandManager& operator=(const CommandManager&) = delete;
		CommandManager& operator=(CommandManager&&) = delete;

		std::shared_ptr<CommandBase> Register(std::shared_ptr<CommandBase> object) override
		{
			auto name = object->GetRegistryName();

			if (commandMap.contains(name))
			{
				std::cerr << "Command map already has command '" << name << "'!";
				return nullptr;
			}

			commandMap.insert({ name, std::move(object) });

			return commandMap[name];
		}

		void Unregister(const std::string& name) override
		{
			if (!commandMap.contains(name))
			{
				std::cerr << "Command map doesn't have command '" << name << "'!";
				return;
			}

			commandMap.erase(name);
		}

		bool Has(const std::string& name) const override
		{
			return commandMap.contains(name);
		}

		std::optional<std::shared_ptr<CommandBase>> Get(const std::string& name) override
		{
			if (!commandMap.contains(name))
			{
				std::cerr << "Command map doesn't have command '" << name << "'!";
				return std::nullopt;
			}

			return std::make_optional(commandMap[name]);
		}

		std::vector<std::shared_ptr<CommandBase>> GetAll() const override
		{
			std::vector<std::shared_ptr<CommandBase>> result(commandMap.size());

			std::ranges::transform(commandMap, result.begin(), [](const auto& pair) { return pair.second; });

			return result;
		}

		static CommandManager& GetInstance()
		{
			std::call_once(initializationFlag, [&]()
			{
				instance = std::unique_ptr<CommandManager>(new CommandManager());
			});

			return *instance;
		}

	private:

		CommandManager() = default;

		std::unordered_map<std::string, std::shared_ptr<CommandBase>> commandMap;

		static std::once_flag initializationFlag;
		static std::unique_ptr<CommandManager> instance;

	};

	std::once_flag CommandManager::initializationFlag;
	std::unique_ptr<CommandManager> CommandManager::instance;
}