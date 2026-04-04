#include <iostream>
#include <chrono>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

#include <wren.hpp>

#include "script_manager.h"
#include "ecs.h"
#include "input.h"
#include "ecs/script_component.h"

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
	delete[] result.userData;
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

void ScriptManager::remove_script(ComponentID id)
{
	//TODO all of remove_script
}

void ScriptManager::start_script(ComponentID id)
{
	wrenEnsureSlots(vm, 1);
	wrenSetSlotHandle(vm, 0, scripts.at(id).scriptHandle);
	if (wrenCall(vm, onStart))
		std::cout << "Error: failed to run on_start in " + scripts.at(id).moduleName + '\n';
}

void ScriptManager::update_script(ComponentID id, float deltaTime)
{
	wrenEnsureSlots(vm, 2);
	wrenSetSlotHandle(vm, 0, scripts.at(id).scriptHandle);
	wrenSetSlotDouble(vm, 1, deltaTime);
	if (wrenCall(vm, onUpdate))
		std::cout << "Error: failed to run on_update in " + scripts.at(id).moduleName + '\n';
}

void ScriptManager::destroy_script(ComponentID id)
{
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

void ScriptManager::game_object_get_script(WrenVM *vm)
{
	auto* ref = static_cast<GameObjectData*>(wrenGetSlotForeign(vm, 0));
	const char* moduleName = wrenGetSlotString(vm, 1);

	ScriptBinding* binding = nullptr;
	std::set<ComponentID> components = ECS::instance->get_all_components_of_type(ref->entityID, typeid(ScriptComponent));
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

	new (value) Vector3Data(ECS::instance->get_entity_local_rot(ref->entityID));
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
