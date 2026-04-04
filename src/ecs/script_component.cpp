#include "script_component.h"
#include "../script_manager.h"

ScriptComponent::ScriptComponent()
{
	
}

ScriptComponent::~ScriptComponent()
{

}

void ScriptComponent::start(EntityID entityID)
{
	ScriptManager::instance->start_script(get_id());
}

void ScriptComponent::update(EntityID entityID, float deltaTime)
{
	ScriptManager::instance->update_script(get_id(), deltaTime);
}

void ScriptComponent::on_destroy(EntityID entityID)
{
	
}

void ScriptComponent::set_module_name(std::string _moduleName)
{
	moduleName = _moduleName;
}

std::string ScriptComponent::get_module_name() const
{
	return moduleName;
}

void ScriptComponent::on_initialize()
{
	ScriptManager::instance->register_script(get_entity_id(), get_id(), moduleName);
}
