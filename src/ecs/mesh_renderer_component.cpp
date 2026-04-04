#include <string>

#include "ecs_component.h"
#include "../rendering/model.h"
#include "../rendering/mesh.h"
#include "../rendering/shader.h"
#include "../rendering/renderer.h"
#include "mesh_renderer_component.h"

MeshRenderer::MeshRenderer()
{
}

MeshRenderer::~MeshRenderer()
{
}

void MeshRenderer::start(EntityID entityID)
{
}

void MeshRenderer::update(EntityID entityID, float deltaTime)
{
	if (!modelID.isValid())
		return;
	
	Renderer::instance->queue_render(get_entity_id(), modelID);
}

void MeshRenderer::on_destroy(EntityID entityID)
{
	if (!modelID.isValid())
		return;
	
	Renderer::instance->release_model(modelID);
}

void MeshRenderer::load_model(const char *modelPath)
{
	modelID = Renderer::instance->load_model(modelPath);
}

void MeshRenderer::on_initialize()
{
	
}
