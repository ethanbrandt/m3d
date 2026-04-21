#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "../ecs/ecs.h"

using json = nlohmann::json;

struct StartConfigSettings
{

};

class SceneLoader
{
private:
	EntityID load_entity_node(const json& entityJson, EntityID parentID, std::vector<EntityID>& entities);
	void load_components(EntityID entityID, const json& componentsJson);
	
	void load_mesh_renderer(EntityID entityID, const json& componentJson);
	void load_script(EntityID entityID, const json& componentJson);
	void load_audio_player(EntityID entityID, const json& componentJson);
	void load_rigid_body(EntityID entityID, const json& componentJson);
	void load_box_collider(EntityID entityID, const json& componentJson);
	void load_sphere_collider(EntityID entityID, const json& componentJson);
	void load_capsule_collider(EntityID entityID, const json& componentJson);
	void load_camera(EntityID entityID, const json& componentJson);

public:
	StartConfigSettings load_start_config(const std::string& startConfigPath);
	void load_and_initialize_scene(const std::string& scenePath);
	EntityID load_entity_prefab(const std::string& prefabPath);
};