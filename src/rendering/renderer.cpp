#include <string>
#include <unordered_map>
#include <deque>
#include <iostream>
#include <glad/glad.h>
#include <SDL3/SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <crossguid/guid.hpp>

#include "../ecs/ecs.h"
#include "../ecs/ecs_component.h"

#include "shader.h"
#include "mesh.h"
#include "model.h"
#include "renderer.h"
#include "camera.h"

Renderer* Renderer::instance = nullptr;

Renderer::Renderer(const char* vertexShaderPath, const char* fragmentShaderPath)
{
	Renderer::instance = this;
	shader = Shader(vertexShaderPath, fragmentShaderPath);
}

Renderer::~Renderer()
{
	Renderer::instance = nullptr;
}

ModelID Renderer::load_model(const std::string &modelPath)
{
	if (pathToModel.find(modelPath) != pathToModel.end())
	{
		ModelID id = pathToModel[modelPath];
		modelReferenceCounts[id]++;
		return id;
	}
	
	Model model(modelPath);
	ModelID id = xg::newGuid();
	models.emplace(id, model);
	
	pathToModel[modelPath] = id;
	modelReferenceCounts[id] = 1;

	return id;
}

void Renderer::release_model(ModelID modelID)
{
	if (models.find(modelID) == models.end())
		return;

	modelReferenceCounts[modelID]--;
	if (modelReferenceCounts[modelID] <= 0)
	{
		models.erase(modelID);
		modelReferenceCounts.erase(modelID);
	}
}

void Renderer::queue_render(EntityID entityID, ModelID modelID)
{
	renderQueue.push_back( { entityID, modelID } );
}

void Renderer::render(SDL_Window* window, int windowWidth, int windowHeight)
{
	if (!Camera::is_active())
		return;

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	shader.use();

	float aspectRatio = static_cast<float>(windowWidth) / windowHeight;
	float nearClippingDist = Camera::instance->get_near_clipping_distance();
	float farClippingDist = Camera::instance->get_far_clipping_distance();
	glm::mat4 projection;
	if (Camera::instance->get_is_orthographic())
	{
		float halfHeight = Camera::instance->get_zoom();
		float halfWidth = halfHeight * aspectRatio;
		projection = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, nearClippingDist, farClippingDist);
	}
	else
		projection = glm::perspective<float>(glm::radians(Camera::instance->get_zoom()), aspectRatio, nearClippingDist, farClippingDist);

	shader.set_mat4("projection", projection);

	glm::mat4 view = Camera::instance->get_view_matrix();
	shader.set_mat4("view", view);

	while (renderQueue.size() > 0)
	{
		RenderInfo renderInfo = renderQueue.front();
		renderQueue.pop_front();
		
		auto it = models.find(renderInfo.modelID);	
		if (it == models.end())
			continue;	

		glm::mat4 modelMatrix = ECS::instance->get_world_matrix(renderInfo.entityID);
		shader.set_mat4("model", modelMatrix);

		it->second.draw(shader);
	}

	SDL_GL_SwapWindow(window);
}
