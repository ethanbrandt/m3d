#pragma once

#include <string>
#include <unordered_map>
#include <deque>

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

using ModelID = xg::Guid;

class Renderer
{
public:
	static Renderer* instance;
	Renderer(const char* vertexShaderPath, const char* fragmentShaderPath);
	~Renderer();

	ModelID load_model(const std::string& modelPath);
	void release_model(ModelID modelID);

	void queue_render(EntityID entityID, ModelID modelID);
	void render(SDL_Window* window, int windowWidth, int windowHeight);

private:
	struct RenderInfo
	{
		EntityID entityID;
		ModelID modelID;
	};

	Shader shader;
	std::unordered_map<ModelID, Model> models;
	std::unordered_map<std::string, ModelID> pathToModel;
	std::unordered_map<ModelID, int> modelReferenceCounts;

	std::deque<RenderInfo> renderQueue;
};
