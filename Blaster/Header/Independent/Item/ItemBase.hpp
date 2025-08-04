#pragma once

#include <string>
#include <optional>
#include "Independent/Utility/AssetPath.hpp"

using namespace Blaster::Independent::Utility;

namespace Blaster::Independent::Item
{
	class ItemBase
	{

	public:

		virtual ~ItemBase() { }

		ItemBase() : id(++nextId) { }

		[[nodiscard]]
		virtual std::string GetRegistryName() const = 0;

		[[nodiscard]]
		virtual std::string GetDisplayName() const = 0;

		[[nodiscard]]
		virtual std::string GetTextureName() const = 0;

		[[nodiscard]]
		virtual std::optional<AssetPath> GetModelPath() const = 0;

		[[nodiscard]]
		std::uint32_t GetId() const
		{
			return id;
		}
		
	private:
		
		std::uint32_t id;

		inline static std::uint32_t nextId = -1;

	};
}