#pragma once

#include <unordered_map>
#include <cstdint>
#include <tuple>
#include <functional>
#include <cstddef>
#include <mutex>
#include "Independent/Item/ItemBase.hpp"

namespace Blaster::Independent::Item
{
	template <typename T>
	concept ItemKeyType = std::same_as<T, std::string> || std::same_as<T, std::uint32_t>;

	class ItemRegistry final
	{

	public:

		ItemRegistry(const ItemRegistry&) = delete;
		ItemRegistry(ItemRegistry&&) = delete;
		ItemRegistry& operator=(const ItemRegistry&) = delete;
		ItemRegistry& operator=(ItemRegistry&&) = delete;

		void Register(std::shared_ptr<ItemBase> object)
		{
			itemMapByName.emplace(object->GetRegistryName(), object);
			itemMapById.emplace(object->GetId(), object);
		}

		template <ItemKeyType T>
		void Unregister(const T& key)
		{
			if constexpr (std::is_same_v<T, std::string>)
				itemMapByName.erase(key);
			else
				itemMapById.erase(key);
		}

		template<ItemKeyType T>
		[[nodiscard]]
		std::optional<std::shared_ptr<ItemBase>> Get(const T& key) const noexcept
		{
			if constexpr (std::is_same_v<T, std::string>)
			{
				if (auto iterator = itemMapByName.find(key); iterator != itemMapByName.end())
					return std::make_optional<std::shared_ptr<ItemBase>>(iterator->second);
			}
			else
			{
				if (auto iterator = itemMapById.find(key); iterator != itemMapById.end())
					return std::make_optional<std::shared_ptr<ItemBase>>(iterator->second);
			}
			
			return std::nullopt;
		}

		static ItemRegistry& GetInstance()
		{
			std::call_once(initializationFlag, [&]()
				{
					instance = std::unique_ptr<ItemRegistry>(new ItemRegistry());
				});

			return *instance;
		}

	private:

		ItemRegistry() = default;

		std::unordered_map<std::uint32_t, std::shared_ptr<ItemBase>> itemMapById;
		std::unordered_map<std::string, std::shared_ptr<ItemBase>> itemMapByName;

		static std::once_flag initializationFlag;
		static std::unique_ptr<ItemRegistry> instance;

	};

	std::once_flag ItemRegistry::initializationFlag;
	std::unique_ptr<ItemRegistry> ItemRegistry::instance;
}