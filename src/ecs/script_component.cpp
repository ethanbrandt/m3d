#include "script_component.h"
#include "../script_manager.h"

ScriptComponent::ScriptComponent()
{
	type = ComponentType::SCRIPT;	
}

ScriptComponent::~ScriptComponent()
{

}

void ScriptComponent::start()
{
	ScriptManager::instance->start_script(get_id());
}

void ScriptComponent::update(float deltaTime)
{
	//ScriptManager::instance->update_script(get_id(), deltaTime);
}

void ScriptComponent::on_destroy()
{
	ScriptManager::instance->destroy_script(get_id());
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
