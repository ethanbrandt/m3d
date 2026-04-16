#include "sphere_collider_component.h"

ColliderShapeDesc SphereCollider::get_collider_shape_description()
{
	ColliderShapeDesc colDesc;
	colDesc.type = ColliderType::SPHERE;
	colDesc.sphereCollider = desc;
	colDesc.positionOffset = get_position_offset();
	colDesc.rotationOffset = get_rotation_offset();

	return colDesc;
}

ColliderType SphereCollider::get_collider_type()
{
	return ColliderType::SPHERE;
}

void SphereCollider::set_radius(float radius)
{
	if (radius <= 0.0f)
		return;
	
	desc.radius = radius;
	update_rigidbody();
}

float SphereCollider::get_radius()
{
	return desc.radius;
}
