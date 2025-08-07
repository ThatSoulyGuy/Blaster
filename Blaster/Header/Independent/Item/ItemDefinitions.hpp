#pragma once

#include "Client/Core/InputManager.hpp"
#include "Client/Network/ClientNetwork.hpp"
#include "Independent/Item/ItemRegistry.hpp"
#include "Server/Entity/Entities/EntityCommands.hpp"
#include "Server/Entity/EntityBase.hpp"

using namespace Blaster::Client::Core;
using namespace Blaster::Client::Network;
using namespace Blaster::Server::Entity;

namespace Blaster::Independent::Item
{
	class ItemAir final : public ItemBase
	{

	public:

		ItemAir() = default;

		std::string GetRegistryName() const override
		{
			return "item_air";
		}

		std::string GetDisplayName() const override
		{
			return "Air";
		}

		std::string GetTextureName() const override
		{
			return "blaster.item.resource_empty";
		}

		std::optional<AssetPath> GetModelPath() const override
		{
			return std::nullopt;
		}

		std::optional<Vector<float, 3>> GetModelViewPosition() const override
		{
			return std::nullopt;
		}

		std::optional<Vector<float, 3>> GetModelViewRotation() const override
		{
			return std::nullopt;
		}

	private:

		REGISTER_ITEM(ItemAir)

	};

	class ItemAssaultRifle final : public ItemBase
	{

	public:

		ItemAssaultRifle() = default;

		void OnUsed(void* interator, void* interactee, const MouseCode& code) override
		{
			if (auto player = static_cast<EntityBase*>(interactee); code == MouseCode::LEFT && player)
				Blaster::Client::Network::ClientNetwork::GetInstance().Send(PacketType::C2S_EntityPlayer_Damage, Entities::DamageCommand{ player->GetGameObject()->GetAbsolutePath(), true, 5.0f });
		}

		std::string GetRegistryName() const override
		{
			return "item_assault_rifle";
		}

		std::string GetDisplayName() const override
		{
			return "Assault Rifle";
		}

		std::string GetTextureName() const override
		{
			return "blaster.item.weapon_assault_rifle";
		}

		std::optional<AssetPath> GetModelPath() const override
		{
			return std::make_optional<AssetPath>({ "Blaster", "Model/AssaultRifle.fbx" });
		}

		std::optional<Vector<float, 3>> GetModelViewPosition() const override
		{
			return std::make_optional<Vector<float, 3>>({ -3.0f, -1.0f, 6.0f });
		}

		std::optional<Vector<float, 3>> GetModelViewRotation() const override
		{
			return std::make_optional<Vector<float, 3>>({ 0.0f, 15.0f, 10.0f });
		}

	private:

		REGISTER_ITEM(ItemAssaultRifle)

	};

}