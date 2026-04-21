#pragma once

#include <string>
#include <unordered_map>
#include <set>

#include <crossguid/guid.hpp>
#include <glm/glm.hpp>
#include <wren.hpp>

#include "ecs/ecs_component.h"
#include "physics/physics.h"

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

	struct ComponentHeader
	{
		ComponentType type;
		ComponentID componentID;
	};

	struct RigidbodyData
	{
		ComponentHeader header;

		int motionType = 2;
		float friction = 0.8f;
		float restitution = 0.0f;
		float mass = 1.0f;
		float gravityFactor = 1.0f;
		bool isSensor = false;
	};

	struct ColliderData
	{
		ComponentHeader header;
		ColliderType colliderType;
		glm::vec3 positionOffset = glm::vec3(0.0f);
		glm::vec3 eulerDegreeOffset = glm::vec3(0.0f);
	};

	struct BoxColliderData : ColliderData
	{
		glm::vec3 halfDimensions = glm::vec3(0.5f, 0.5f, 0.5f);
	};

	struct SphereColliderData : ColliderData
	{
		float radius = 0.5f;
	};

	struct CapsuleColliderData : ColliderData
	{
		float radius = 0.5f;
		float halfHeight = 0.5f;
	};

	std::unordered_map<ComponentID, ScriptBinding> scripts;
	std::unordered_map<std::string, WrenHandle*> scriptClasses;
	std::set<ComponentID> dirtyScripts;

	WrenConfiguration config;
	WrenVM* vm;
	
	WrenHandle* ctorNew = nullptr;
	WrenHandle* onStart = nullptr;
	WrenHandle* onUpdate = nullptr;
	WrenHandle* onDestroy = nullptr;
	WrenHandle* onCollisionEnter = nullptr;
	WrenHandle* onCollisionStay = nullptr;
	WrenHandle* onCollisionExit = nullptr;
	WrenHandle* onTriggerEnter = nullptr;
	WrenHandle* onTriggerStay = nullptr;
	WrenHandle* onTriggerExit = nullptr;
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
	static void gameobject_get_local_position(WrenVM* vm);
	static void game_object_set_local_position(WrenVM* vm);
	static void gameobject_get_local_euler_degrees(WrenVM* vm);
	static void game_object_set_local_euler_degrees(WrenVM* vm);
	static void gameobject_get_local_scale(WrenVM* vm);
	static void game_object_set_local_scale(WrenVM* vm);
	static void game_object_get_local_forward(WrenVM* vm);
	static void game_object_get_local_right(WrenVM* vm);
	static void game_object_get_local_up(WrenVM* vm);
	static void game_object_get_script(WrenVM* vm);
	static void game_object_get_components_by_type(WrenVM* vm);
	static void game_object_attach_component(WrenVM* vm);
	static void game_object_remove_component(WrenVM* vm);

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

	static void rigidbody_allocate(WrenVM* vm);
	static void rigidbody_finalize(void* data);
	static void rigidbody_get_motion_type(WrenVM* vm); // TODO
	static void rigidbody_set_motion_type(WrenVM* vm); // TODO
	static void rigidbody_get_friction(WrenVM* vm);
	static void rigidbody_set_friction(WrenVM* vm);
	static void rigidbody_get_restitution(WrenVM* vm);
	static void rigidbody_set_restitution(WrenVM* vm);
	static void rigidbody_get_mass(WrenVM* vm);
	static void rigidbody_set_mass(WrenVM* vm);
	static void rigidbody_get_gravity_factor(WrenVM* vm);
	static void rigidbody_set_gravity_factor(WrenVM* vm);
	static void rigidbody_get_is_trigger(WrenVM* vm);
	static void rigidbody_set_is_trigger(WrenVM* vm);
	static void rigidbody_get_linear_velocity(WrenVM* vm);
	static void rigidbody_set_linear_velocity(WrenVM* vm);
	static void rigidbody_get_angular_velocity(WrenVM* vm);
	static void rigidbody_set_angular_velocity(WrenVM* vm);
	static void rigidbody_add_force(WrenVM* vm);
	static void rigidbody_add_impulse(WrenVM* vm);
	static void rigidbody_add_angular_force(WrenVM* vm);
	static void rigidbody_add_angular_impulse(WrenVM* vm);

	static void collider_finalize(void* data);
	static void collider_set_position_offset(WrenVM* vm);
	static void collider_get_position_offset(WrenVM* vm);
	static void collider_set_euler_offset(WrenVM* vm);
	static void collider_get_euler_offset(WrenVM* vm);

	static void box_collider_allocate(WrenVM* vm);
	static void box_collider_get_half_dimensions(WrenVM* vm);
	static void box_collider_set_half_dimensions(WrenVM* vm);

	static void sphere_collider_allocate(WrenVM* vm);
	static void sphere_collider_get_radius(WrenVM* vm);
	static void sphere_collider_set_radius(WrenVM* vm);

	static void capsule_collider_allocate(WrenVM* vm);
	static void capsule_collider_get_half_height(WrenVM* vm);
	static void capsule_collider_set_half_height(WrenVM* vm);
	static void capsule_collider_get_radius(WrenVM* vm);
	static void capsule_collider_set_radius(WrenVM* vm);

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

	void register_script(EntityID entityID, ComponentID componentID, std::string moduleName);
	void remove_dirty_scripts();

	void start_script(ComponentID id);
	void update_script(ComponentID id, float deltaTime);
	void update_all_scripts(float deltaTime);
	void destroy_script(ComponentID id);

	void collision_enter_script_event(EntityID entityID, EntityID otherID);
	void collision_stay_script_event(EntityID entityID, EntityID otherID);
	void collision_exit_script_event(EntityID entityID, EntityID otherID);

	void trigger_enter_script_event(EntityID entityID, EntityID otherID);
	void trigger_stay_script_event(EntityID entityID, EntityID otherID);
	void trigger_exit_script_event(EntityID entityID, EntityID otherID);

	void force_garbage_collect();
};