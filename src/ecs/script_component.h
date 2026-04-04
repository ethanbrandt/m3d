#pragma once

#include <crossguid/guid.hpp>

#include "ecs.h"
#include "ecs_component.h"
#include "../script_manager.h"

class ScriptComponent : public Component
{
public:
	ScriptComponent();
	~ScriptComponent() override;

	void start(EntityID entityID) override;
	void update(EntityID entityID, float deltaTime) override;
	void on_destroy(EntityID entityID) override;

	void set_module_name(std::string _moduleName);
	std::string get_module_name() const;
private:
	std::string moduleName;	

protected:
	void on_initialize() override;
};