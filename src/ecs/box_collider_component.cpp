#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../physics/physics.h"
#include "collider_component.h"

#include "box_collider_component.h"

ColliderShapeDesc BoxCollider::get_collider_shape_description()
{
	ColliderShapeDesc colDesc;
	colDesc.type = ColliderType::BOX;
	colDesc.boxCollider = desc;
	colDesc.positionOffset = get_position_offset();
	colDesc.rotationOffset = get_rotation_offset();

	return colDesc;
}

ColliderType BoxCollider::get_collider_type()
{
	return ColliderType::BOX;
}

void BoxCollider::set_half_dimensions(const glm::vec3 &halfDimensions)
{
	if (halfDimensions.x <= 0.0f || halfDimensions.y <= 0.0f || halfDimensions.z <= 0.0f)
		return;
	
	desc.halfDimensions = halfDimensions;
	update_rigidbody();
}

glm::vec3 BoxCollider::get_half_dimensions()
{
	return desc.halfDimensions;
}
