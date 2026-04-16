#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../physics/physics.h"
#include "ecs.h"
#include "ecs_component.h"
#include "collider_component.h"

class SphereCollider : public Collider 
{
public:
	SphereCollider() = default;

	ColliderShapeDesc get_collider_shape_description() override;
	ColliderType get_collider_type() override;

	void set_radius(float radius);
	float get_radius();

private:
	SphereColliderDesc desc;
};