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

	void start() override;
	void update(float deltaTime) override;
	void on_destroy() override;

	void set_module_name(std::string _moduleName);
	std::string get_module_name() const;
private:
	std::string moduleName;	

protected:
	void on_initialize() override;
};