#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../physics/physics.h"
#include "ecs.h"
#include "ecs_component.h"

class Collider : public Component
{
public:
	Collider();
	~Collider() override;

	virtual ColliderShapeDesc get_collider_shape_description() = 0;
	virtual ColliderType get_collider_type() = 0;

	void start() override;
	void update(float deltaTime) override;
	void on_destroy() override;

	void set_position_offset(const glm::vec3& _positionOffset);
	void set_rotation_offset(const glm::quat& _rotationOffset);
	void set_euler_degree_offset(const glm::vec3& _eulerDegreeOffset);

	glm::vec3 get_position_offset();
	glm::quat get_rotation_offset();
	glm::vec3 get_euler_degree_offset();

private:
	glm::vec3 positionOffset = glm::vec3(0.0f);
	glm::quat rotationOffset = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 eulerDegreeOffset = glm::vec3(0.0f);

protected:
	void on_initialize() override;
	void update_rigidbody();
};