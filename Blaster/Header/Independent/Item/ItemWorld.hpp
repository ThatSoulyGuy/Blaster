#pragma once

#include "Independent/Utility/Time.hpp"
#include "Server/Entity/Entities/EntityPlayer.hpp"
#include "Server/Network/ServerNetwork.hpp"

using namespace Blaster::Server::Entity::Entities;

namespace Blaster::Independent::Item
{
	class ItemWorld final : public Component
	{

	public:

		ItemWorld(const ItemWorld&) = delete;
		ItemWorld(ItemWorld&&) = delete;
		ItemWorld& operator=(const ItemWorld&) = delete;
		ItemWorld& operator=(ItemWorld&&) = delete;

		void Initialize() override
		{
#ifndef IS_SERVER
			if (auto transform = GetGameObject()->GetTransform3d())
			{
				basePosition = transform->GetLocalPosition();
				baseYaw = transform->GetLocalRotation().y();

				baseCaptured = true;
			}
#endif
		}

		void Update() override
		{
#ifndef IS_SERVER
			AnimateVisual();
#else
			if (used)
				return;

			auto itemTransform = GetGameObject()->GetTransform3d();
			if (!itemTransform)
				return;

			const Vector<float, 3> itemPosition = itemTransform->GetWorldPosition();
			constexpr float pickupDistanceSquared = 3.0f * 3.0f;

			for (const auto& clientId : Blaster::Server::Network::ServerNetwork::GetInstance().GetConnectedClients())
			{
				auto clientReferenceOptional = Blaster::Server::Network::ServerNetwork::GetInstance().GetClient(clientId);

				if (!clientReferenceOptional.has_value())
					continue;

				const auto& clientReference = clientReferenceOptional.value();

				for (const auto& gameObjectDecayed : clientReference->ownedGameObjectList | std::views::values)
				{
					auto gameObject = std::dynamic_pointer_cast<GameObject>(gameObjectDecayed.lock());

					if (!gameObject || !gameObject->HasComponent<EntityPlayer>())
						continue;

					auto playerTransform = gameObject->GetTransform3d();

					if (!playerTransform)
						continue;

					const Vector<float, 3> playerPosition = playerTransform->GetWorldPosition();

					if (Vector<float, 3>::LengthSquared(playerPosition - itemPosition) < pickupDistanceSquared)
					{
						gameObject->GetComponent<EntityPlayer>().value()->AddItem(itemId);
						GameObjectManager::GetInstance().UnregisterDeferred(GetGameObject()->GetAbsolutePath());

						used = true;

						return;
					}
				}
			}
#endif
		}

		static std::shared_ptr<ItemWorld> Create(std::uint8_t itemId)
		{
			std::shared_ptr<ItemWorld> result = std::shared_ptr<ItemWorld>(new ItemWorld());

			result->itemId = itemId;

			return result;
		}

	private:

		ItemWorld() = default;

		friend class boost::serialization::access;
		friend class Blaster::Independent::ECS::ComponentFactory;

		template <typename Archive>
		void serialize(Archive& archive, const unsigned)
		{
			archive & boost::serialization::base_object<Component>(*this);

			archive & BOOST_SERIALIZATION_NVP(itemId);
			archive & BOOST_SERIALIZATION_NVP(used);
		}

#ifndef IS_SERVER
		void AnimateVisual()
		{
			auto transform = GetGameObject()->GetTransform3d();

			if (!transform)
				return;

			if (!baseCaptured)
			{
				basePosition = transform->GetLocalPosition();
				baseYaw = transform->GetLocalRotation().y();
				baseCaptured = true;
			}

			animTime += Blaster::Independent::Utility::Time::GetInstance().GetDeltaTime();

			const float phase = animTime * bobHz * 2.0f * std::numbers::pi_v<float>;

			Vector<float, 3> position = basePosition;
			position.y() += std::sin(phase) * bobAmplitude;

			Vector<float, 3> rotation = transform->GetLocalRotation();

			rotation.y() = std::fmod(baseYaw + spinDegPerSec * animTime, 360.0f);

			if (rotation.y() < 0.0f)
				rotation.y() += 360.0f;

			transform->SetLocalPosition(position, false);
			transform->SetLocalRotation(rotation, false);
		}

		float bobAmplitude = 0.5f;
		float bobHz = 1.0f;
		float spinDegPerSec = 90.0f;

		float animTime = 0.0f;

		Vector<float, 3> basePosition{ 0.0f, 0.0f, 0.0f };

		float baseYaw = 0.0f;
		bool baseCaptured = false;
#endif

		std::uint8_t itemId;
		bool used = false;

		DESCRIBE_AND_REGISTER(ItemWorld, (Component), (), (), (itemId, used))
	};
}

REGISTER_COMPONENT(Blaster::Independent::Item::ItemWorld, 10381)