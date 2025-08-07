#pragma once

#include <string>
#include <optional>
#include "Independent/Utility/AssetPath.hpp"

#ifdef _MSC_VER
#define REGISTER_ITEM(type) [[maybe_unused]] \
inline static bool registered_ = [] \
	{ \
		ItemRegistry::GetInstance().Register(std::make_shared<type>()); \
 \
		return true; \
	}();
#else
#define REGISTER_ITEM(type) [[gnu::used]] \
inline static bool registered_ = [] \
{ \
ItemRegistry::GetInstance().Register(std::make_shared<type>()); \
\
return true; \
}();
#endif

using namespace Blaster::Independent::Utility;

namespace Blaster::Client::Core
{
	enum class MouseCode;
}

namespace Blaster::Independent::Item
{
	class ItemBase
	{

	public:

		virtual ~ItemBase() { }

		ItemBase() : id(++nextId) { }

		virtual void OnUsed(void*, void*, const MouseCode&) { }

		[[nodiscard]]
		virtual std::string GetRegistryName() const = 0;

		[[nodiscard]]
		virtual std::string GetDisplayName() const = 0;

		[[nodiscard]]
		virtual std::string GetTextureName() const = 0;

		[[nodiscard]]
		virtual std::optional<AssetPath> GetModelPath() const = 0;

		[[nodiscard]]
		virtual std::optional<Vector<float, 3>> GetModelViewPosition() const = 0;

		[[nodiscard]]
		virtual std::optional<Vector<float, 3>> GetModelViewRotation() const = 0;

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