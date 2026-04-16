#pragma once

#include <string>

#include "ecs_component.h"
#include "../rendering/model.h"
#include "../rendering/mesh.h"
#include "../rendering/shader.h"
#include "../rendering/renderer.h"

class MeshRenderer : public Component
{
public:
	MeshRenderer();
	~MeshRenderer() override;

	void start() override;
	void update(float deltaTime) override;
	void on_destroy() override;

	void load_model(const char* modelPath);
private:
	ModelID modelID;

protected:
	void on_initialize() override;
};