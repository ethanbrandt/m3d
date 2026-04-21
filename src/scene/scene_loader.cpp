#include "scene_loader.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/MotionType.h>

#include "ecs/audio_player_component.h"
#include "ecs/box_collider_component.h"
#include "ecs/capsule_collider_component.h"
#include "ecs/mesh_renderer_component.h"
#include "ecs/rigid_body_component.h"
#include "ecs/script_component.h"
#include "ecs/sphere_collider_component.h"
#include "rendering/camera.h"

namespace
{
	glm::vec3 read_vec3(const json& vec3Json, const char* key, const glm::vec3& defaultVal)
	{
		if (!vec3Json.contains(key) || !vec3Json.at(key).is_array() || vec3Json.at(key).size() != 3)
			return defaultVal;
		
		const auto& val = vec3Json.at(key);
		return glm::vec3(val.at(0).get<float>(), val.at(1).get<float>(), val.at(2).get<float>());
	}

	glm::vec3 read_transform_vec3(const json& entityJson, const char* key, const glm::vec3& defaultVal)
	{
		if (!entityJson.contains("transform"))
			return defaultVal;
		
		return read_vec3(entityJson.at("transform"), key, defaultVal);
	}
}

EntityID SceneLoader::load_entity_node(const json &entityJson, EntityID parentID, std::vector<EntityID>& entities)
{
	ECS* ecs = ECS::instance;
	std::string name = entityJson.value("name", "");
	EntityID entityID = ecs->create_entity_record(name);

	if (!entityID.isValid())
		return EntityID();
	
	glm::vec3 position = read_transform_vec3(entityJson, "position", glm::vec3(0.0f));
	glm::vec3 eulerDegrees = read_transform_vec3(entityJson, "eulerDegrees", glm::vec3(0.0f));
	glm::vec3 scale = read_transform_vec3(entityJson, "scale", glm::vec3(1.0f));

	ecs->set_entity_local_pos(entityID, position);
	ecs->set_entity_local_rot(entityID, eulerDegrees);
	ecs->set_entity_local_scale(entityID, scale);

	if (parentID.isValid())
		ecs->set_parent_entity(entityID, parentID, false);
	
	if (entityJson.contains("components"))
		load_components(entityID, entityJson.at("components"));

	entities.push_back(entityID);

	if (entityJson.contains("children"))
	{
		for (const auto& childJson : entityJson.at("children"))
			load_entity_node(childJson, entityID, entities);
	}

	return entityID;
}

void SceneLoader::load_components(EntityID entityID, const json &componentsJson)
{
	if (!componentsJson.is_array())
		return;
	
	for (const auto& componentJson : componentsJson)
	{
		std::string type = componentJson.value("type", "");
		if (type == "MESH_RENDERER")
			load_mesh_renderer(entityID, componentJson);
		else if (type == "SCRIPT")
			load_script(entityID, componentJson);
		else if (type == "AUDIO_PLAYER")
			load_audio_player(entityID, componentJson);
		else if (type == "RIGIDBODY")
			load_rigid_body(entityID, componentJson);
		else if (type == "BOX_COLLIDER")
			load_box_collider(entityID, componentJson);
		else if (type == "SPHERE_COLLIDER")
			load_sphere_collider(entityID, componentJson);
		else if (type == "CAPSULE_COLLIDER")
			load_capsule_collider(entityID, componentJson);
		else if (type == "CAMERA")
			load_camera(entityID, componentJson);
		else
			std::cout << "[Scene Loader Error] Unknown component type: " << type << '\n';
	}
}

void SceneLoader::load_mesh_renderer(EntityID entityID, const json &componentJson)
{
	auto meshRenderer = std::make_unique<MeshRenderer>();

	std::string modelPath = componentJson.value("modelPath", "");
	if (!modelPath.empty())
		meshRenderer->load_model(modelPath.c_str());
	
	ECS::instance->attach_component(entityID, std::move(meshRenderer));
}

void SceneLoader::load_script(EntityID entityID, const json &componentJson)
{
	auto script = std::make_unique<ScriptComponent>();
	script->set_module_name(componentJson.value("moduleName", ""));
	ECS::instance->attach_component(entityID, std::move(script));
}

void SceneLoader::load_audio_player(EntityID entityID, const json &componentJson)
{
	auto audioPlayer = std::make_unique<AudioPlayer>();

	std::string soundPath = componentJson.value("soundPath", "");
	if (!soundPath.empty())
		audioPlayer->set_sound(soundPath);
	
	audioPlayer->set_gain(componentJson.value("gain", 1.0f));
	audioPlayer->set_looping(componentJson.value("isLooping", false));
	audioPlayer->set_playing(componentJson.value("playOnStart", false));
	audioPlayer->set_full_volume_radius(componentJson.value("fullVolumeRadius", 10.0f));
	audioPlayer->set_attenuation(componentJson.value("attenuation", 0.02f));

	ECS::instance->attach_component(entityID, std::move(audioPlayer));
}

void SceneLoader::load_rigid_body(EntityID entityID, const json &componentJson)
{
	ECS::instance->sync_transforms();

	auto rigidbody = std::make_unique<RigidBody>();

	glm::vec3 pos;
	glm::quat rot;	
	glm::vec3 scale;
	glm::vec3 skew;
	glm::vec4 perspective;

	glm::mat4 worldMat = ECS::instance->get_world_matrix(entityID);
	glm::decompose(worldMat, scale, rot, pos, skew, perspective);
	rot = glm::normalize(rot);

	BodyDesc desc;

	desc.position = pos;
	desc.rotation = rot;

	std::string motionTypeStr = componentJson.value("motionType", "DYNAMIC");
	if (motionTypeStr == "STATIC")
		desc.motionType = JPH::EMotionType::Static;
	else if (motionTypeStr == "KINEMATIC")
		desc.motionType = JPH::EMotionType::Kinematic;
	else
		desc.motionType = JPH::EMotionType::Dynamic;
	
	desc.physicsLayer = componentJson.value("layer", desc.physicsLayer);
	desc.friction = componentJson.value("friction", desc.friction);
	desc.restitution = componentJson.value("restitution", desc.restitution);
	desc.mass = componentJson.value("mass", desc.mass);
	desc.gravityFactor = componentJson.value("gravityFactor", desc.gravityFactor);
	desc.isSensor = componentJson.value("isTrigger", desc.isSensor);

	rigidbody->set_body_desc(desc);
	ECS::instance->attach_component(entityID, std::move(rigidbody));
}

void SceneLoader::load_box_collider(EntityID entityID, const json &componentJson)
{
	auto col = std::make_unique<BoxCollider>();

	col->set_half_dimensions(read_vec3(componentJson, "halfDimensions", glm::vec3(0.5f)));
	col->set_position_offset(read_vec3(componentJson, "positionOffset", glm::vec3(0.0f)));
	col->set_euler_degree_offset(read_vec3(componentJson, "eulerDegreesOffset", glm::vec3(0.0f)));

	ECS::instance->attach_component(entityID, std::move(col));
}

void SceneLoader::load_sphere_collider(EntityID entityID, const json &componentJson)
{
	auto col = std::make_unique<SphereCollider>();

	col->set_radius(componentJson.value("radius", 0.5f));
	col->set_position_offset(read_vec3(componentJson, "positionOffset", glm::vec3(0.0f)));
	col->set_euler_degree_offset(read_vec3(componentJson, "eulerDegreesOffset", glm::vec3(0.0f)));

	ECS::instance->attach_component(entityID, std::move(col));
}

void SceneLoader::load_capsule_collider(EntityID entityID, const json &componentJson)
{
	auto col = std::make_unique<CapsuleCollider>();

	col->set_radius(componentJson.value("radius", 0.5f));
	col->set_half_height(componentJson.value("halfHeight", 0.5f));
	col->set_position_offset(read_vec3(componentJson, "positionOffset", glm::vec3(0.0f)));
	col->set_euler_degree_offset(read_vec3(componentJson, "eulerDegreesOffset", glm::vec3(0.0f)));

	ECS::instance->attach_component(entityID, std::move(col));
}

void SceneLoader::load_camera(EntityID entityID, const json &componentJson)
{
	Camera* cam = Camera::instance;
	if (!cam)
		return;

	cam->attach_entity(entityID);
	cam->set_is_orthographic(componentJson.value("isOrthographic", cam->get_is_orthographic()));
	cam->set_near_clipping_distance(componentJson.value("nearClip", cam->get_near_clipping_distance()));
	cam->set_far_clipping_distance(componentJson.value("farClip", cam->get_far_clipping_distance()));
	cam->set_zoom(componentJson.value("zoom", cam->get_zoom()));
}

StartConfigSettings SceneLoader::load_start_config(const std::string &startConfigPath)
{
	return StartConfigSettings();
}

void SceneLoader::load_and_initialize_scene(const std::string &scenePath)
{
	auto startTime = std::chrono::steady_clock::now();

	std::ifstream file(scenePath);
	if (!file.is_open())
	{
		std::cout << "[Scene Loader Error] Failed to open scene file: " << scenePath << '\n';
		return;
	}

	json sceneJson = json::parse(file, nullptr, false);
	if (sceneJson.is_discarded())
	{
		std::cout << "[Scene Loader Error] Failed to parse scene JSON: " << scenePath << '\n';
		return;
	}

	if (sceneJson.value("schema", "") != "m3d.scene")
	{
		std::cout << "[Scene Loader Error] Unsupported schema: " << scenePath << '\n';
		return;
	}

	if (!sceneJson.contains("entities") || !sceneJson.at("entities").is_array())
	{
		std::cout << "[Scene Loader Error] Scene is missing entities array: " << scenePath << '\n';
		return;
	}

	std::vector<EntityID> entities;
	for (const auto& entityJson : sceneJson.at("entities"))
		load_entity_node(entityJson, EntityID(), entities);

	ECS::instance->sync_transforms();

	for (auto& id : entities)
		ECS::instance->start(id);

	std::cout << "Scene load time: " << std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count() << '\n';
}

EntityID SceneLoader::load_entity_prefab(const std::string &prefabPath)
{
	std::ifstream file(prefabPath);
	if (!file.is_open())
	{
		std::cout << "[Scene Loader Error] Failed to open prefab file: " << prefabPath << '\n';
		return {};
	}

	json prefabJson = json::parse(file, nullptr, false);
	if (prefabJson.is_discarded())
	{
		std::cout << "[Scene Loader Error] Failed to parse prefab JSON: " << prefabPath << '\n';
		return {};
	}

	if (prefabJson.value("schema", "") != "m3d.prefab")
	{
		std::cout << "[Scene Loader Error] Unsupported schema: " << prefabJson << '\n';
		return {};	
	}

	std::vector<EntityID> entities;
	EntityID entityID = load_entity_node(prefabJson, EntityID(), entities);

	ECS::instance->sync_transforms();

	for (auto& id : entities)
		ECS::instance->start(id);
	
	return entityID;
}
