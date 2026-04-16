#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../physics/physics.h"
#include "ecs.h"
#include "ecs_component.h"
#include "collider_component.h"

class BoxCollider : public Collider 
{
public:
	BoxCollider() = default;

	ColliderShapeDesc get_collider_shape_description() override;
	ColliderType get_collider_type() override;

	void set_half_dimensions(const glm::vec3& halfDimensions);
	glm::vec3 get_half_dimensions();

private:
	BoxColliderDesc desc;
};