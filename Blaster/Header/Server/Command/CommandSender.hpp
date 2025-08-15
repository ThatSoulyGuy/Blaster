#pragma once

#include <string>
#include "Server/Command/CommandAuthority.hpp"

namespace Blaster::Server::Command
{
	class CommandSender
	{

	public:

		virtual ~CommandSender() = default;

		virtual std::string GetSenderName() const = 0;
		virtual CommandAuthority GetAuthority() const = 0;

	};

	class CommandSenderServer final : public CommandSender
	{

	public:

		[[nodiscard]]
		std::string GetSenderName() const override
		{
			return "Server";
		}

		[[nodiscard]]
		CommandAuthority GetAuthority() const override
		{
			return CommandAuthority::SERVER;
		}

	};
}