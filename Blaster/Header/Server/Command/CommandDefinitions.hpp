#pragma once

#include "Server/Command/CommandBase.hpp"
#include "Server/Network/ServerNetwork.hpp"

using namespace Blaster::Server::Network;

namespace Blaster::Server::Command
{
	class CommandSpeak final : public CommandBase
	{

	public:

		CommandSpeak(const CommandSpeak&) = delete;
		CommandSpeak(CommandSpeak&&) = delete;
		CommandSpeak& operator=(const CommandSpeak&) = delete;
		CommandSpeak& operator=(CommandSpeak&&) = delete;

		[[nodiscard]]
		std::string GetRegistryName() const override
		{
			return "speak";
		}

		[[nodiscard]]
		std::string GetDescription() const override
		{
			return "/speak (String) - Says something in chat";
		}

		[[nodiscard]]
		CommandAuthority GetRequiredAuthority() const override
		{
			return CommandAuthority::PLAYER;
		}

		int Run(const CommandSender& sender, const CommandDescriptor& descriptor) override
		{
			if (descriptor.argumentList.size() != 1)
				return -1;

			if (!descriptor.argumentList.front().IsString())
				return -2;

			for (NetworkId id : ServerNetwork::GetInstance().GetConnectedClients())
				ServerNetwork::GetInstance().SendTo(id, PacketType::S2C_Chat, "[" + sender.GetSenderName() + "]: " + descriptor.argumentList.front().AsString());

			return 0;
		}

		static std::shared_ptr<CommandSpeak> Create()
		{
			return std::shared_ptr<CommandSpeak>(new CommandSpeak());
		}

	private:

		CommandSpeak() = default;

	};
}