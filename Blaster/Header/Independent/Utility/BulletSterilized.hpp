#pragma once

#ifdef BT_BULLET_DYNAMICS_COMMON_H
#error "Bullet headers must be included via BulletConfig.hpp only."
#endif

#if defined(NodeArray)
#define BLASTER_RESTORE_NODEARRAY 1
#pragma push_macro("NodeArray")
#undef NodeArray
#endif
#define NodeArray Bullet3_NodeArray

#include <btBulletDynamicsCommon.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#undef NodeArray
#if defined(BLASTER_RESTORE_NODEARRAY)
#pragma pop_macro("NodeArray")
#undef BLASTER_RESTORE_NODEARRAY
#endif

static_assert(sizeof(btScalar) == sizeof(float), "Bullet was compiled with double precision but app uses float. " "Turn OFF USE_DOUBLE_PRECISION for Bullet and the app.");