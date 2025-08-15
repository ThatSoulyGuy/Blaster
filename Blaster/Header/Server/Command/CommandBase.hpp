#pragma once

#include <string>
#include "Server/Command/CommandAuthority.hpp"
#include "Server/Command/CommandParser.hpp"
#include "Server/Command/CommandSender.hpp"

namespace Blaster::Server::Command
{
	class CommandBase
	{

	public:

		virtual ~CommandBase() { }

		virtual std::string GetRegistryName() const = 0;
		virtual std::string GetDescription() const = 0;
		virtual CommandAuthority GetRequiredAuthority() const = 0;
		virtual int Run(const CommandSender&, const CommandDescriptor&) = 0;

	};
}