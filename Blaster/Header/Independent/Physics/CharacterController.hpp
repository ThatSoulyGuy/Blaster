#pragma once

#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include "Independent/ECS/ComponentFactory.hpp"
#include "Independent/Physics/PhysicsBody.hpp"
#include "Independent/Physics/PhysicsCommands.hpp"

using namespace Blaster::Independent::ECS;

namespace Blaster::Independent::Physics
{
	class CharacterController final : public PhysicsBody
	{
		
	public:

		~CharacterController()
		{
			auto* world = PhysicsWorld::GetInstance().GetHandle();

			if (kinematicCharacterController)
				world->removeAction(kinematicCharacterController);

			if (ghost)
				world->removeCollisionObject(ghost);

			delete kinematicCharacterController;
			kinematicCharacterController = nullptr;

			delete ghost;
			ghost = nullptr;

			delete shape;
			shape = nullptr;
		}

		CharacterController(const CharacterController&) = delete;
		CharacterController(CharacterController&&) = delete;
		CharacterController& operator=(const CharacterController&) = delete;
		CharacterController& operator=(CharacterController&&) = delete;

		void Initialize() override
		{
#ifndef IS_SERVER
			if (!GetGameObject()->IsLocallyControlled())
				GetGameObject()->SetLocal(true);

			GetGameObject()->GetTransform3d()->SetShouldSynchronize(false);
#endif

			float total = height;
			float cyl = std::max(0.f, total - 2.f * radius);

			shape = new btCapsuleShape(radius, cyl);

			ghost = new btPairCachingGhostObject();

			ghost->setCollisionShape(shape);
			ghost->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
			ghost->setUserPointer(this);

			const auto worldPosition = GetGameObject()->GetTransform3d()->GetWorldPosition();

			btTransform start;
			
			start.setIdentity();

			start.setOrigin({ worldPosition.x(), worldPosition.y(), worldPosition.z() });

			ghost->setWorldTransform(start);

			kinematicCharacterController = new btKinematicCharacterController(ghost, shape, stepHeight);

			kinematicCharacterController->setMaxJumpHeight(jumpSpeed * jumpSpeed / (2 * gravity));
			kinematicCharacterController->setGravity(btVector3(0.0f, gravity, 0.0f));
			kinematicCharacterController->setMaxSlope(btRadians(maxSlopeDeg));

			auto* world = PhysicsWorld::GetInstance().GetHandle();

			world->addCollisionObject(ghost, btBroadphaseProxy::CharacterFilter, btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);
			world->addAction(kinematicCharacterController);
		}

		void SetWalkDirection(const Vector<float, 3>& directionNormalized)
		{
			requestedDirection = directionNormalized;

			QueueToServer(PacketType::C2S_CharacterController_Input, CharacterControllerInputCommand{ GetGameObject()->GetAbsolutePath(), false, directionNormalized });
		}

		void Jump()
		{
			wantJump = true;

			QueueToServer(PacketType::C2S_CharacterController_Input, CharacterControllerInputCommand{ GetGameObject()->GetAbsolutePath(), true, { 0.0f, 0.0f, 0.0f } });
		}

		bool OnGround() const
		{
			return kinematicCharacterController ? kinematicCharacterController->onGround() : false;
		}

		void Warp(const Vector<float, 3>& position)
		{
			if (kinematicCharacterController)
				kinematicCharacterController->warp({ position.x(), position.y(), position.z() });
		}

		void Update() override
		{
			if (!IsAuthoritative())
				return;

			if (kinematicCharacterController)
			{
				btVector3 walk = { requestedDirection.x(), requestedDirection.y(), requestedDirection.z() };

				kinematicCharacterController->setWalkDirection(walk * PhysicsWorld::GetInstance().GetHandle()->getSolverInfo().m_timeStep);

				if (wantJump && kinematicCharacterController->canJump())
					kinematicCharacterController->jump({ 0, jumpSpeed, 0 });

				wantJump = false;
			}
		}

		void SyncToBullet() override
		{
			if (!IsAuthoritative())
			{
				btTransform transform;

				transform.setIdentity();

				const auto p = GetGameObject()->GetTransform3d()->GetWorldPosition();

				transform.setOrigin({ p.x(), p.y(), p.z() });

				ghost->setWorldTransform(transform);
			}
		}

		void SyncFromBullet() override
		{
			if (!IsAuthoritative())
				return;

			const btTransform& t = ghost->getWorldTransform();
			const btVector3& o = t.getOrigin();

			GetGameObject()->GetTransform3d()->SetLocalPosition({ o.x(), o.y(), o.z() }, false);
		}

		btCollisionObject* GetCollisionObject() const override
		{
			return ghost;
		}

		bool IsAuthoritative() const noexcept override
		{
#ifdef IS_SERVER
			return true;
#else
			return GetGameObject()->IsLocallyControlled();
#endif
		}

		static std::shared_ptr<CharacterController> Create(float radius = 0.6f, float height = 1.8f, float stepHeight = 0.4f, float maxSlopeDeg = 45.0f, float jumpSpeed = 10.0f, float gravity = -9.81f)
		{
			std::shared_ptr<CharacterController> result(new CharacterController());

			result->radius = radius;
			result->height = height;
			result->stepHeight = stepHeight;
			result->maxSlopeDeg = maxSlopeDeg;
			result->jumpSpeed = jumpSpeed;
			result->gravity = gravity;

			return result;
		}
	
	private:

		CharacterController() = default;

		friend class boost::serialization::access;
		friend class Blaster::Independent::ECS::ComponentFactory;

		template <typename Archive>
		void serialize(Archive& archive, const unsigned)
		{
			archive & boost::serialization::base_object<Component>(*this);

			archive & radius;
			archive & height;
			archive & stepHeight;
			archive & maxSlopeDeg;
			archive & jumpSpeed;
			archive & gravity;
		}

		btPairCachingGhostObject* ghost{ nullptr };
		btConvexShape* shape{ nullptr };
		btKinematicCharacterController* kinematicCharacterController{ nullptr };

		Vector<float, 3> requestedDirection{ 0,0,0 };
		bool wantJump{ false };

		float radius;
		float height;
		float stepHeight;
		float maxSlopeDeg;
		float jumpSpeed;
		float gravity;

		DESCRIBE_AND_REGISTER(CharacterController, (PhysicsBody), (), (), ())

	};
}

REGISTER_COMPONENT(Blaster::Independent::Physics::CharacterController, 12963)