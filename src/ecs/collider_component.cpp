#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/quaternion.hpp>

#include <set>
#include <memory>

#include "../physics/physics.h"
#include "ecs.h"
#include "ecs_component.h"
#include "rigid_body_component.h"

#include "collider_component.h"

Collider::Collider()
{
	type = ComponentType::COLLIDER;
}

Collider::~Collider()
{

}

void Collider::start()
{

}

void Collider::update(float deltaTime)
{

}

void Collider::on_destroy()
{
	auto rigidBodies = ECS::instance->get_all_component_ids_of_type(get_entity_id(), ComponentType::RIGID_BODY);
	if (rigidBodies.size() == 0)
		return;

	ComponentID id = *rigidBodies.begin();
	if (!ECS::instance->is_component_valid(id))
		return;
	
	RigidBody& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(id));

	rb.remove_collision_shape(get_id());
}

void Collider::set_position_offset(const glm::vec3& _positionOffset)
{
	positionOffset = _positionOffset;
	update_rigidbody();
}

void Collider::set_rotation_offset(const glm::quat& _rotationOffset)
{
	rotationOffset = glm::normalize(_rotationOffset);
	eulerDegreeOffset = glm::degrees(glm::eulerAngles(glm::normalize(_rotationOffset)));
	update_rigidbody();
}

void Collider::set_euler_degree_offset(const glm::vec3 &_eulerDegreeOffset)
{
	rotationOffset = glm::normalize(glm::quat(glm::radians(_eulerDegreeOffset)));
	eulerDegreeOffset = _eulerDegreeOffset;
	update_rigidbody();
}

glm::vec3 Collider::get_position_offset()
{
	return positionOffset;
}

glm::quat Collider::get_rotation_offset()
{
	return rotationOffset;
}

glm::vec3 Collider::get_euler_degree_offset()
{
	return eulerDegreeOffset;
}

void Collider::on_initialize()
{
	auto rigidBodies = ECS::instance->get_all_component_ids_of_type(get_entity_id(), ComponentType::RIGID_BODY);
	if (rigidBodies.size() == 0)
		return;

	ComponentID id = *rigidBodies.begin();

	if (!ECS::instance->is_component_valid(id))
		return;

	RigidBody& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(id));

	rb.add_collision_shape(get_id(), get_collider_shape_description());
}

void Collider::update_rigidbody()
{
	if (!get_initialized())
		return;

	auto rigidBodies = ECS::instance->get_all_component_ids_of_type(get_entity_id(), ComponentType::RIGID_BODY);
	if (rigidBodies.size() == 0)
		return;

	ComponentID id = *rigidBodies.begin();

	if (!ECS::instance->is_component_valid(id))
		return;

	RigidBody& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(id));

	rb.update_collision_shape(get_id(), get_collider_shape_description());
}
