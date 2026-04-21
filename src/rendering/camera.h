#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <crossguid/guid.hpp>

using EntityID = xg::Guid;

class Camera
{
public:
	static Camera* instance;

	Camera();
	~Camera();

	static bool is_active();
	glm::mat4 get_view_matrix();

	void attach_entity(EntityID _entityID);
	EntityID get_entity_id();

	float get_zoom();
	void set_zoom(float _zoom);

	bool get_is_orthographic();
	void set_is_orthographic(bool _isOrtho);

	float get_near_clipping_distance();
	void set_near_clipping_distance(float distance);

	float get_far_clipping_distance();
	void set_far_clipping_distance(float distance);
private:
	EntityID entityID;
	float zoom = 45.0f;
	bool isOrtho = false;
	float nearClippingDist = 0.1f;
	float farClippingDist = 1000.0f;
};