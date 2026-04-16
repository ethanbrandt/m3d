#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <utility>
#include <typeinfo>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <crossguid/guid.hpp>
#include "ecs_component.h"

using EntityID = xg::Guid;

struct Transform
{
	glm::vec3 localPosition = glm::vec3(0.0f);
	glm::quat localRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 localEulerDegrees = glm::vec3(0.0f);
	glm::vec3 localScale = glm::vec3(1.0f);

	glm::mat4 localMatrix = glm::mat4(1.0f);
	glm::mat4 worldMatrix = glm::mat4(1.0f);

	bool localDirty = true;
	bool worldDirty = true;
};

class ECS
{
private:
	struct EntityRecord
	{
		EntityID id = xg::Guid();
		std::string name = "";
		Transform transform;
		std::unordered_map<ComponentID, std::unique_ptr<Component>> components;

		EntityID parentID = xg::Guid();
		std::unordered_set<EntityID> childrenIDs;

		EntityRecord() = default;

		EntityRecord(EntityID entityID, std::string entityName) : id(entityID), name(entityName) { }

		EntityRecord(const EntityRecord&) = delete;
		EntityRecord& operator=(const EntityRecord&) = delete;

		EntityRecord(EntityRecord&&) = default;
		EntityRecord& operator=(EntityRecord&&) = default;

		~EntityRecord()
		{
			for (auto& [componentID, component] : components)
				component->on_destroy();
		}
	};

	std::unordered_map<EntityID, EntityRecord> records;
	std::unordered_map<ComponentID, EntityID> componentToEntity;

	bool would_create_cycle(EntityID child, EntityID parent);

	void update_world_recursive(EntityID entityID, bool parentDirty);
	void mark_transform_subtree_dirty(EntityID entityID);

	glm::mat4 compose_transform(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale);
	bool decompose_transform(const glm::mat4& matrix, glm::vec3& outPos, glm::quat& outQuat, glm::vec3& outScale);

public:
	static ECS* instance;

	ECS();
	~ECS();

	EntityID create_entity_record(std::string name);
	void destroy_entity_record(EntityID entityID);

	bool is_entity_valid(EntityID entityID) const;

	EntityID get_attached_entity(ComponentID id) const;

	ComponentID attach_component(EntityID entityID, std::unique_ptr<Component> component);
	void remove_component(EntityID entityID, ComponentID componentID);

	EntityID get_parent_entity(EntityID entity);
	std::unordered_set<EntityID> get_children_entities(EntityID entity);

	std::set<ComponentID> get_all_component_ids_of_type(EntityID entityID, ComponentType type) const;

	bool is_component_valid(ComponentID id);
	Component& get_component_reference(ComponentID componentID);

	EntityID get_entity_by_name(std::string name) const;

	std::string get_entity_name(EntityID entityID);

	void set_entity_world_pos(EntityID entityID, glm::vec3 pos);
	void set_entity_world_rot(EntityID entityID, glm::quat rot);

	void set_entity_local_pos(EntityID entityID, glm::vec3 pos);
	void set_entity_local_rot(EntityID entityID, glm::vec3 eulerDegrees);
	void set_entity_local_rot(EntityID entityID, glm::quat rot);
	void set_entity_local_scale(EntityID entityID, glm::vec3 scale);

	glm::vec3 get_entity_local_pos(EntityID entityID);
	glm::quat get_entity_local_rot(EntityID entityID);
	glm::vec3 get_entity_local_eurler_degrees(EntityID entityID);
	glm::vec3 get_entity_local_scale(EntityID entityID);

	glm::vec3 get_entity_local_forward(EntityID entityID);
	glm::vec3 get_entity_local_right(EntityID entityID);
	glm::vec3 get_entity_local_up(EntityID entityID);

	glm::mat4 get_world_matrix(EntityID entityID);
	void set_parent_entity(EntityID child, EntityID parent, bool keepWorldTransform);
	void clear_parent_entity(EntityID child, bool keepWorldTransform);

	void sync_transforms();

	void start(EntityID entityID);
	void update(float deltaTime);
};
