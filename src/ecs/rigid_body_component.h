#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>

#include <unordered_map>

#include "ecs_component.h"
#include "ecs.h"
#include "../physics/physics.h"

class RigidBody : public Component
{
public:
	RigidBody();
	~RigidBody() override;

	void start() override;
	void update(float deltaTime) override;
	void on_destroy() override;

	void set_body_desc(BodyDesc _bodyDesc);
	void add_collision_shape(ComponentID colliderID, ColliderShapeDesc shapeDesc);
	void remove_collision_shape(ComponentID colliderID);
	void update_collision_shape(ComponentID colliderID, ColliderShapeDesc shapeDesc);

	JPH::EMotionType get_motion_type();
	void set_motion_type(JPH::EMotionType motionType);
	float get_gravity_factor();
	void set_gravity_factor(float gravityFactor);
	float get_friction();
	void set_friction(float friction);
	float get_restitution();
	void set_restitution(float restitution);
	float get_mass();
	void set_mass(float mass);
	bool get_is_sensor();
	void set_is_sensor(bool isSensor);
	glm::vec3 get_linear_velocity();
	void set_linear_velocity(glm::vec3 linearVelocity);
	glm::vec3 get_angular_velocity();
	void set_angular_velocity(glm::vec3 angularVelocity);
	void add_force(glm::vec3 force);
	void add_impulse(glm::vec3 force);
	void add_angular_force(glm::vec3 angularForce);
	void add_angular_impulse(glm::vec3 angularForce);

private:
	JPH::BodyID bodyID;	
	BodyDesc bodyDesc;
	std::unordered_map<ComponentID, ColliderShapeDesc> colliders;

	void set_physics_colliders();

protected:
	void on_initialize() override;
};