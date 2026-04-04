#pragma once

#include <string>
#include <unordered_map>
#include <set>

#include <crossguid/guid.hpp>
#include <glm/glm.hpp>
#include <wren.hpp>

using ComponentID = xg::Guid;
using EntityID = xg::Guid;

class ScriptManager
{
private:
	struct ScriptBinding
	{
		ComponentID id = xg::Guid();
		std::string moduleName = "";
		WrenHandle* scriptHandle = nullptr;

		ScriptBinding(std::string _moduleName, WrenHandle* _scriptHandle) : moduleName(_moduleName), scriptHandle(_scriptHandle) {};
		ScriptBinding() = default;

		bool operator<(const ScriptBinding& other) const
		{
			return moduleName < other.moduleName;
		}

		bool operator==(const ScriptBinding& other) const
		{
			return moduleName == other.moduleName;
		}
	};

	struct GameObjectData
	{
		xg::Guid entityID;

		GameObjectData() = default;
		~GameObjectData() = default;
	};

	struct Vector2Data
	{
		float x;
		float y;

		Vector2Data() = default;

		Vector2Data(float _x, float _y) : x(_x), y(_y)
		{}

		Vector2Data(const glm::vec2& val) : x(val.x), y(val.y)
		{}

		operator glm::vec2() const
		{
			return glm::vec2(x, y);
		}
	};

	struct Vector3Data
	{
		float x;
		float y;
		float z;

		Vector3Data() = default;

		Vector3Data(float _x, float _y, float _z) : x(_x), y(_y), z(_z)
		{}

		Vector3Data(const glm::vec3& val) : x(val.x), y(val.y), z(val.z)
		{}

		operator glm::vec3() const
		{
			return glm::vec3(x, y, z);
		}
	};

	std::unordered_map<ComponentID, ScriptBinding> scripts;
	std::unordered_map<std::string, WrenHandle*> scriptClasses;

	WrenConfiguration config;
	WrenVM* vm;
	
	WrenHandle* ctorNew = nullptr;
	WrenHandle* onStart = nullptr;
	WrenHandle* onUpdate = nullptr;
	WrenHandle* gameObjectClass = nullptr;

	static WrenLoadModuleResult load_module(WrenVM*, const char* name);
	static WrenForeignClassMethods bind_foreign_class(WrenVM*, const char* module, const char* className);
	static WrenForeignMethodFn bind_foreign_method(WrenVM* vm, const char* module, const char* className, bool isStatic, const char* signature);
	static void write_output(WrenVM*, const char* text);
	static void report_error(WrenVM*, WrenErrorType type, const char* module, int line, const char* message);
	
	static void free_loaded_module(WrenVM*, const char*, WrenLoadModuleResult result);

	WrenHandle* get_script_class(std::string moduleName);
	bool attach_script(EntityID entityID, ComponentID componentID, std::string moduleName);

	static void game_object_allocate(WrenVM* vm);
	static void game_object_finalize(void* data);
	static void game_object_get_id(WrenVM* vm);
	static void game_object_get_parent(WrenVM* vm);
	static void game_object_get_name(WrenVM* vm);
	static void game_object_get_script(WrenVM* vm);
	static void gameobject_get_local_position(WrenVM* vm);
	static void game_object_set_local_position(WrenVM* vm);
	static void gameobject_get_local_euler_degrees(WrenVM* vm);
	static void game_object_set_local_euler_degrees(WrenVM* vm);
	static void gameobject_get_local_scale(WrenVM* vm);
	static void game_object_set_local_scale(WrenVM* vm);
	static void game_object_get_local_forward(WrenVM* vm);
	static void game_object_get_local_right(WrenVM* vm);
	static void game_object_get_local_up(WrenVM* vm);

	static void scene_find_object_by_name(WrenVM* vm);

	static void vector2_allocate(WrenVM* vm);
	static void vector2_finalize(void* data);
	static void vector2_get_x(WrenVM* vm);
	static void vector2_set_x(WrenVM* vm);
	static void vector2_get_y(WrenVM* vm);
	static void vector2_set_y(WrenVM* vm);
	static void vector2_add(WrenVM* vm);
	static void vector2_sub(WrenVM* vm);
	static void vector2_scalar_multiply(WrenVM* vm);

	static void vector3_allocate(WrenVM* vm);
	static void vector3_finalize(void* data);
	static void vector3_get_x(WrenVM* vm);
	static void vector3_set_x(WrenVM* vm);
	static void vector3_get_y(WrenVM* vm);
	static void vector3_set_y(WrenVM* vm);
	static void vector3_get_z(WrenVM* vm);
	static void vector3_set_z(WrenVM* vm);
	static void vector3_add(WrenVM* vm);
	static void vector3_sub(WrenVM* vm);
	static void vector3_scalar_multiply(WrenVM* vm);

	static void input_is_key_pressed(WrenVM* vm);
	static void input_was_key_pressed_down(WrenVM* vm);
	static void input_was_key_released(WrenVM* vm);
	static void input_is_mouse_button_pressed(WrenVM* vm);
	static void input_was_mouse_button_pressed_down(WrenVM* vm);
	static void input_was_mouse_button_released(WrenVM* vm);
	static void input_get_mouse_position(WrenVM* vm);
	static void input_get_mouse_delta(WrenVM* vm);
	static void input_get_mouse_scroll_delta(WrenVM* vm);

public:
	static ScriptManager* instance;

	bool initialize();

	ScriptManager();
	~ScriptManager();

	void register_script(EntityID entityID, ComponentID componentID, std::string scriptFilePath); //TODO
	void remove_script(ComponentID id); //TODO

	void start_script(ComponentID id);
	void update_script(ComponentID id, float deltaTime);
	void destroy_script(ComponentID id);

	void force_garbage_collect();
};