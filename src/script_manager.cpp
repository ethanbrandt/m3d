#include <iostream>
#include <chrono>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>

#include <wren.hpp>

#include "script_manager.h"
#include "ecs.h"
#include "input.h"
#include "ecs/script_component.h"
#include "ecs/rigid_body_component.h"
#include "ecs/collider_component.h"
#include "ecs/box_collider_component.h"
#include "ecs/sphere_collider_component.h"
#include "ecs/capsule_collider_component.h"
#include "physics/physics.h"

ScriptManager* ScriptManager::instance = nullptr;
#pragma region INITIALIZATION
char* read_file(std::string path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
		return nullptr;

	std::ostringstream stream;
	stream << file.rdbuf();
	const std::string source = stream.str();

	char* buff = new char[source.size() + 1];
	if (buff == nullptr)
		return nullptr;
	
	std::memcpy(buff, source.c_str(), source.size());
	buff[source.size()] = '\0';
	return buff;
}

WrenLoadModuleResult ScriptManager::load_module(WrenVM *, const char *name)
{
	WrenLoadModuleResult result = {};
	std::string path = std::string("scripts/") + name + ".wren";
	char* source = read_file(path);

	if (source == nullptr)
		return result;
	
	result.source = source;
	result.userData = source;
	result.onComplete = free_loaded_module;
	return result;
}

void ScriptManager::write_output(WrenVM *, const char *text)
{
	std::cout << text;
}

void ScriptManager::report_error(WrenVM *, WrenErrorType type, const char *module, int line, const char *message)
{
	switch(type)
	{
		case WREN_ERROR_COMPILE:
			std::cout << "[Wren][Compile Error] " << module << ":" << line << " " << message << '\n';
			break;
		case WREN_ERROR_RUNTIME:
			std::cout << "[Wren][Runtime Error] " << message << '\n';
			break;	
		case WREN_ERROR_STACK_TRACE:
			std::cout << "[Wren][Stack Error] " << module << ":" << line << " " << message << '\n';
			break;
	}
}

void ScriptManager::free_loaded_module(WrenVM *, const char *, WrenLoadModuleResult result)
{
	delete[] static_cast<char*>(result.userData);
}

WrenHandle *ScriptManager::get_script_class(std::string moduleName)
{
	auto it = scriptClasses.find(moduleName);
	if (it != scriptClasses.end())
		return it->second;
	
	std::string bootstrap = "import \"" + moduleName + "\" for GameScript";
	std::string bootstrapModule = "__load_" + moduleName;

	if (wrenInterpret(vm, bootstrapModule.c_str(), bootstrap.c_str()) != WREN_RESULT_SUCCESS)
		return nullptr;
	
	wrenEnsureSlots(vm, 1);
	wrenGetVariable(vm, moduleName.c_str(), "GameScript", 0);
	if (wrenGetSlotType(vm, 0) == WREN_TYPE_NULL)
		return nullptr;
	
	WrenHandle* wrenClass = wrenGetSlotHandle(vm, 0);
	scriptClasses[moduleName] = wrenClass;
	return wrenClass;
}

bool ScriptManager::attach_script(EntityID entityID, ComponentID componentID, std::string moduleName)
{
	WrenHandle* wrenClass = get_script_class(moduleName);
	if (!wrenClass)
		return false;
	
	wrenEnsureSlots(vm, 3);
	wrenSetSlotHandle(vm, 0, wrenClass);
	wrenSetSlotHandle(vm, 2, gameObjectClass);

	auto* ownerRef = static_cast<GameObjectData*>(wrenSetSlotNewForeign(vm, 1, 2, sizeof(GameObjectData)));
	new (ownerRef) GameObjectData{};
	ownerRef->entityID = entityID;

	if (wrenCall(vm, ctorNew) != WREN_RESULT_SUCCESS)
		return false;
	
	ScriptBinding script = {};
	script.id = entityID;
	script.moduleName = moduleName;
	script.scriptHandle = wrenGetSlotHandle(vm, 0);
	scripts.emplace(componentID, script);

	wrenEnsureSlots(vm, 1);
	wrenSetSlotHandle(vm, 0, script.scriptHandle);
	return true;
}

bool ScriptManager::initialize()
{
	wrenInitConfiguration(&config);
	config.loadModuleFn = load_module;
	config.bindForeignClassFn = bind_foreign_class;
	config.bindForeignMethodFn = bind_foreign_method;
	config.writeFn = write_output;
	config.errorFn = report_error;

	vm = wrenNewVM(&config);
	if (vm == nullptr)
	{
		std::cout << "Error: Failed to create Wren VM\n";
		return false;
	}
	ctorNew = wrenMakeCallHandle(vm, "new(_)");
	onStart = wrenMakeCallHandle(vm, "on_start()");
	onUpdate = wrenMakeCallHandle(vm, "on_update(_)");


	const char* bootstrap = "import \"m3d\" for GameObject";
	if (wrenInterpret(vm, "__m3d_bootstrap", bootstrap) != WREN_RESULT_SUCCESS)
		return false;

	wrenEnsureSlots(vm, 1);
	wrenGetVariable(vm, "m3d", "GameObject", 0);
	if (wrenGetSlotType(vm, 0) == WREN_TYPE_NULL)
		return false;
	gameObjectClass = wrenGetSlotHandle(vm, 0);

	return true;
}

ScriptManager::ScriptManager()
{
	ScriptManager::instance = this;
}

ScriptManager::~ScriptManager()
{
	ScriptManager::instance = nullptr;

	for (auto& [id, script] : scripts)
		if (script.scriptHandle)
			wrenReleaseHandle(vm, script.scriptHandle);
	
	for (auto& [_, wrenClass] : scriptClasses)
		if (wrenClass)
			wrenReleaseHandle(vm, wrenClass);
	
	if (gameObjectClass)
		wrenReleaseHandle(vm, gameObjectClass);
	if (onStart)
		wrenReleaseHandle(vm, onStart);
	if (onUpdate)
		wrenReleaseHandle(vm, onUpdate);
	if (ctorNew)
		wrenReleaseHandle(vm, ctorNew);
	
	if (vm)
		wrenFreeVM(vm);
}

void ScriptManager::register_script(EntityID entityID, ComponentID componentID, std::string scriptFilePath)
{
	attach_script(entityID, componentID, scriptFilePath);
	//TODO scriptFilePath stuff
}

void ScriptManager::remove_dirty_scripts()
{
	for (auto id : dirtyScripts)
	{
		if (!id.isValid() || scripts.find(id) == scripts.end())
			continue;
		
		if (scripts.at(id).scriptHandle != nullptr)
			wrenReleaseHandle(vm, scripts.at(id).scriptHandle);
		
		scripts.erase(id);
	}

	dirtyScripts.clear();
}

void ScriptManager::start_script(ComponentID id)
{
	wrenEnsureSlots(vm, 1);
	wrenSetSlotHandle(vm, 0, scripts.at(id).scriptHandle);
	if (wrenCall(vm, onStart))
		std::cout << "[Script System Error] Failed to run on_start in " + scripts.at(id).moduleName + '\n';
}

void ScriptManager::update_script(ComponentID id, float deltaTime)
{
	wrenEnsureSlots(vm, 2);
	wrenSetSlotHandle(vm, 0, scripts.at(id).scriptHandle);
	wrenSetSlotDouble(vm, 1, deltaTime);
	if (wrenCall(vm, onUpdate))
		std::cout << "Error: failed to run on_update in " + scripts.at(id).moduleName + '\n';
}

void ScriptManager::update_all_scripts(float deltaTime)
{
	for (auto& [componentID, scriptBinding] : scripts)
	{
		wrenEnsureSlots(vm, 2);
		wrenSetSlotHandle(vm, 0, scriptBinding.scriptHandle);
		wrenSetSlotDouble(vm, 1, deltaTime);
		if (wrenCall(vm, onUpdate))
			std::cout << "[Script System Error] Failed to run on_update in " + scriptBinding.moduleName + '\n';
	}
}

void ScriptManager::destroy_script(ComponentID id)
{
	dirtyScripts.insert(id);
}

void ScriptManager::force_garbage_collect()
{
	wrenCollectGarbage(vm);
}

#pragma endregion

#pragma region FOREIGN_BINDINGS
WrenForeignClassMethods ScriptManager::bind_foreign_class(WrenVM *, const char *module, const char *className)
{
	WrenForeignClassMethods methods = {};

	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "GameObject") == 0)
	{
		methods.allocate = game_object_allocate;
		methods.finalize = game_object_finalize;
	}

	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "Vector2") == 0)
	{
		methods.allocate = vector2_allocate;
		methods.finalize = vector2_finalize;
	}

	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "Vector3") == 0)
	{
		methods.allocate = vector3_allocate;
		methods.finalize = vector3_finalize;
	}

	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "Rigidbody") == 0)
	{
		methods.allocate = rigidbody_allocate;
		methods.finalize = rigidbody_finalize;
	}

	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "BoxCollider") == 0)
	{
		methods.allocate = box_collider_allocate;
		methods.finalize = collider_finalize;
	}
	
	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "SphereCollider") == 0)
	{
		methods.allocate = sphere_collider_allocate;
		methods.finalize = collider_finalize;
	}

	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "CapsuleCollider") == 0)
	{
		methods.allocate = capsule_collider_allocate;
		methods.finalize = collider_finalize;
	}

	return methods;
}

WrenForeignMethodFn ScriptManager::bind_foreign_method(WrenVM *vm, const char *module, const char *className, bool isStatic, const char *signature)
{
	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "Input") == 0 && isStatic)
	{
		if (std::strcmp(signature, "is_key_pressed(_)") == 0)
			return input_is_key_pressed;
		if (std::strcmp(signature, "was_key_pressed_down(_)") == 0)
			return input_was_key_pressed_down;
		if (std::strcmp(signature, "was_key_released(_)") == 0)
			return input_was_key_released;
		
		if (std::strcmp(signature, "is_mouse_button_pressed(_)") == 0)
			return input_is_mouse_button_pressed;
		if (std::strcmp(signature, "was_mouse_button_pressed_down(_)") == 0)
			return input_was_mouse_button_pressed_down;
		if (std::strcmp(signature, "was_mouse_button_released(_)") == 0)
			return input_was_mouse_button_released;

		if (std::strcmp(signature, "get_mouse_position()") == 0)
			return input_get_mouse_position;
		if (std::strcmp(signature, "get_mouse_delta()") == 0)
			return input_get_mouse_delta;
		if (std::strcmp(signature, "get_mouse_scroll_delta()") == 0)
			return input_get_mouse_scroll_delta;
	}

	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "GameObject") == 0)
	{
		if (std::strcmp(signature, "position=(_)") == 0)
			return game_object_set_local_position;
		if (std::strcmp(signature, "position") == 0)
			return gameobject_get_local_position;

		if (std::strcmp(signature, "eulerDegrees=(_)") == 0)
			return game_object_set_local_euler_degrees;
		if (std::strcmp(signature, "eulerDegrees") == 0)
			return gameobject_get_local_euler_degrees;

		if (std::strcmp(signature, "scale=(_)") == 0)
			return game_object_set_local_scale;
		if (std::strcmp(signature, "scale") == 0)
			return gameobject_get_local_scale;

		if (std::strcmp(signature, "localForward") == 0)
			return game_object_get_local_forward;
		if (std::strcmp(signature, "localRight") == 0)
			return game_object_get_local_right;
		if (std::strcmp(signature, "localUp") == 0)
			return game_object_get_local_up;

		if (std::strcmp(signature, "id") == 0)
			return game_object_get_id;
		if (std::strcmp(signature, "name") == 0)
			return game_object_get_name;
		if (std::strcmp(signature, "parent") == 0)
			return game_object_get_parent;

		if (std::strcmp(signature, "get_script(_)") == 0)
			return game_object_get_script;
		if (std::strcmp(signature, "get_components_by_type(_)") == 0)
			return game_object_get_components_by_type;

		if (std::strcmp(signature, "attach_component(_)") == 0)
			return game_object_attach_component;
		if (std::strcmp(signature, "remove_component(_)") == 0)
			return game_object_remove_component;
	}

	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "Scene") == 0 && isStatic)
	{
		if (std::strcmp(signature, "find_object_by_name(_)") == 0)
			return scene_find_object_by_name;
	}

	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "Vector2") == 0)
	{
		if (std::strcmp(signature, "x") == 0)
			return vector2_get_x;
		if (std::strcmp(signature, "x=(_)") == 0)
			return vector2_set_x;
	
		if (std::strcmp(signature, "y") == 0)
			return vector2_get_y;
		if (std::strcmp(signature, "y=(_)") == 0)
			return vector2_set_y;
	
		if (std::strcmp(signature, "+(_)") == 0)
			return vector2_add;
		if (std::strcmp(signature, "-(_)") == 0)
			return vector2_sub;	
		if (std::strcmp(signature, "*(_)") == 0)
			return vector2_scalar_multiply;
	}

	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "Vector3") == 0)
	{
		if (std::strcmp(signature, "x") == 0)
			return vector3_get_x;
		if (std::strcmp(signature, "x=(_)") == 0)
			return vector3_set_x;
	
		if (std::strcmp(signature, "y") == 0)
			return vector3_get_y;
		if (std::strcmp(signature, "y=(_)") == 0)
			return vector3_set_y;

		if (std::strcmp(signature, "z") == 0)
			return vector3_get_z;
		if (std::strcmp(signature, "z=(_)") == 0)
			return vector3_set_z;

		if (std::strcmp(signature, "+(_)") == 0)
			return vector3_add;
		if (std::strcmp(signature, "-(_)") == 0)
			return vector3_sub;
		if (std::strcmp(signature, "*(_)") == 0)
			return vector3_scalar_multiply;
	}

	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "Rigidbody") == 0)
	{
		if (std::strcmp(signature, "friction") == 0)
			return rigidbody_get_friction;
		if (std::strcmp(signature, "friction=(_)") == 0)
			return rigidbody_set_friction;

		if (std::strcmp(signature, "restitution") == 0)
			return rigidbody_get_restitution;
		if (std::strcmp(signature, "restitution=(_)") == 0)
			return rigidbody_set_restitution;

		if (std::strcmp(signature, "mass") == 0)
			return rigidbody_get_mass;
		if (std::strcmp(signature, "mass=(_)") == 0)
			return rigidbody_set_mass;

		if (std::strcmp(signature, "gravityFactor") == 0)
			return rigidbody_get_gravity_factor;
		if (std::strcmp(signature, "gravityFactor=(_)") == 0)
			return rigidbody_set_gravity_factor;

		if (std::strcmp(signature, "isTrigger") == 0)
			return rigidbody_get_is_trigger;
		if (std::strcmp(signature, "isTrigger=(_)") == 0)
			return rigidbody_set_is_trigger;

		if (std::strcmp(signature, "linearVelocity") == 0)
			return rigidbody_get_linear_velocity;
		if (std::strcmp(signature, "linearVelocity=(_)") == 0)
			return rigidbody_set_linear_velocity;

		if (std::strcmp(signature, "angularVelocity") == 0)
			return rigidbody_get_angular_velocity;
		if (std::strcmp(signature, "angularVelocity=(_)") == 0)
			return rigidbody_set_angular_velocity;

		if (std::strcmp(signature, "add_force(_)") == 0)
			return rigidbody_add_force;
		if (std::strcmp(signature, "add_impulse(_)") == 0)
			return rigidbody_add_impulse;
		if (std::strcmp(signature, "add_angular_force(_)") == 0)
			return rigidbody_add_angular_force;
		if (std::strcmp(signature, "add_angular_impulse(_)") == 0)
			return rigidbody_add_angular_impulse;
	}

	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "BoxCollider") == 0)
	{
		if (std::strcmp(signature, "positionOffset") == 0)
			return collider_get_position_offset;
		if (std::strcmp(signature, "positionOffset=(_)") == 0)
			return collider_set_position_offset;

		if (std::strcmp(signature, "eulerDegreesOffset") == 0)
			return collider_get_euler_offset;
		if (std::strcmp(signature, "eulerDegreesOffset=(_)") == 0)
			return collider_set_euler_offset;

		if (std::strcmp(signature, "halfDimensions") == 0)
			return box_collider_get_half_dimensions;
		if (std::strcmp(signature, "halfDimensions=(_)") == 0)
			return box_collider_set_half_dimensions;
	}

	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "SphereCollider") == 0)
	{
		if (std::strcmp(signature, "positionOffset") == 0)
			return collider_get_position_offset;
		if (std::strcmp(signature, "positionOffset=(_)") == 0)
			return collider_set_position_offset;

		if (std::strcmp(signature, "eulerDegreesOffset") == 0)
			return collider_get_euler_offset;
		if (std::strcmp(signature, "eulerDegreesOffset=(_)") == 0)
			return collider_set_euler_offset;

		if (std::strcmp(signature, "radius") == 0)
			return sphere_collider_get_radius;
		if (std::strcmp(signature, "radius=(_)") == 0)
			return sphere_collider_set_radius;
	}

	if (std::strcmp(module, "m3d") == 0 && std::strcmp(className, "CapsuleCollider") == 0)
	{
		if (std::strcmp(signature, "positionOffset") == 0)
			return collider_get_position_offset;
		if (std::strcmp(signature, "positionOffset=(_)") == 0)
			return collider_set_position_offset;

		if (std::strcmp(signature, "eulerDegreesOffset") == 0)
			return collider_get_euler_offset;
		if (std::strcmp(signature, "eulerDegreesOffset=(_)") == 0)
			return collider_set_euler_offset;

		if (std::strcmp(signature, "radius") == 0)
			return capsule_collider_get_radius;
		if (std::strcmp(signature, "radius=(_)") == 0)
			return capsule_collider_set_radius;

		if (std::strcmp(signature, "halfHeight") == 0)
			return capsule_collider_get_half_height;
		if (std::strcmp(signature, "halfHeight=(_)") == 0)
			return capsule_collider_set_half_height;
	}

	return nullptr;
}

#pragma region GAME_OBJECT_BINDINGS
void ScriptManager::game_object_allocate(WrenVM *vm)
{
	void* data = wrenSetSlotNewForeign(vm, 0, 0, sizeof(GameObjectData));
	new (data) GameObjectData{};
}

void ScriptManager::game_object_finalize(void *data)
{
	static_cast<GameObjectData*>(data)->~GameObjectData();
}

void ScriptManager::game_object_get_id(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));
	std::string id = ref->entityID.str();
	wrenSetSlotString(vm, 0, id.c_str());
}

void ScriptManager::game_object_get_parent(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));
	EntityID parentID = ECS::instance->get_parent_entity(ref->entityID);

	if (!parentID.isValid())
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "GameObject", 1);

	auto* parentRef = static_cast<GameObjectData*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(GameObjectData)));

	new (parentRef) GameObjectData{};
	parentRef->entityID = parentID;
}

void ScriptManager::game_object_get_name(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));
	std::string name = ECS::instance->get_entity_name(ref->entityID);

	wrenSetSlotString(vm, 0, name.c_str());
}

void ScriptManager::gameobject_get_local_position(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector3", 1);

	auto* value = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));

	new (value) Vector3Data(ECS::instance->get_entity_local_pos(ref->entityID));
}

void ScriptManager::game_object_set_local_position(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));
	auto* value = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 1));
	ECS::instance->set_entity_local_pos(ref->entityID, *value);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::gameobject_get_local_euler_degrees(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector3", 1);

	auto* value = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));

	new (value) Vector3Data(ECS::instance->get_entity_local_eurler_degrees(ref->entityID));
}

void ScriptManager::game_object_set_local_euler_degrees(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));
	auto* value = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 1));
	ECS::instance->set_entity_local_rot(ref->entityID, *value);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::gameobject_get_local_scale(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector3", 1);

	auto* value = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));

	new (value) Vector3Data(ECS::instance->get_entity_local_scale(ref->entityID));
}

void ScriptManager::game_object_set_local_scale(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));
	auto* value = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 1));
	ECS::instance->set_entity_local_scale(ref->entityID, *value);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::game_object_get_local_forward(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector3", 1);

	auto* value = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));
	new (value) Vector3Data(ECS::instance->get_entity_local_forward(ref->entityID));
}

void ScriptManager::game_object_get_local_right(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector3", 1);

	auto* value = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));
	new (value) Vector3Data(ECS::instance->get_entity_local_right(ref->entityID));
}

void ScriptManager::game_object_get_local_up(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector3", 1);

	auto* value = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));
	new (value) Vector3Data(ECS::instance->get_entity_local_up(ref->entityID));
}

void ScriptManager::game_object_get_script(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));
	const char* moduleName = wrenGetSlotString(vm, 1);

	ScriptBinding* binding = nullptr;
	std::set<ComponentID> components = ECS::instance->get_all_component_ids_of_type(ref->entityID, ComponentType::SCRIPT);
	for (const auto& id : components)
	{
		auto it = ScriptManager::instance->scripts.find(id);
		if (it != ScriptManager::instance->scripts.end() && it->second.moduleName == moduleName)
			binding = &(it->second);
	}

	if (!binding || binding->scriptHandle == nullptr)
	{
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	wrenSetSlotHandle(vm, 0, binding->scriptHandle);
}

void ScriptManager::game_object_get_components_by_type(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));
	int typeEnum = std::rint(wrenGetSlotDouble(vm, 1));

	const char* wrenClassName = nullptr;
	ComponentType type;
	ColliderType colType;

	switch (typeEnum)
	{
		case 0: // RIGIDBODY
			wrenClassName = "Rigidbody";
			type = ComponentType::RIGID_BODY;
			break;
		
		case 1: // BOX_COLLIDER
			wrenClassName = "BoxCollider";
			type = ComponentType::COLLIDER;
			colType = ColliderType::BOX;
			break;

		case 2: // SPHERE_COLLIDER
			wrenClassName = "SphereCollider";
			type = ComponentType::COLLIDER;
			colType = ColliderType::SPHERE;
			break;

		case 3: // CAPSULE_COLLIDER
			wrenClassName = "CapsuleCollider";
			type = ComponentType::COLLIDER;
			colType = ColliderType::CAPSULE;
			break;

		default:
			wrenEnsureSlots(vm, 1);
			wrenSetSlotNewList(vm, 0);
			return;
	}

	if (!wrenClassName)
	{
		wrenEnsureSlots(vm, 1);
		wrenSetSlotNewList(vm, 0);
		return;
	}

	std::set<ComponentID> components = ECS::instance->get_all_component_ids_of_type(ref->entityID, type);

	wrenEnsureSlots(vm, 4);
	wrenSetSlotNewList(vm, 0);
	wrenGetVariable(vm, "m3d", wrenClassName, 1);

	for (const auto& id : components)
	{
		switch (type)
		{
			case ComponentType::RIGID_BODY:
			{
				auto* data = static_cast<RigidbodyData*>(wrenSetSlotNewForeign(vm, 2, 1, sizeof(RigidbodyData)));
				
				new (data) RigidbodyData{};
				data->header.componentID = id;
				data->header.type = ComponentType::RIGID_BODY;

				wrenInsertInList(vm, 0, -1, 2);
				break;
			}

			case ComponentType::COLLIDER:
			{
				if (!ECS::instance->is_component_valid(id))
					break;
				
				auto& collider = dynamic_cast<Collider&>(ECS::instance->get_component_reference(id));
				if (collider.get_collider_type() != colType)
					break;

				switch (colType)
				{
					case ColliderType::BOX:
					{
						auto* data = static_cast<BoxColliderData*>(wrenSetSlotNewForeign(vm, 2, 1, sizeof(BoxColliderData)));
				
						new (data) BoxColliderData{};
						data->header.componentID = id;
						data->header.type = ComponentType::COLLIDER;
						data->colliderType = ColliderType::BOX;
						
						wrenInsertInList(vm, 0, -1, 2);
						break;
					}
					
					case ColliderType::SPHERE:
					{
						auto* data = static_cast<SphereColliderData*>(wrenSetSlotNewForeign(vm, 2, 1, sizeof(SphereColliderData)));
				
						new (data) SphereColliderData{};
						data->header.componentID = id;
						data->header.type = ComponentType::COLLIDER;
						data->colliderType = ColliderType::SPHERE;
						
						wrenInsertInList(vm, 0, -1, 2);
						break;
					}

					case ColliderType::CAPSULE:
					{
						auto* data = static_cast<CapsuleColliderData*>(wrenSetSlotNewForeign(vm, 2, 1, sizeof(CapsuleColliderData)));
				
						new (data) CapsuleColliderData{};
						data->header.componentID = id;
						data->header.type = ComponentType::COLLIDER;
						data->colliderType = ColliderType::CAPSULE;

						wrenInsertInList(vm, 0, -1, 2);
						break;
					}
				}
				break;	
			}
		}
	}
}

void ScriptManager::game_object_attach_component(WrenVM *vm)
{
	auto* obj = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));
	auto* header = static_cast<ComponentHeader*>(wrenGetSlotForeign(vm, 1));

	if (header->componentID.isValid() || ECS::instance->is_component_valid(header->componentID))
	{
		wrenSetSlotBool(vm, 0, false);
		return;
	}

	ComponentID id;

	switch (header->type)
	{
		case ComponentType::RIGID_BODY:
		{
			auto* data = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 1));
			auto rb = std::make_unique<RigidBody>();

			BodyDesc desc;
			desc.friction = data->friction;
			desc.gravityFactor = data->gravityFactor;
			desc.mass = data->mass;
			desc.isSensor = data->isSensor;
			desc.restitution = data->restitution;

			rb->set_body_desc(desc);
			id = ECS::instance->attach_component(obj->entityID, std::move(rb));
			break;
		}

		case ComponentType::COLLIDER:
		{
			auto* data = static_cast<ColliderData*>(wrenGetSlotForeign(vm, 1));
			switch (data->colliderType)
			{
				case ColliderType::BOX:
				{
					auto* boxData = static_cast<BoxColliderData*>(data);
					auto boxCollider = std::make_unique<BoxCollider>();

					boxCollider->set_position_offset(boxData->positionOffset);
					boxCollider->set_euler_degree_offset(boxData->eulerDegreeOffset);
					boxCollider->set_half_dimensions(boxData->halfDimensions);

					id = ECS::instance->attach_component(obj->entityID, std::move(boxCollider));
					break;
				}

				case ColliderType::SPHERE:
				{
					auto* sphereData = static_cast<SphereColliderData*>(data);
					auto sphereCollider = std::make_unique<SphereCollider>();

					sphereCollider->set_position_offset(sphereData->positionOffset);
					sphereCollider->set_euler_degree_offset(sphereData->eulerDegreeOffset);
					sphereCollider->set_radius(sphereData->radius);

					id = ECS::instance->attach_component(obj->entityID, std::move(sphereCollider));
					break;
				}

				case ColliderType::CAPSULE:
				{
					auto* capsuleData = static_cast<CapsuleColliderData*>(data);
					auto capsuleCollider = std::make_unique<CapsuleCollider>();

					capsuleCollider->set_position_offset(capsuleData->positionOffset);
					capsuleCollider->set_euler_degree_offset(capsuleData->eulerDegreeOffset);
					capsuleCollider->set_radius(capsuleData->radius);
					capsuleCollider->set_half_height(capsuleData->halfHeight);

					id = ECS::instance->attach_component(obj->entityID, std::move(capsuleCollider));
					break;
				}
			}
			break;
		}
	}

	if (!id.isValid())
	{
		wrenSetSlotBool(vm, 0, false);
		return;
	}

	header->componentID = id;
	wrenSetSlotBool(vm, 0, true);
}

void ScriptManager::game_object_remove_component(WrenVM *vm)
{
	auto* obj = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));
	auto* header = static_cast<ComponentHeader*>(wrenGetSlotForeign(vm, 1));

	if (!header->componentID.isValid() || !ECS::instance->is_component_valid(header->componentID))
	{
		wrenSetSlotBool(vm, 0, false);
		return;
	}

	EntityID attachedEntity = ECS::instance->get_attached_entity(header->componentID);
	if (!attachedEntity.isValid() || attachedEntity != obj->entityID)
	{
		wrenSetSlotBool(vm, 0, false);
		return;
	}

	ECS::instance->remove_component(attachedEntity, header->componentID);
	header->componentID = xg::Guid();
	wrenSetSlotBool(vm, 0, true);
}
#pragma endregion

#pragma region SCENE_BINDINGS
void ScriptManager::scene_find_object_by_name(WrenVM *vm)
{
	const char* objName = wrenGetSlotString(vm, 1);
	
	EntityID entityID = ECS::instance->get_entity_by_name(objName);
	
	if (!entityID.isValid())
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "GameObject", 1);

	auto* data = static_cast<GameObjectData*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(GameObjectData)));
	new (data) GameObjectData{};
	data->entityID = entityID;
}
#pragma endregion

#pragma region VECTOR2_BINDINGS
void ScriptManager::vector2_allocate(WrenVM *vm)
{
	auto* data = static_cast<Vector2Data*>(wrenSetSlotNewForeign(vm, 0, 0, sizeof(Vector2Data)));
	float x = wrenGetSlotDouble(vm, 1);
	float y = wrenGetSlotDouble(vm, 2);
	new (data) Vector2Data(x, y);
}

void ScriptManager::vector2_finalize(void *data)
{

}

void ScriptManager::vector2_get_x(WrenVM *vm)
{
	auto* ref = static_cast<Vector2Data*>(wrenGetSlotForeign(vm, 0));
	wrenSetSlotDouble(vm, 0, ref->x);
}

void ScriptManager::vector2_set_x(WrenVM *vm)
{
	auto* ref = static_cast<Vector2Data*>(wrenGetSlotForeign(vm, 0));
	ref->x = wrenGetSlotDouble(vm, 1);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::vector2_get_y(WrenVM *vm)
{
	auto* ref = static_cast<Vector2Data*>(wrenGetSlotForeign(vm, 0));
	wrenSetSlotDouble(vm, 0, ref->y);
}

void ScriptManager::vector2_set_y(WrenVM *vm)
{
	auto* ref = static_cast<Vector2Data*>(wrenGetSlotForeign(vm, 0));
	ref->y = wrenGetSlotDouble(vm, 1);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::vector2_add(WrenVM *vm)
{
	auto* ref = static_cast<Vector2Data*>(wrenGetSlotForeign(vm, 0));
	auto* other = static_cast<Vector2Data*>(wrenGetSlotForeign(vm, 1));

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector2", 1);

	auto* res = static_cast<Vector2Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector2Data)));
	new (res) Vector2Data(ref->x + other->x, ref->y + other->y);
}

void ScriptManager::vector2_sub(WrenVM *vm)
{
	auto* ref = static_cast<Vector2Data*>(wrenGetSlotForeign(vm, 0));
	auto* other = static_cast<Vector2Data*>(wrenGetSlotForeign(vm, 1));

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector2", 1);

	auto* res = static_cast<Vector2Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector2Data)));
	new (res) Vector2Data(ref->x + other->x, ref->y + other->y);
}

void ScriptManager::vector2_scalar_multiply(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_NUM)
	{
		wrenEnsureSlots(vm, 2);
		wrenSetSlotString(vm, 1, "Vector2.*(_) expects a num scalar");
		wrenAbortFiber(vm, 1);
		return;
	}

	auto* ref = static_cast<Vector2Data*>(wrenGetSlotForeign(vm, 0));
	float scalar = wrenGetSlotDouble(vm, 1);

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector2", 1);

	auto* res = static_cast<Vector2Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector2Data)));
	new (res) Vector2Data(ref->x * scalar, ref->y * scalar);
}
#pragma endregion

#pragma region VECTOR3_BINDINGS
void ScriptManager::vector3_allocate(WrenVM *vm)
{
	auto* data = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 0, sizeof(Vector3Data)));
	float x = wrenGetSlotDouble(vm, 1);
	float y = wrenGetSlotDouble(vm, 2);
	float z = wrenGetSlotDouble(vm, 3);
	new (data) Vector3Data(x, y, z);
}

void ScriptManager::vector3_finalize(void *data)
{

}

void ScriptManager::vector3_get_x(WrenVM *vm)
{
	auto* ref = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 0));
	wrenSetSlotDouble(vm, 0, ref->x);
}

void ScriptManager::vector3_set_x(WrenVM *vm)
{
	auto* ref = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 0));
	ref->x = wrenGetSlotDouble(vm, 1);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::vector3_get_y(WrenVM *vm)
{
	auto* ref = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 0));
	wrenSetSlotDouble(vm, 0, ref->y);
}

void ScriptManager::vector3_set_y(WrenVM *vm)
{
	auto* ref = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 0));
	ref->y = wrenGetSlotDouble(vm, 1);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::vector3_get_z(WrenVM *vm)
{
	auto* ref = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 0));
	wrenSetSlotDouble(vm, 0, ref->z);
}

void ScriptManager::vector3_set_z(WrenVM *vm)
{
	auto* ref = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 0));
	ref->z = wrenGetSlotDouble(vm, 1);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::vector3_add(WrenVM *vm)
{
	auto* ref = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 0));
	auto* other = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 1));

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector3", 1);

	auto* res = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));
	new (res) Vector3Data(ref->x + other->x, ref->y + other->y, ref->z + other->z);
}

void ScriptManager::vector3_sub(WrenVM *vm)
{
	auto* ref = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 0));
	auto* other = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 1));

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector3", 1);

	auto* res = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));
	new (res) Vector3Data(ref->x - other->x, ref->y - other->y, ref->z - other->z);
}

void ScriptManager::vector3_scalar_multiply(WrenVM *vm)
{
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_NUM)
	{
		wrenEnsureSlots(vm, 2);
		wrenSetSlotString(vm, 1, "Vector3.*(_) expects a num scalar");
		wrenAbortFiber(vm, 1);
		return;
	}

	auto* ref = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 0));
	float scalar = wrenGetSlotDouble(vm, 1);

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector3", 1);

	auto* res = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));
	new (res) Vector3Data(ref->x * scalar, ref->y * scalar, ref->z * scalar);
}
#pragma endregion

#pragma region RIGIDBODY_BINDINGS
void ScriptManager::rigidbody_allocate(WrenVM *vm)
{
	auto* data = static_cast<RigidbodyData*>(wrenSetSlotNewForeign(vm, 0, 0, sizeof(RigidbodyData)));

	new (data) RigidbodyData{};
	data->header.type = ComponentType::RIGID_BODY;
	data->header.componentID = {};
}

void ScriptManager::rigidbody_finalize(void *data)
{

}

void ScriptManager::rigidbody_get_motion_type(WrenVM *vm)
{

}

void ScriptManager::rigidbody_set_motion_type(WrenVM *vm)
{
	
}

void ScriptManager::rigidbody_get_friction(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));

	if (!ref->header.componentID.isValid())
	{
		wrenSetSlotDouble(vm, 0, ref->friction);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	wrenSetSlotDouble(vm, 0, rb.get_friction());
}

void ScriptManager::rigidbody_set_friction(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));
	float value = wrenGetSlotDouble(vm, 1);

	if (!ref->header.componentID.isValid())
	{
		ref->friction = value;
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	rb.set_friction(value);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::rigidbody_get_restitution(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));

	if (!ref->header.componentID.isValid())
	{
		wrenSetSlotDouble(vm, 0, ref->restitution);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	wrenSetSlotDouble(vm, 0, rb.get_restitution());
}

void ScriptManager::rigidbody_set_restitution(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));
	float value = wrenGetSlotDouble(vm, 1);

	if (!ref->header.componentID.isValid())
	{
		ref->restitution = value;
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	rb.set_restitution(value);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::rigidbody_get_mass(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));

	if (!ref->header.componentID.isValid())
	{
		wrenSetSlotDouble(vm, 0, ref->mass);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	wrenSetSlotDouble(vm, 0, rb.get_mass());
}

void ScriptManager::rigidbody_set_mass(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));
	float value = wrenGetSlotDouble(vm, 1);

	if (!ref->header.componentID.isValid())
	{
		ref->mass = value;
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	rb.set_mass(value);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::rigidbody_get_gravity_factor(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));

	if (!ref->header.componentID.isValid())
	{
		wrenSetSlotDouble(vm, 0, ref->gravityFactor);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	wrenSetSlotDouble(vm, 0, rb.get_gravity_factor());
}

void ScriptManager::rigidbody_set_gravity_factor(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));
	float value = wrenGetSlotDouble(vm, 1);

	if (!ref->header.componentID.isValid())
	{
		ref->gravityFactor = value;
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	rb.set_gravity_factor(value);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::rigidbody_get_is_trigger(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));

	if (!ref->header.componentID.isValid())
	{
		wrenSetSlotBool(vm, 0, ref->isSensor);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	wrenSetSlotBool(vm, 0, rb.get_is_sensor());
}

void ScriptManager::rigidbody_set_is_trigger(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));
	bool value = wrenGetSlotBool(vm, 1);

	if (!ref->header.componentID.isValid())
	{
		ref->isSensor = value;
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	rb.set_is_sensor(value);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::rigidbody_get_linear_velocity(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector3", 1);
	
	auto* value = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));
	new (value) Vector3Data();

	if (!ref->header.componentID.isValid())
	{
		value->x = 0.0f;
		value->y = 0.0f;
		value->z = 0.0f;
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		value->x = 0.0f;
		value->y = 0.0f;
		value->z = 0.0f;
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	glm::vec3 v = rb.get_linear_velocity();
	value->x = v.x;
	value->y = v.y;
	value->z = v.z;
}

void ScriptManager::rigidbody_set_linear_velocity(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));
	auto* value = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 1));

	if (!ref->header.componentID.isValid())
	{
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	rb.set_linear_velocity(*value);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::rigidbody_get_angular_velocity(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector3", 1);
	
	auto* value = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));
	new (value) Vector3Data();

	if (!ref->header.componentID.isValid())
	{
		value->x = 0.0f;
		value->y = 0.0f;
		value->z = 0.0f;
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		value->x = 0.0f;
		value->y = 0.0f;
		value->z = 0.0f;
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	glm::vec3 v = rb.get_angular_velocity();
	value->x = v.x;
	value->y = v.y;
	value->z = v.z;
}

void ScriptManager::rigidbody_set_angular_velocity(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));
	auto* value = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 1));

	if (!ref->header.componentID.isValid())
	{
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	rb.set_angular_velocity(*value);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::rigidbody_add_force(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));
	auto* value = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 1));

	if (!ref->header.componentID.isValid())
	{
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	rb.add_force(*value);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::rigidbody_add_impulse(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));
	auto* value = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 1));

	if (!ref->header.componentID.isValid())
	{
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	rb.add_impulse(*value);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::rigidbody_add_angular_force(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));
	auto* value = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 1));

	if (!ref->header.componentID.isValid())
	{
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	rb.add_angular_force(*value);
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::rigidbody_add_angular_impulse(WrenVM *vm)
{
	auto* ref = static_cast<RigidbodyData*>(wrenGetSlotForeign(vm, 0));
	auto* value = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 1));

	if (!ref->header.componentID.isValid())
	{
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& rb = dynamic_cast<RigidBody&>(ECS::instance->get_component_reference(ref->header.componentID));
	rb.add_angular_impulse(*value);
	wrenSetSlotNull(vm, 0);
}
#pragma endregion

#pragma region COLLIDER_BINDINGS
void ScriptManager::collider_finalize(void *data)
{

}

void ScriptManager::collider_set_position_offset(WrenVM *vm)
{
	auto* ref = static_cast<ColliderData*>(wrenGetSlotForeign(vm, 0));
	auto* value = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 1));

	if (!ref->header.componentID.isValid())
	{
		ref->positionOffset = *value;
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& collider = dynamic_cast<Collider&>(ECS::instance->get_component_reference(ref->header.componentID));
	collider.set_position_offset(*value);	
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::collider_get_position_offset(WrenVM *vm)
{
	auto* ref = static_cast<ColliderData*>(wrenGetSlotForeign(vm, 0));

	if (!ref->header.componentID.isValid())
	{
		wrenEnsureSlots(vm, 2);
		wrenGetVariable(vm, "m3d", "Vector3", 1);
		auto* value = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));
		new (value) Vector3Data();
		value->x = ref->positionOffset.x;
		value->y = ref->positionOffset.y;
		value->z = ref->positionOffset.z;
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector3", 1);
	auto* value = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));
	new (value) Vector3Data();
	
	auto& collider = dynamic_cast<Collider&>(ECS::instance->get_component_reference(ref->header.componentID));
	glm::vec3 v = collider.get_position_offset();
	value->x = v.x;
	value->y = v.y;
	value->z = v.z;
}

void ScriptManager::collider_set_euler_offset(WrenVM *vm)
{
	auto* ref = static_cast<ColliderData*>(wrenGetSlotForeign(vm, 0));
	auto* value = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 1));

	if (!ref->header.componentID.isValid())
	{
		ref->eulerDegreeOffset = *value;
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& collider = dynamic_cast<Collider&>(ECS::instance->get_component_reference(ref->header.componentID));
	collider.set_euler_degree_offset(*value);	
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::collider_get_euler_offset(WrenVM *vm)
{
	auto* ref = static_cast<ColliderData*>(wrenGetSlotForeign(vm, 0));

	if (!ref->header.componentID.isValid())
	{
		wrenEnsureSlots(vm, 2);
		wrenGetVariable(vm, "m3d", "Vector3", 1);
		auto* value = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));
		new (value) Vector3Data();
		value->x = ref->eulerDegreeOffset.x;
		value->y = ref->eulerDegreeOffset.y;
		value->z = ref->eulerDegreeOffset.z;
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector3", 1);
	auto* value = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));
	new (value) Vector3Data();

	auto& collider = dynamic_cast<Collider&>(ECS::instance->get_component_reference(ref->header.componentID));
	glm::vec3 v = collider.get_euler_degree_offset();
	value->x = v.x;
	value->y = v.y;
	value->z = v.z;
}

void ScriptManager::box_collider_allocate(WrenVM *vm)
{
	auto* data = static_cast<BoxColliderData*>(wrenSetSlotNewForeign(vm, 0, 0, sizeof(BoxColliderData)));

	new (data) BoxColliderData{};
	data->header.type = ComponentType::COLLIDER;
	data->header.componentID = {};
	data->colliderType = ColliderType::BOX;
}

void ScriptManager::box_collider_get_half_dimensions(WrenVM *vm)
{
	auto* ref = static_cast<BoxColliderData*>(wrenGetSlotForeign(vm, 0));

	if (ref->colliderType != ColliderType::BOX)
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	if (!ref->header.componentID.isValid())
	{
		wrenEnsureSlots(vm, 2);
		wrenGetVariable(vm, "m3d", "Vector3", 1);
		auto* value = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));
		new (value) Vector3Data();
		value->x = ref->halfDimensions.x;
		value->y = ref->halfDimensions.y;
		value->z = ref->halfDimensions.z;
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector3", 1);
	auto* value = static_cast<Vector3Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector3Data)));
	new (value) Vector3Data();

	auto& collider = dynamic_cast<BoxCollider&>(ECS::instance->get_component_reference(ref->header.componentID));
	glm::vec3 v = collider.get_half_dimensions();
	value->x = v.x;
	value->y = v.y;
	value->z = v.z;
}

void ScriptManager::box_collider_set_half_dimensions(WrenVM *vm)
{
	auto* ref = static_cast<BoxColliderData*>(wrenGetSlotForeign(vm, 0));
	auto* value = static_cast<Vector3Data*>(wrenGetSlotForeign(vm, 1));

	if (ref->colliderType != ColliderType::BOX)
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	if (!ref->header.componentID.isValid())
	{
		ref->halfDimensions = *value;
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& collider = dynamic_cast<BoxCollider&>(ECS::instance->get_component_reference(ref->header.componentID));
	collider.set_half_dimensions(*value);	
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::sphere_collider_allocate(WrenVM *vm)
{
	auto* data = static_cast<SphereColliderData*>(wrenSetSlotNewForeign(vm, 0, 0, sizeof(SphereColliderData)));

	new (data) SphereColliderData{};
	data->header.type = ComponentType::COLLIDER;
	data->header.componentID = {};
	data->colliderType = ColliderType::SPHERE;
}

void ScriptManager::sphere_collider_get_radius(WrenVM *vm)
{
	auto* ref = static_cast<SphereColliderData*>(wrenGetSlotForeign(vm, 0));

	if (ref->colliderType != ColliderType::SPHERE)
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	if (!ref->header.componentID.isValid())
	{
		wrenSetSlotDouble(vm, 0, ref->radius);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& collider = dynamic_cast<SphereCollider&>(ECS::instance->get_component_reference(ref->header.componentID));
	wrenSetSlotDouble(vm, 0, collider.get_radius());
}

void ScriptManager::sphere_collider_set_radius(WrenVM *vm)
{
	auto* ref = static_cast<SphereColliderData*>(wrenGetSlotForeign(vm, 0));
	float value = wrenGetSlotDouble(vm, 1);

	if (ref->colliderType != ColliderType::SPHERE)
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	if (!ref->header.componentID.isValid())
	{
		ref->radius = value;
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& collider = dynamic_cast<SphereCollider&>(ECS::instance->get_component_reference(ref->header.componentID));
	collider.set_radius(value);	
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::capsule_collider_allocate(WrenVM *vm)
{
	auto* data = static_cast<CapsuleColliderData*>(wrenSetSlotNewForeign(vm, 0, 0, sizeof(CapsuleColliderData)));

	new (data) CapsuleColliderData{};
	data->header.type = ComponentType::COLLIDER;
	data->header.componentID = {};
	data->colliderType = ColliderType::CAPSULE;
}

void ScriptManager::capsule_collider_get_half_height(WrenVM *vm)
{
	auto* ref = static_cast<CapsuleColliderData*>(wrenGetSlotForeign(vm, 0));

	if (ref->colliderType != ColliderType::CAPSULE)
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	if (!ref->header.componentID.isValid())
	{
		wrenSetSlotDouble(vm, 0, ref->halfHeight);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& collider = dynamic_cast<CapsuleCollider&>(ECS::instance->get_component_reference(ref->header.componentID));
	wrenSetSlotDouble(vm, 0, collider.get_half_height());
}

void ScriptManager::capsule_collider_set_half_height(WrenVM *vm)
{
	auto* ref = static_cast<CapsuleColliderData*>(wrenGetSlotForeign(vm, 0));
	float value = wrenGetSlotDouble(vm, 1);

	if (ref->colliderType != ColliderType::CAPSULE)
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	if (!ref->header.componentID.isValid())
	{
		ref->halfHeight = value;
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& collider = dynamic_cast<CapsuleCollider&>(ECS::instance->get_component_reference(ref->header.componentID));
	collider.set_half_height(value);	
	wrenSetSlotNull(vm, 0);
}

void ScriptManager::capsule_collider_get_radius(WrenVM *vm)
{
	auto* ref = static_cast<CapsuleColliderData*>(wrenGetSlotForeign(vm, 0));

	if (ref->colliderType != ColliderType::CAPSULE)
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	if (!ref->header.componentID.isValid())
	{
		wrenSetSlotDouble(vm, 0, ref->radius);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& collider = dynamic_cast<CapsuleCollider&>(ECS::instance->get_component_reference(ref->header.componentID));
	wrenSetSlotDouble(vm, 0, collider.get_radius());
}

void ScriptManager::capsule_collider_set_radius(WrenVM *vm)
{
	auto* ref = static_cast<CapsuleColliderData*>(wrenGetSlotForeign(vm, 0));
	float value = wrenGetSlotDouble(vm, 1);

	if (ref->colliderType != ColliderType::CAPSULE)
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	if (!ref->header.componentID.isValid())
	{
		ref->radius = value;
		wrenSetSlotNull(vm, 0);
		return;
	}
	
	if (!ECS::instance->is_component_valid(ref->header.componentID))
	{
		wrenSetSlotNull(vm, 0);
		return;
	}

	auto& collider = dynamic_cast<CapsuleCollider&>(ECS::instance->get_component_reference(ref->header.componentID));
	collider.set_radius(value);	
	wrenSetSlotNull(vm, 0);
}
#pragma endregion

#pragma region INPUT_BINDINGS
void ScriptManager::input_is_key_pressed(WrenVM *vm)
{
	const char* keyName = wrenGetSlotString(vm, 1);
	wrenSetSlotBool(vm, 0, Input::instance->is_key_pressed(SDL_GetKeyFromName(keyName)));
}

void ScriptManager::input_was_key_pressed_down(WrenVM *vm)
{
	const char* keyName = wrenGetSlotString(vm, 1);
	wrenSetSlotBool(vm, 0, Input::instance->was_key_pressed_down(SDL_GetKeyFromName(keyName)));
}

void ScriptManager::input_was_key_released(WrenVM *vm)
{
	const char* keyName = wrenGetSlotString(vm, 1);
	wrenSetSlotBool(vm, 0, Input::instance->was_key_released(SDL_GetKeyFromName(keyName)));
}

void ScriptManager::input_is_mouse_button_pressed(WrenVM *vm)
{
	const char* mouseButtonName = wrenGetSlotString(vm, 1);
	wrenSetSlotBool(vm, 0, Input::instance->is_mouse_button_pressed(Input::get_mouse_button_from_name(mouseButtonName)));
}

void ScriptManager::input_was_mouse_button_pressed_down(WrenVM *vm)
{
	const char* mouseButtonName = wrenGetSlotString(vm, 1);
	wrenSetSlotBool(vm, 0, Input::instance->was_mouse_button_pressed_down(Input::get_mouse_button_from_name(mouseButtonName)));
}

void ScriptManager::input_was_mouse_button_released(WrenVM *vm)
{
	const char* mouseButtonName = wrenGetSlotString(vm, 1);
	wrenSetSlotBool(vm, 0, Input::instance->was_mouse_button_released(Input::get_mouse_button_from_name(mouseButtonName)));
}

void ScriptManager::input_get_mouse_position(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector2", 1);

	auto* value = static_cast<Vector2Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector2Data)));
	new (value) Vector2Data(Input::instance->get_mouse_pos());
}

void ScriptManager::input_get_mouse_delta(WrenVM *vm)
{
	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "m3d", "Vector2", 1);

	auto* value = static_cast<Vector2Data*>(wrenSetSlotNewForeign(vm, 0, 1, sizeof(Vector2Data)));
	new (value) Vector2Data(Input::instance->get_mouse_delta());
}

void ScriptManager::input_get_mouse_scroll_delta(WrenVM *vm)
{
	wrenSetSlotDouble(vm, 0, Input::instance->get_scroll());
}
#pragma endregion
#pragma endregion
