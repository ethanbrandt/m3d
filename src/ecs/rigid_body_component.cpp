#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>

#include <unordered_map>
#include <vector>

#include "ecs_component.h"
#include "ecs.h"
#include "collider_component.h"
#include "../physics/physics.h"
#include "rigid_body_component.h"

RigidBody::RigidBody()
{
	type = ComponentType::RIGID_BODY;
}

RigidBody::~RigidBody()
{
}

void RigidBody::start()
{
}

void RigidBody::update(float deltaTime)
{
}

void RigidBody::on_destroy()
{
	Physics::instance->destroy_body(bodyID);
	bodyID = JPH::BodyID();
}

void RigidBody::set_body_desc(BodyDesc _bodyDesc)
{
	bodyDesc = _bodyDesc;
}

void RigidBody::add_collision_shape(ComponentID colliderID, ColliderShapeDesc shapeDesc)
{
	if (!colliderID.isValid())
		return;
	
	colliders.emplace(colliderID, shapeDesc);

	set_physics_colliders();
}

void RigidBody::remove_collision_shape(ComponentID colliderID)
{
	if (!colliderID.isValid())
		return;
	
	if (colliders.find(colliderID) == colliders.end())
		return;
	
	colliders.erase(colliderID);

	set_physics_colliders();
}

void RigidBody::update_collision_shape(ComponentID colliderID, ColliderShapeDesc shapeDesc)
{
	if (!colliderID.isValid())
		return;
	
	if (colliders.find(colliderID) == colliders.end())
		return;
	
	colliders.at(colliderID) = shapeDesc;

	set_physics_colliders();
}

JPH::EMotionType RigidBody::get_motion_type()
{
	return Physics::instance->get_body_motion_type(bodyID);
}

void RigidBody::set_motion_type(JPH::EMotionType motionType)
{
	Physics::instance->set_body_motion_type(bodyID, motionType);
}

float RigidBody::get_gravity_factor()
{
	return Physics::instance->get_body_gravity_factor(bodyID);
}

void RigidBody::set_gravity_factor(float gravityFactor)
{
	Physics::instance->set_body_gravity_factor(bodyID, gravityFactor);
}

float RigidBody::get_friction()
{
	return Physics::instance->get_body_friction(bodyID);
}

void RigidBody::set_friction(float friction)
{
	Physics::instance->set_body_friction(bodyID, friction);
}

float RigidBody::get_restitution()
{
	return Physics::instance->get_body_restitution(bodyID);
}

void RigidBody::set_restitution(float restitution)
{
	Physics::instance->set_body_restitution(bodyID, restitution);
}

float RigidBody::get_mass()
{
	return Physics::instance->get_body_mass(bodyID);
}

void RigidBody::set_mass(float mass)
{
	Physics::instance->set_body_mass(bodyID, mass);
}

bool RigidBody::get_is_sensor()
{
	return Physics::instance->get_body_is_sensor(bodyID);
}

void RigidBody::set_is_sensor(bool isSensor)
{
	Physics::instance->set_body_is_sensor(bodyID, isSensor);
}

glm::vec3 RigidBody::get_linear_velocity()
{
	return Physics::instance->get_body_linear_velocity(bodyID);
}

void RigidBody::set_linear_velocity(glm::vec3 linearVelocity)
{
	Physics::instance->set_body_linear_velocity(bodyID, linearVelocity);
}

glm::vec3 RigidBody::get_angular_velocity()
{
	return Physics::instance->get_body_angular_velocity(bodyID);
}

void RigidBody::set_angular_velocity(glm::vec3 angularVelocity)
{
	Physics::instance->set_body_angular_velocity(bodyID, angularVelocity);
}

void RigidBody::add_force(glm::vec3 force)
{
	Physics::instance->add_force_to_body(bodyID, force);
}

void RigidBody::add_impulse(glm::vec3 force)
{
	Physics::instance->add_impulse_to_body(bodyID, force);
}

void RigidBody::add_angular_force(glm::vec3 angularForce)
{
	Physics::instance->add_angular_force_to_body(bodyID, angularForce);
}

void RigidBody::add_angular_impulse(glm::vec3 angularForce)
{
	Physics::instance->add_angular_impulse_to_body(bodyID, angularForce);
}

void RigidBody::set_physics_colliders()
{
	if (bodyID.IsInvalid())
		return;
	
	std::vector<ColliderShapeDesc> colVec;
	colVec.reserve(colliders.size());

	std::transform(colliders.begin(), colliders.end(), std::back_inserter(colVec), [](auto const& pair) { return pair.second; });

	Physics::instance->set_colliders(bodyID, colVec);
}

void RigidBody::on_initialize()
{
	bodyID = Physics::instance->create_body(bodyDesc, get_entity_id());

	auto components = ECS::instance->get_all_component_ids_of_type(get_entity_id(), ComponentType::COLLIDER);
	if (components.size() == 0)
		return;

	for (auto& component : components)
	{
		if (!ECS::instance->is_component_valid(component))
			continue;

		Collider& col = dynamic_cast<Collider&>(ECS::instance->get_component_reference(component));
		colliders.emplace(component, col.get_collider_shape_description());	
	}

	set_physics_colliders();
}
