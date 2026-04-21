#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/RegisterTypes.h>

#include <iostream>
#include <cstdarg>
#include <cstdio>
#include <memory>

#include "physics.h"
#include "../ecs/ecs.h"
#include "../script_manager.h"

Physics* Physics::instance = nullptr;

constexpr JPH::ObjectLayer NON_MOVING_LAYER = 0;
constexpr JPH::ObjectLayer MOVING_LAYER = 1;
constexpr JPH::BroadPhaseLayer NON_MOVING_BROAD_PHASE_LAYER(0);
constexpr JPH::BroadPhaseLayer MOVING_BROAD_PHASE_LAYER(1);
constexpr JPH::uint NUM_OBJECT_LAYERS = 2;
constexpr JPH::uint NUM_BROAD_PHASE_LAYERS = 2;

constexpr JPH::uint MAX_BODIES = 1024;
constexpr JPH::uint NUM_BODY_MUTEXES = 0;
constexpr JPH::uint MAX_BODY_PAIRS = 1024;
constexpr JPH::uint MAX_CONTACT_CONSTRAINTS = 1024;
constexpr JPH::uint MAX_PHYSICS_JOBS = 1024;
constexpr JPH::uint MAX_PHYSICS_BARRIERS = 1024;
constexpr std::size_t TEMP_ALLOCATOR_SIZE = 10 * 1024 * 1024;

void jolt_trace_impl(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char buff[1024];
	vsnprintf(buff, sizeof(buff), format, args);
	va_end(args);
	std::cout << "[Jolt Trace] " << buff << '\n';
}

#ifdef JPH_ENABLE_ASSERTS
	bool jolt_assert_failed_impl(const char* expr, const char* msg, const char* file, JPH::uint line)
	{
		std::cout << "[Jolt Assert] " << file << " : " << line << " (" << expr << ")";
		if (msg != nullptr)
			std::cout << " " << msg;
		std::cout << '\n';
		return true;
	}
#endif

JPH::Vec3 Physics::to_jolt(const glm::vec3 &val)
{
	return JPH::Vec3(val.x, val.y, val.z);
}

JPH::Quat Physics::to_jolt(const glm::quat &val)
{
	return JPH::Quat(val.x, val.y, val.z, val.w);
}

glm::vec3 Physics::to_glm(const JPH::Vec3 &val)
{
	return glm::vec3(val.GetX(), val.GetY(), val.GetZ());
}

glm::quat Physics::to_glm(const JPH::Quat &val)
{
	return glm::quat(val.GetW(), val.GetX(), val.GetY(), val.GetZ());
}

JPH::RefConst<JPH::Shape> Physics::create_shape(const ColliderShapeDesc &shapeDesc)
{
	if (shapeDesc.type == ColliderType::BOX)
	{
		BoxColliderDesc box = shapeDesc.boxCollider;
		JPH::RefConst<JPH::Shape> shape = new JPH::BoxShape(to_jolt(box.halfDimensions));
		return shape;
	}

	if (shapeDesc.type == ColliderType::SPHERE)
	{
		SphereColliderDesc sphere = shapeDesc.sphereCollider;
		JPH::RefConst<JPH::Shape> shape = new JPH::SphereShape(sphere.radius);
		return shape;
	}

	if (shapeDesc.type == ColliderType::CAPSULE)
	{
		CapsuleColliderDesc capsule = shapeDesc.capsuleCollider;
		JPH::RefConst<JPH::Shape> shape = new JPH::CapsuleShape(capsule.halfHeight, capsule.radius);
		return shape;
	}
}

JPH::RefConst<JPH::Shape> Physics::apply_local_offset(JPH::RefConst<JPH::Shape> shape, const glm::vec3 &positionOffset, const glm::quat &rotationOffset)
{
	if (positionOffset == glm::vec3(0.0f) && rotationOffset == glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
		return shape;
	
	return new JPH::RotatedTranslatedShape(to_jolt(positionOffset), to_jolt(rotationOffset), shape.GetPtr());
}

Physics::Physics() : objectLayerPairFilter(NUM_OBJECT_LAYERS), broadPhaseLayerInterface(NUM_OBJECT_LAYERS, NUM_BROAD_PHASE_LAYERS), tempAllocator(TEMP_ALLOCATOR_SIZE), jobSystem(MAX_PHYSICS_JOBS, MAX_PHYSICS_BARRIERS, -1), contactListener(*this)
{
	Physics::instance = this;

	JPH::Trace = jolt_trace_impl;

#ifdef JPH_ENABLE_ASSERTS
	JPH::AssertFailed = jolt_assert_failed_impl;
#endif

	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

	objectLayerPairFilter.EnableCollision(NON_MOVING_LAYER, MOVING_LAYER);
	objectLayerPairFilter.EnableCollision(MOVING_LAYER, MOVING_LAYER);

	broadPhaseLayerInterface.MapObjectToBroadPhaseLayer(NON_MOVING_LAYER, NON_MOVING_BROAD_PHASE_LAYER);
	broadPhaseLayerInterface.MapObjectToBroadPhaseLayer(MOVING_LAYER, MOVING_BROAD_PHASE_LAYER);
	
	objectVsBroadPhaseLayerFilter = std::make_unique<JPH::ObjectVsBroadPhaseLayerFilterTable>(broadPhaseLayerInterface, NUM_BROAD_PHASE_LAYERS, objectLayerPairFilter, NUM_OBJECT_LAYERS);

	physicsSystem.Init(MAX_BODIES, NUM_BODY_MUTEXES, MAX_BODY_PAIRS, MAX_CONTACT_CONSTRAINTS, broadPhaseLayerInterface, *objectVsBroadPhaseLayerFilter, objectLayerPairFilter);
	physicsSystem.SetContactListener(&contactListener);
	physicsSystem.SetGravity(JPH::Vec3(0.0f, -9.81, 0.0f));
}

Physics::~Physics()
{
	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
	Physics::instance = nullptr;
}

JPH::BodyID Physics::create_body(const BodyDesc &bodyDesc, EntityID entityID)
{
	JPH::RefConst<JPH::Shape> shape = new JPH::EmptyShape;

	JPH::BodyCreationSettings settings(shape.GetPtr(), to_jolt(bodyDesc.position), to_jolt(bodyDesc.rotation), bodyDesc.motionType, bodyDesc.physicsLayer);

	settings.mFriction = bodyDesc.friction;
	settings.mRestitution = bodyDesc.restitution;
	settings.mGravityFactor = bodyDesc.gravityFactor;
	settings.mIsSensor = bodyDesc.isSensor;

	JPH::EActivation activation = bodyDesc.motionType == JPH::EMotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate;

	if (bodyDesc.mass != 1.0f && bodyDesc.mass > 0 && bodyDesc.motionType == JPH::EMotionType::Dynamic)
	{
		settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
		settings.mMassPropertiesOverride.mMass = bodyDesc.mass;
	}

	JPH::BodyID id = physicsSystem.GetBodyInterface().CreateAndAddBody(settings, activation);
	bodyToEntity.emplace(id, entityID);

	return id;
}

void Physics::destroy_body(JPH::BodyID bodyID)
{
	if (bodyID.IsInvalid())
		return;

	auto& bodyInterface = physicsSystem.GetBodyInterface();	

	if (bodyInterface.IsAdded(bodyID))
		bodyInterface.RemoveBody(bodyID);
	bodyInterface.DestroyBody(bodyID);

	if (bodyToEntity.find(bodyID) != bodyToEntity.end())
		bodyToEntity.erase(bodyID);
}

void Physics::set_colliders(JPH::BodyID bodyID, const std::vector<ColliderShapeDesc> &colliderDescs)
{
	if (bodyID.IsInvalid())
		return;

	JPH::RefConst<JPH::Shape> finalShape;
	
	if (colliderDescs.size() == 0)
		finalShape = new JPH::EmptyShape();
	else if (colliderDescs.size() == 1)
	{
		finalShape = create_shape(colliderDescs[0]);
		finalShape = apply_local_offset(finalShape, colliderDescs[0].positionOffset, colliderDescs[0].rotationOffset);
	}
	else
	{
		JPH::Ref<JPH::MutableCompoundShape> compound = new JPH::MutableCompoundShape();

		for (const ColliderShapeDesc& col : colliderDescs)
		{
			JPH::RefConst<JPH::Shape> subShape = create_shape(col);

			compound->AddShape(to_jolt(col.positionOffset), to_jolt(col.rotationOffset), subShape.GetPtr());
		}

		compound->AdjustCenterOfMass();
		finalShape = compound;
	}

	JPH::EActivation activation = physicsSystem.GetBodyInterface().GetMotionType(bodyID) == JPH::EMotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate;
	physicsSystem.GetBodyInterface().SetShape(bodyID, finalShape.GetPtr(), true, activation);
}

void Physics::sync_physics_transforms()
{
	for (auto& [bodyID, entityID] : bodyToEntity)
	{
		if (bodyID.IsInvalid())
			continue;
		
		glm::mat4 worldMat = ECS::instance->get_world_matrix(entityID);

		glm::vec3 position = glm::vec3(worldMat[3]);

		glm::vec3 scale;
		scale.x = glm::length(glm::vec3(worldMat[0]));
		scale.y = glm::length(glm::vec3(worldMat[1]));
		scale.z = glm::length(glm::vec3(worldMat[2]));

		glm::mat3 rotMat;
		rotMat[0] = glm::vec3(worldMat[0]) / scale.x;
		rotMat[1] = glm::vec3(worldMat[1]) / scale.y;
		rotMat[2] = glm::vec3(worldMat[2]) / scale.z;

		glm::quat rotation = glm::quat_cast(rotMat);

		physicsSystem.GetBodyInterface().SetPositionAndRotation(bodyID, to_jolt(position), to_jolt(rotation), JPH::EActivation::DontActivate);
	}
}

void Physics::physics_update(float fixedDeltaTime)
{
	if (fixedDeltaTime <= 0.0f)
		return;
	
	physicsSystem.Update(fixedDeltaTime, 1, &tempAllocator, &jobSystem);

	auto& bodyInterface = physicsSystem.GetBodyInterface();
	for (auto& [bodyID, entityID] : bodyToEntity)
	{
		if (bodyID.IsInvalid())
			continue;
		
		if (bodyInterface.GetMotionType(bodyID) != JPH::EMotionType::Dynamic)
			continue;
		
		ECS::instance->set_entity_world_pos(entityID, to_glm(bodyInterface.GetPosition(bodyID)));
		ECS::instance->set_entity_world_rot(entityID, to_glm(bodyInterface.GetRotation(bodyID)));
	}
}

void Physics::dispatch_collision_events()
{
	ScriptManager* scriptManager = ScriptManager::instance;

	while (!collisionEventQueue.empty())
	{
		CollisionEvent e = collisionEventQueue.front();
		collisionEventQueue.pop_front();

		if (e.isTrigger)
		{
			switch (e.phase)
			{
				case CollisionPhase::ENTER:
					scriptManager->trigger_enter_script_event(e.self, e.other);
					break;
				case CollisionPhase::STAY:
					scriptManager->trigger_stay_script_event(e.self, e.other);
					break;
				case CollisionPhase::EXIT:
					scriptManager->trigger_exit_script_event(e.self, e.other);
					break;
			}
		}
		else
		{
			switch (e.phase)
			{
				case CollisionPhase::ENTER:
					scriptManager->collision_enter_script_event(e.self, e.other);
					break;
				case CollisionPhase::STAY:
					scriptManager->collision_stay_script_event(e.self, e.other);
					break;
				case CollisionPhase::EXIT:
					scriptManager->collision_exit_script_event(e.self, e.other);
					break;
			}
		}
	}
}

JPH::EMotionType Physics::get_body_motion_type(JPH::BodyID bodyID)
{
	return physicsSystem.GetBodyInterface().GetMotionType(bodyID);
}

void Physics::set_body_motion_type(JPH::BodyID bodyID, JPH::EMotionType motionType)
{
	physicsSystem.GetBodyInterface().SetMotionType(bodyID, motionType, JPH::EActivation::DontActivate);
}

float Physics::get_body_gravity_factor(JPH::BodyID bodyID)
{
	return physicsSystem.GetBodyInterface().GetGravityFactor(bodyID);
}

void Physics::set_body_gravity_factor(JPH::BodyID bodyID, float gravityFactor)
{
	physicsSystem.GetBodyInterface().SetGravityFactor(bodyID, gravityFactor);
}

float Physics::get_body_friction(JPH::BodyID bodyID)
{
	return physicsSystem.GetBodyInterface().GetFriction(bodyID);
}

void Physics::set_body_friction(JPH::BodyID bodyID, float friction)
{
	if (friction < 0.0f)
		return;
	
	physicsSystem.GetBodyInterface().SetFriction(bodyID, friction);
}

float Physics::get_body_restitution(JPH::BodyID bodyID)
{
	return physicsSystem.GetBodyInterface().GetRestitution(bodyID);
}

void Physics::set_body_restitution(JPH::BodyID bodyID, float restitution)
{
	if (restitution < 0.0f)
		return;
	
	physicsSystem.GetBodyInterface().SetRestitution(bodyID, restitution);
}

float Physics::get_body_mass(JPH::BodyID bodyID)
{
	JPH::BodyLockRead lock(physicsSystem.GetBodyLockInterface(), bodyID);
	if (!lock.Succeeded())
		return 0.0f;

	if (!lock.GetBody().IsDynamic())
		return 0.0f;

	return 1.0f / lock.GetBody().GetMotionProperties()->GetInverseMass();	
}

void Physics::set_body_mass(JPH::BodyID bodyID, float mass)
{
	if (mass <= 0.0f)
		return;

	JPH::BodyLockWrite lock(physicsSystem.GetBodyLockInterface(), bodyID);
	if (!lock.Succeeded())
		return;

	if (!lock.GetBody().IsDynamic())
		return;

	lock.GetBody().GetMotionProperties()->ScaleToMass(mass);
}

bool Physics::get_body_is_sensor(JPH::BodyID bodyID)
{
	return physicsSystem.GetBodyInterface().IsSensor(bodyID);
}

void Physics::set_body_is_sensor(JPH::BodyID bodyID, bool isSensor)
{
	physicsSystem.GetBodyInterface().SetIsSensor(bodyID, isSensor);
}

glm::vec3 Physics::get_body_linear_velocity(JPH::BodyID bodyID)
{
	return to_glm(physicsSystem.GetBodyInterface().GetLinearVelocity(bodyID));
}

void Physics::set_body_linear_velocity(JPH::BodyID bodyID, glm::vec3 linearVelocity)
{
	physicsSystem.GetBodyInterface().SetLinearVelocity(bodyID, to_jolt(linearVelocity));
}

glm::vec3 Physics::get_body_angular_velocity(JPH::BodyID bodyID)
{
	return to_glm(physicsSystem.GetBodyInterface().GetAngularVelocity(bodyID));
}

void Physics::set_body_angular_velocity(JPH::BodyID bodyID, glm::vec3 angularVelocity)
{
	physicsSystem.GetBodyInterface().SetAngularVelocity(bodyID, to_jolt(angularVelocity));
}

void Physics::add_force_to_body(JPH::BodyID bodyID, glm::vec3 force)
{
	physicsSystem.GetBodyInterface().AddForce(bodyID, to_jolt(force));
}

void Physics::add_impulse_to_body(JPH::BodyID bodyID, glm::vec3 force)
{
	physicsSystem.GetBodyInterface().AddImpulse(bodyID, to_jolt(force));
}

void Physics::add_angular_force_to_body(JPH::BodyID bodyID, glm::vec3 angularForce)
{
	physicsSystem.GetBodyInterface().AddTorque(bodyID, to_jolt(angularForce));
}

void Physics::add_angular_impulse_to_body(JPH::BodyID bodyID, glm::vec3 angularForce)
{
	physicsSystem.GetBodyInterface().AddAngularImpulse(bodyID, to_jolt(angularForce));
}

void PhysicsContactListener::OnContactAdded(const JPH::Body &body1, const JPH::Body &body2, const JPH::ContactManifold &manifold, JPH::ContactSettings &settings)
{
	EntityID entity1 = physics.bodyToEntity.at(body1.GetID());
	EntityID entity2 = physics.bodyToEntity.at(body2.GetID());

	if (!entity1.isValid() || !entity2.isValid())
		return;

	JPH::SubShapeIDPair subShapePair = { body1.GetID(), manifold.mSubShapeID1, body2.GetID(), manifold.mSubShapeID2 };
	subShapePairIsTriggerCache.emplace( subShapePair, settings.mIsSensor);
	
	physics.collisionEventQueue.push_back( { entity1, entity2, settings.mIsSensor, CollisionPhase::ENTER } );
	physics.collisionEventQueue.push_back( { entity2, entity1, settings.mIsSensor, CollisionPhase::ENTER } );
}

void PhysicsContactListener::OnContactPersisted(const JPH::Body &body1, const JPH::Body &body2, const JPH::ContactManifold &manifold, JPH::ContactSettings &settings)
{
	EntityID entity1 = physics.bodyToEntity.at(body1.GetID());
	EntityID entity2 = physics.bodyToEntity.at(body2.GetID());

	if (!entity1.isValid() || !entity2.isValid())
		return;

	physics.collisionEventQueue.push_back( { entity1, entity2, settings.mIsSensor, CollisionPhase::STAY } );
	physics.collisionEventQueue.push_back( { entity2, entity1, settings.mIsSensor, CollisionPhase::STAY } );
}

void PhysicsContactListener::OnContactRemoved(const JPH::SubShapeIDPair &pair)
{
	EntityID entity1 = physics.bodyToEntity.at(pair.GetBody1ID());
	EntityID entity2 = physics.bodyToEntity.at(pair.GetBody2ID());

	if (!entity1.isValid() || !entity2.isValid())
		return;
	
	bool isSensor = subShapePairIsTriggerCache.at(pair);
	subShapePairIsTriggerCache.erase(pair);

	physics.collisionEventQueue.push_back( { entity1, entity2, isSensor, CollisionPhase::EXIT } );
	physics.collisionEventQueue.push_back( { entity2, entity1, isSensor, CollisionPhase::EXIT } );
}
