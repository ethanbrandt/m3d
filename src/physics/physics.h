#pragma once

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
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/RegisterTypes.h>

#include <memory>
#include <unordered_map>
#include <vector>
#include <deque>

#include "../ecs/ecs.h"

enum ColliderType
{
	BOX,
	SPHERE,
	CAPSULE	
};

struct BoxColliderDesc
{
	glm::vec3 halfDimensions = glm::vec3(0.5f);
};

struct SphereColliderDesc
{
	float radius = 0.5f;
};

struct CapsuleColliderDesc
{
	float halfHeight = 0.5f;
	float radius = 0.25;
};

struct ColliderShapeDesc
{
	ColliderType type = ColliderType::BOX;
	BoxColliderDesc boxCollider;
	SphereColliderDesc sphereCollider;
	CapsuleColliderDesc capsuleCollider;
	glm::vec3 positionOffset;
	glm::quat rotationOffset;
};

struct BodyDesc
{
	glm::vec3 position = glm::vec3(0.0f);
	glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

	JPH::EMotionType motionType = JPH::EMotionType::Dynamic;
	JPH::ObjectLayer physicsLayer = 1;

	float friction = 0.8f;
	float restitution = 0.0f;
	float mass = 1.0f;
	float gravityFactor = 1.0f;
	bool isSensor = false;
};

enum CollisionPhase
{
	ENTER,
	STAY,
	EXIT
};

struct CollisionEvent
{
	EntityID self;
	EntityID other;
	bool isTrigger;
	CollisionPhase phase;
};

class Physics;

class PhysicsContactListener final : public JPH::ContactListener
{
private:
	Physics& physics;
	std::unordered_map<JPH::SubShapeIDPair, bool> subShapePairIsTriggerCache;
public:
	PhysicsContactListener(Physics& _physics) : physics(_physics) {};

	void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override;
	void OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override;
	void OnContactRemoved(const JPH::SubShapeIDPair& pair) override;
};

class Physics
{

private:
	std::unordered_map<JPH::BodyID, EntityID> bodyToEntity;

	std::deque<CollisionEvent> collisionEventQueue;
	PhysicsContactListener contactListener;

	JPH::ObjectLayerPairFilterTable objectLayerPairFilter;
	JPH::BroadPhaseLayerInterfaceTable broadPhaseLayerInterface;
	std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable> objectVsBroadPhaseLayerFilter;
	JPH::TempAllocatorImpl tempAllocator;
	JPH::JobSystemThreadPool jobSystem;
	JPH::PhysicsSystem physicsSystem;

	JPH::Vec3 to_jolt(const glm::vec3 &val);
	JPH::Quat to_jolt(const glm::quat &val);
	glm::vec3 to_glm(const JPH::Vec3 &val);
	glm::quat to_glm(const JPH::Quat &val);

	JPH::RefConst<JPH::Shape> create_shape(const ColliderShapeDesc& shapeDesc);
	JPH::RefConst<JPH::Shape> apply_local_offset(JPH::RefConst<JPH::Shape> shape, const glm::vec3& positionOffset, const glm::quat& rotationOffset);

	friend PhysicsContactListener;

public:
	static Physics* instance;

	Physics();
	~Physics();

	JPH::BodyID create_body(const BodyDesc& bodyDesc, EntityID entityID);
	void destroy_body(JPH::BodyID bodyID);

	void set_colliders(JPH::BodyID bodyID, const std::vector<ColliderShapeDesc>& colliderDescs);

	void sync_physics_transforms();

	void physics_update(float fixedDeltaTime);
	void dispatch_collision_events();

	JPH::EMotionType get_body_motion_type(JPH::BodyID bodyID);
	void set_body_motion_type(JPH::BodyID bodyID, JPH::EMotionType motionType);
	float get_body_gravity_factor(JPH::BodyID bodyID);
	void set_body_gravity_factor(JPH::BodyID bodyID, float gravityFactor);
	float get_body_friction(JPH::BodyID bodyID);
	void set_body_friction(JPH::BodyID bodyID, float friction);
	float get_body_restitution(JPH::BodyID bodyID);
	void set_body_restitution(JPH::BodyID bodyID, float restitution);
	float get_body_mass(JPH::BodyID bodyID);
	void set_body_mass(JPH::BodyID bodyID, float mass);
	bool get_body_is_sensor(JPH::BodyID bodyID);
	void set_body_is_sensor(JPH::BodyID bodyID, bool isSensor);
	glm::vec3 get_body_linear_velocity(JPH::BodyID bodyID);
	void set_body_linear_velocity(JPH::BodyID bodyID, glm::vec3 linearVelocity);
	glm::vec3 get_body_angular_velocity(JPH::BodyID bodyID);
	void set_body_angular_velocity(JPH::BodyID bodyID, glm::vec3 angularVelocity);
	void add_force_to_body(JPH::BodyID bodyID, glm::vec3 force);
	void add_impulse_to_body(JPH::BodyID bodyID, glm::vec3 force);
	void add_angular_force_to_body(JPH::BodyID bodyID, glm::vec3 angularForce);
	void add_angular_impulse_to_body(JPH::BodyID bodyID, glm::vec3 angularForce);
};