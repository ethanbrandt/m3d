#include "capsule_collider_component.h"

ColliderShapeDesc CapsuleCollider::get_collider_shape_description()
{
	ColliderShapeDesc colDesc;
	colDesc.type = ColliderType::CAPSULE;
	colDesc.capsuleCollider = desc;
	colDesc.positionOffset = get_position_offset();
	colDesc.rotationOffset = get_rotation_offset();

	return colDesc;
}

ColliderType CapsuleCollider::get_collider_type()
{
	return ColliderType::CAPSULE;
}

void CapsuleCollider::set_radius(float radius)
{
	if (radius <= 0.0f)
		return;
	
	desc.radius = radius;
	update_rigidbody();
}

float CapsuleCollider::get_radius()
{
	return desc.radius;
}

void CapsuleCollider::set_half_height(float halfHeight)
{
	if (halfHeight <= 0.0f)
		return;

	desc.halfHeight = halfHeight;
	update_rigidbody();
}

float CapsuleCollider::get_half_height()
{
	return desc.halfHeight;
}
