#pragma once

namespace Blaster::Server::Command
{
	enum class CommandAuthority : uint8_t
	{
		SERVER = 4,
		ADMINISTRATOR = 3,
		PLAYER = 2,
		NONE = 1
	};
}