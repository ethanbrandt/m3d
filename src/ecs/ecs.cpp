#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "ecs.h"

ECS* ECS::instance = nullptr;

bool ECS::would_create_cycle(EntityID child, EntityID parent)
{
	EntityID curr = parent;
	while (curr.isValid())
	{
		if (curr == child)
			return true;
		curr = records.at(curr).parentID;
	}

	return false;
}

void ECS::update_world_recursive(EntityID entityID, bool parentDirty)
{
	auto& record = records[entityID];
	bool isDirty = parentDirty || record.transform.localDirty || record.transform.worldDirty;

	if (isDirty)
	{
		record.transform.localMatrix = compose_transform(record.transform.localPosition, record.transform.localRotation, record.transform.localScale);

		if (record.parentID.isValid())
			record.transform.worldMatrix = records[record.parentID].transform.worldMatrix * record.transform.localMatrix;
		else
			record.transform.worldMatrix = record.transform.localMatrix;
		
		record.transform.localDirty = false;
		record.transform.worldDirty = false;
	}

	for (const auto& child : record.childrenIDs)
		update_world_recursive(child, isDirty);
}

void ECS::mark_transform_subtree_dirty(EntityID entityID)
{
	if (!is_entity_valid(entityID))
		return;
	
	records[entityID].transform.worldDirty = true;

	for (const auto& child : records[entityID].childrenIDs)
		mark_transform_subtree_dirty(child);
}

glm::mat4 ECS::compose_transform(const glm::vec3 &pos, const glm::quat &rot, const glm::vec3 &scale)
{
	glm::mat4 mat = glm::mat4(1.0f);

	mat = glm::translate(mat, pos);
	mat *= glm::mat4_cast(glm::normalize(rot));
	mat = glm::scale(mat, scale);

	return mat;
}

bool ECS::decompose_transform(const glm::mat4 &matrix, glm::vec3 &outPos, glm::quat &outRot, glm::vec3 &outScale)
{
	glm::vec3 skew;
	glm::vec4 perspective;

	if (!glm::decompose(matrix, outScale, outRot, outPos, skew, perspective))
		return false;
	
	outRot = glm::normalize(outRot);
	return true;
}

ECS::ECS()
{
	ECS::instance = this;
}

ECS::~ECS()
{
	records.clear();
	componentToEntity.clear();
	ECS::instance = nullptr;
}

EntityID ECS::create_entity_record(std::string name)
{
	EntityID entityID = xg::newGuid();
	records[entityID] = EntityRecord(entityID, name);

	return entityID;
}

void ECS::destroy_entity_record(EntityID entityID)
{
	auto it = records.find(entityID);
	if (it != records.end())
	{
		clear_parent_entity(entityID, false);

		std::vector<EntityID> children(it->second.childrenIDs.begin(), it->second.childrenIDs.end());
		for (const auto& child : children)
			destroy_entity_record(child);
		
		std::vector<ComponentID> componentIDs;
		componentIDs.reserve(it->second.components.size());
		for (auto& [componentID, component] : it->second.components)
			componentIDs.push_back(componentID);

		records.erase(it);

		for (const auto& componentID : componentIDs)
			componentToEntity.erase(componentID);
	}
}

bool ECS::is_entity_valid(EntityID entityID) const
{
	return (records.find(entityID) != records.end());
}

EntityID ECS::get_attached_entity(ComponentID id) const
{
	auto it = componentToEntity.find(id);
	if (it == componentToEntity.end())
	{
		std::cout << "does not exist\n";
		return xg::Guid();
	}
	return it->second;
}

ComponentID ECS::attach_component(EntityID entityID, std::unique_ptr<Component> component)
{
	auto it = records.find(entityID);
	if (it == records.end() || component == nullptr)
		return xg::Guid();

	if (!component->initialize(entityID))
		return xg::Guid();

	ComponentID componentID = component->get_id();
	it->second.components[componentID] = std::move(component);
	componentToEntity[componentID] = entityID;

	return componentID;
}

void ECS::remove_component(EntityID entityID, ComponentID componentID)
{
	auto entityIT = records.find(entityID);
	if (entityIT == records.end())
		return;	
	
	auto componentIT = entityIT->second.components.find(componentID);
	if (componentIT != entityIT->second.components.end())
	{
		componentIT->second->on_destroy();
		entityIT->second.components.erase(componentIT);
	}

	auto it = componentToEntity.find(componentID);
	if (it != componentToEntity.end())
		componentToEntity.erase(it);
}

EntityID ECS::get_parent_entity(EntityID entity)
{
	if (!is_entity_valid(entity))
		return { };

	return records[entity].parentID;
}

std::unordered_set<EntityID> ECS::get_children_entities(EntityID entity)
{
	if (!is_entity_valid(entity))
		return { };

	return records[entity].childrenIDs;
}

std::set<ComponentID> ECS::get_all_component_ids_of_type(EntityID entityID, ComponentType type) const
{
	auto entityIT = records.find(entityID);
	if (entityIT == records.end())
		return { };

	std::set<ComponentID> outSet;
	for (const auto& [componentID, component] : entityIT->second.components)
	{
		if (component && component->get_type() == type)
			outSet.insert(componentID);
	}

	return outSet;
}

bool ECS::is_component_valid(ComponentID id)
{
	return componentToEntity.find(id) != componentToEntity.end();
}

// This must always be paired with a is_component_valid() call before being called to ensure safety
Component &ECS::get_component_reference(ComponentID componentID)
{
	auto it = records.at(componentToEntity.at(componentID)).components.find(componentID);	
	return *it->second;
}

EntityID ECS::get_entity_by_name(std::string name) const
{
	for (const auto& [entityID, record] : records)
		if (record.name == name)
			return entityID;

	return { };
}

std::string ECS::get_entity_name(EntityID entityID)
{
	if (!is_entity_valid(entityID))
		return { };

	return records[entityID].name;
}

void ECS::set_entity_world_pos(EntityID entityID, glm::vec3 pos)
{
	EntityRecord& record = records.at(entityID);

	if (!record.parentID.isValid())
		record.transform.localPosition = pos;
	else
	{
		glm::vec4 local = glm::inverse(records[record.parentID].transform.worldMatrix) * glm::vec4(pos, 1.0f);
		record.transform.localPosition = glm::vec3(local);
	}

	record.transform.localDirty = true;
	mark_transform_subtree_dirty(entityID);
}

void ECS::set_entity_world_rot(EntityID entityID, glm::quat rot)
{
	EntityRecord& record = records.at(entityID);

	glm::quat localRot = rot;

	if (record.parentID.isValid())
	{
		glm::mat4 parentWorld = records.at(record.parentID).transform.worldMatrix;

		glm::vec3 parentScale;
		glm::quat parentWorldRot;
		glm::vec3 parentPos;
		glm::vec3 skew;
		glm::vec4 perspective;

		if (glm::decompose(parentWorld, parentScale, parentWorldRot, parentPos, skew, perspective))
		{
			parentWorldRot = glm::normalize(parentWorldRot);
			localRot = glm::inverse(parentWorldRot) * rot;
		}
	}

	localRot = glm::normalize(localRot);
	record.transform.localRotation = localRot;
	record.transform.localEulerDegrees = glm::degrees(glm::eulerAngles(localRot));
	record.transform.localDirty = true;
	mark_transform_subtree_dirty(entityID);
}

void ECS::set_entity_local_pos(EntityID entityID, glm::vec3 pos)
{
	records.at(entityID).transform.localPosition = pos;
	records.at(entityID).transform.localDirty = true;
	mark_transform_subtree_dirty(entityID);
}

void ECS::set_entity_local_rot(EntityID entityID, glm::vec3 eulerDegrees)
{
	records.at(entityID).transform.localRotation = glm::normalize(glm::quat(glm::radians(eulerDegrees)));
	records.at(entityID).transform.localEulerDegrees = eulerDegrees;
	records.at(entityID).transform.localDirty = true;
	mark_transform_subtree_dirty(entityID);
}

void ECS::set_entity_local_rot(EntityID entityID, glm::quat rot)
{
	rot = glm::normalize(rot);
	records.at(entityID).transform.localRotation = rot;
	records.at(entityID).transform.localEulerDegrees = glm::degrees(glm::eulerAngles(rot));
	records.at(entityID).transform.localDirty = true;
	mark_transform_subtree_dirty(entityID);
}

void ECS::set_entity_local_scale(EntityID entityID, glm::vec3 scale)
{
	if (scale.x <= 0 || scale.y <= 0 || scale.z <= 0)
		return;
	records.at(entityID).transform.localScale = scale;
	records.at(entityID).transform.localDirty = true;
	mark_transform_subtree_dirty(entityID);
}

glm::vec3 ECS::get_entity_local_pos(EntityID entityID)
{
	return records.at(entityID).transform.localPosition;
}

glm::quat ECS::get_entity_local_rot(EntityID entityID)
{
	return records.at(entityID).transform.localRotation;
}

glm::vec3 ECS::get_entity_local_eurler_degrees(EntityID entityID)
{
	return records.at(entityID).transform.localEulerDegrees;
}

glm::vec3 ECS::get_entity_local_scale(EntityID entityID)
{
	return records.at(entityID).transform.localScale;
}

glm::vec3 ECS::get_entity_local_forward(EntityID entityID)
{
	glm::quat rot = glm::normalize(records.at(entityID).transform.localRotation);
	return rot * glm::vec3(0.0f, 0.0f, -1.0f);
}

glm::vec3 ECS::get_entity_local_right(EntityID entityID)
{
	glm::quat rot = glm::normalize(records.at(entityID).transform.localRotation);
	return rot * glm::vec3(1.0f, 0.0f, 0.0f);
}

glm::vec3 ECS::get_entity_local_up(EntityID entityID)
{
	glm::quat rot = glm::normalize(records.at(entityID).transform.localRotation);
	return rot * glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::mat4 ECS::get_world_matrix(EntityID entityID)
{
	return records.at(entityID).transform.worldMatrix;
}

void ECS::set_parent_entity(EntityID child, EntityID parent, bool keepWorldTransform)
{
	if (!is_entity_valid(child) || !is_entity_valid(parent))
		return;

	if (child == parent)
		return;
	
	if (would_create_cycle(child, parent))
		return;
	
	EntityRecord& childRecord = records[child];
	EntityRecord& parentRecord = records[parent];

	glm::mat4 oldWorld = childRecord.transform.worldMatrix;

	EntityID oldParent = childRecord.parentID;
	if (oldParent.isValid())
		records[oldParent].childrenIDs.erase(child);
	
	childRecord.parentID = parent;
	parentRecord.childrenIDs.insert(child);

	if (keepWorldTransform)
	{
		glm::mat4 parentWorld = parentRecord.transform.worldMatrix;
		glm::mat4 local = glm::inverse(parentWorld) * oldWorld;

		decompose_transform(local, childRecord.transform.localPosition, childRecord.transform.localRotation, childRecord.transform.localScale);
		childRecord.transform.localEulerDegrees = glm::degrees(glm::eulerAngles(childRecord.transform.localRotation));

		childRecord.transform.localMatrix = local;
		childRecord.transform.localDirty = false;
	}
	else
		childRecord.transform.localDirty = true;

	mark_transform_subtree_dirty(child);
}

void ECS::clear_parent_entity(EntityID child, bool keepWorldTransform)
{
	if (!is_entity_valid(child))
		return;
	
	EntityID parent = records.at(child).parentID;
	if (!parent.isValid())
		return;
	auto it = records.at(parent).childrenIDs.find(child);
	if (it != records.at(parent).childrenIDs.end())
		records.at(parent).childrenIDs.erase(it);
	records.at(child).parentID = xg::Guid();

	EntityRecord& childRecord = records[child];
	glm::mat4 oldWorld = childRecord.transform.worldMatrix;	
	if (keepWorldTransform)
	{
		decompose_transform(oldWorld, childRecord.transform.localPosition, childRecord.transform.localRotation, childRecord.transform.localScale);
		childRecord.transform.localEulerDegrees = glm::degrees(glm::eulerAngles(childRecord.transform.localRotation));

		childRecord.transform.localMatrix = oldWorld;
		childRecord.transform.localDirty = true;
	}
	else
		childRecord.transform.localDirty = true;
	
	mark_transform_subtree_dirty(child);
}

void ECS::sync_transforms()
{
	for (auto& [entityID, record] : records)
		if (!record.parentID.isValid())
			update_world_recursive(entityID, false);
}

void ECS::start(EntityID entityID)
{
	auto entityIT = records.find(entityID);
	if (entityIT != records.end())
		for (auto& [componentID, component] : entityIT->second.components)
			component->start();
}

void ECS::update(float deltaTime)
{
	for (auto& [entityID, entityRecord] : records)
		for (auto& [componentID, component] : entityRecord.components)
			component->update(deltaTime);
}
