#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/quaternion.hpp>

#include <crossguid/guid.hpp>

#include "../ecs/ecs.h"
#include "camera.h"

Camera* Camera::instance = nullptr;

Camera::Camera()
{
	if (Camera::instance)
	{
		std::cout << "[Camera Error] Scene cannot have multiple cameras\n";
		return;
	}

	Camera::instance = this;
}

Camera::~Camera()
{
	Camera::instance = nullptr;
}

bool Camera::is_active()
{
	if (Camera::instance == nullptr)
		return false;
	
	if (!Camera::instance->entityID.isValid())
		return false;

	return true;	
}

glm::mat4 Camera::get_view_matrix()
{
	glm::mat4 worldMat = ECS::instance->get_world_matrix(entityID);
	glm::vec3 scale;
	glm::quat rot;
	glm::vec3 trans;
	glm::vec3 skew;
	glm::vec4 persp;

	glm::decompose(worldMat, scale, rot, trans, skew, persp);
	rot = glm::normalize(rot);

	glm::mat4 viewWorld = glm::translate(glm::mat4(1.0f), trans) * glm::mat4_cast(rot);

	return glm::inverse(viewWorld);
}

void Camera::attach_entity(EntityID _entityID)
{
	entityID = _entityID;
}

EntityID Camera::get_entity_id()
{
	return entityID;
}

float Camera::get_zoom()
{
	return zoom;
}

void Camera::set_zoom(float _zoom)
{
	if (_zoom <= 0.0f || _zoom > 360.0f)
		return;
	
	zoom = _zoom;	
}

bool Camera::get_is_orthographic()
{
	return isOrtho;
}

void Camera::set_is_orthographic(bool _isOrtho)
{
	isOrtho = _isOrtho;
}

float Camera::get_near_clipping_distance()
{
	return nearClippingDist;
}

void Camera::set_near_clipping_distance(float distance)
{
	if (distance < 0.1f)
		return;
	nearClippingDist = distance;
}

float Camera::get_far_clipping_distance()
{
	return farClippingDist;
}

void Camera::set_far_clipping_distance(float distance)
{
	farClippingDist = distance;
}
