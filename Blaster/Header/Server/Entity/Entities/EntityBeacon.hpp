#pragma once

#include "Client/Render/Model.hpp"
#include "Server/Entity/EntityBase.hpp"

namespace Blaster::Server::Entity::Entities
{
	class EntityBeacon final : public EntityBase
	{

	public:

		EntityBeacon(const EntityBeacon&) = delete;
		EntityBeacon(EntityBeacon&&) = delete;
		EntityBeacon& operator=(const EntityBeacon&) = delete;
		EntityBeacon& operator=(EntityBeacon&&) = delete;

		void Initialize() override
		{
			if (GetGameObject()->IsAuthoritative())
			{
				currentHealth = GetMaximumHealth();

				switch (team)
				{
				case Team::RED:
					GetGameObject()->GetTransform3d()->SetLocalPosition({ 420.0f, -190.0f, 15.0f });
					break;

				case Team::BLUE:
					GetGameObject()->GetTransform3d()->SetLocalPosition({ -420.0f, -190.0f, 15.0f });
					break;

				case Team::NONE:
					break;

				default:
					break;
				}
			} 
			else
			{
				modelGameObject = GameObjectManager::GetInstance().Register(GameObject::Create("model"), GetGameObject()->GetAbsolutePath());

				modelGameObject->AddComponent(Model::Create({ "Blaster", "Model/Radio.fbx" }));
			}
		}

		void Update() override
		{
			if (currentHealth <= 0)
				GameObjectManager::GetInstance().UnregisterDeferred(GetGameObject()->GetAbsolutePath());
		}

		std::string GetRegistryName() const override
		{
			return "entity_beacon";
		}

		Team GetTeam() const override
		{
			return team;
		}

		std::uint8_t GetCurrentHealth() const override
		{
			return currentHealth;
		}

		std::uint8_t GetMaximumHealth() const override
		{
			return 50.0f;
		}

		void DealDamage(std::uint8_t damage) override
		{
			if ((int(currentHealth) - damage) <= 0)
				currentHealth = 0;
			else
				currentHealth -= abs(damage);

			Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(GetGameObject(), typeid(EntityBeacon));
		}

		void HealDamage(std::uint8_t damage) override
		{
			currentHealth += abs(damage);

			Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(GetGameObject(), typeid(EntityBeacon));
		}

		std::optional<std::shared_ptr<GameObject>> GetEntityModel() const override
		{
			return modelGameObject;
		}

		static std::shared_ptr<EntityBeacon> Create(const Team& team)
		{
			std::shared_ptr<EntityBeacon> result(new EntityBeacon());

			result->team = team;

			return result;
		}

	private:

		EntityBeacon() = default;

		friend class boost::serialization::access;
		friend class Blaster::Independent::ECS::ComponentFactory;

		template <typename Archive>
		void serialize(Archive& archive, const unsigned)
		{
			archive & boost::serialization::base_object<Component>(*this);

			archive & BOOST_SERIALIZATION_NVP(team);
			archive & BOOST_SERIALIZATION_NVP(currentHealth);
		}

		Team team;
		std::uint8_t currentHealth;

		std::shared_ptr<GameObject> modelGameObject;

		DESCRIBE_AND_REGISTER(EntityBeacon, (EntityBase), (), (), (team, currentHealth))

	};
}

REGISTER_COMPONENT(Blaster::Server::Entity::Entities::EntityBeacon, 19381)