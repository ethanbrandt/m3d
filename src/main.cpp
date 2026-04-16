#include <iostream>
#include <chrono>
#include <memory>
#include <exception>
#include <glad/glad.h>
#include <SDL3/SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/MotionType.h>

#include "input.h"
#include "audio/audio.h"
#include "ecs/ecs.h"
#include "ecs/audio_player_component.h"
#include "script_manager.h"
#include "ecs/script_component.h"
#include "rendering/renderer.h"
#include "rendering/camera.h"
#include "ecs/mesh_renderer_component.h"
#include "physics/physics.h"
#include "ecs/box_collider_component.h"
#include "ecs/rigid_body_component.h"

void game_loop(SDL_Window* window)
{
	bool exitFlag = false;

	Input input;
	Audio audio;
	ScriptManager scriptManager;
	Renderer renderer("shaders/vertex_shader.vs", "shaders/fragment_shader.fs");
	Physics physics;
	ECS ecs;
	
	if (!scriptManager.initialize())
	{
		std::cout << "[Engine Error] ScriptManager failed to initialize\n";
		exitFlag = true;
	}

	if (!audio.initialize())
	{
		std::cout << "[Engine Error] Audio failed to initialize\n";
		exitFlag = true;
	}

	audio.set_master_gain(1.0f);

	EntityID audioObject = ecs.create_entity_record("Audio Test Object");

	std::unique_ptr<AudioPlayer> audioPlayer = std::make_unique<AudioPlayer>();
	audioPlayer->set_sound("assets/song.wav");
	audioPlayer->set_looping(true);
	audioPlayer->set_playing(true);
	audioPlayer->set_full_volume_radius(15.0f);
	audioPlayer->set_attenuation(0.01f);

	std::unique_ptr<ScriptComponent> script = std::make_unique<ScriptComponent>();
	script->set_module_name("test");

	std::unique_ptr<MeshRenderer> meshRenderer = std::make_unique<MeshRenderer>();
	meshRenderer->load_model("assets/scene.gltf");

	std::unique_ptr<RigidBody> rigidBody = std::make_unique<RigidBody>();
	BodyDesc terminalBodyDesc;
	rigidBody->set_body_desc(terminalBodyDesc);

	std::unique_ptr<BoxCollider> boxCol = std::make_unique<BoxCollider>();
	boxCol->set_half_dimensions(glm::vec3(0.5f, 0.5f, 0.5f));

	ecs.attach_component(audioObject, std::move(meshRenderer));
	ecs.attach_component(audioObject, std::move(audioPlayer));
	ecs.attach_component(audioObject, std::move(script));
	ecs.attach_component(audioObject, std::move(rigidBody));
	ecs.attach_component(audioObject, std::move(boxCol));

	EntityID obj2 = ecs.create_entity_record("Object 2");
	std::unique_ptr<ScriptComponent> script2 = std::make_unique<ScriptComponent>();
	script2->set_module_name("test2");

	ecs.attach_component(obj2, std::move(script2));

	EntityID camObj = ecs.create_entity_record("Camera Object");
	std::unique_ptr<ScriptComponent> camScript = std::make_unique<ScriptComponent>();
	camScript->set_module_name("cam");

	ecs.attach_component(camObj, std::move(camScript));

	ecs.set_entity_local_scale(camObj, glm::vec3(0.5f, 1.0f, 0.3f));

	EntityID camParent = ecs.create_entity_record("Camera Parent");
	ecs.set_parent_entity(camObj, camParent, false);

	Camera cam(camObj);
	cam.set_is_orthographic(false);
	cam.set_near_clipping_distance(0.1f);
	cam.set_far_clipping_distance(1000.0f);
	cam.set_zoom(90.0f);

	EntityID floorObj = ecs.create_entity_record("Floor");

	glm::vec3 floorPos = glm::vec3(0.0f, -10.0f, 0.0f);
	ecs.set_entity_world_pos(floorObj, floorPos);

	std::unique_ptr<RigidBody> floorRB = std::make_unique<RigidBody>();
	BodyDesc floorDesc;
	floorDesc.position = floorPos;
	floorDesc.motionType = JPH::EMotionType::Static;
	floorDesc.physicsLayer = 0;
	floorRB->set_body_desc(floorDesc);

	std::unique_ptr<BoxCollider> floorCol = std::make_unique<BoxCollider>();
	floorCol->set_half_dimensions(glm::vec3(15.0f, 1.0f, 15.0f));

	ecs.attach_component(floorObj, std::move(floorCol));
	ecs.attach_component(floorObj, std::move(floorRB));

	ecs.start(audioObject);
	ecs.start(obj2);
	ecs.start(camObj);
	ecs.start(floorObj);

	auto lastFrameTime = std::chrono::steady_clock::now();

	double physicsAccumulator = 0.0;
	constexpr double FIXED_DELTA_TIME = 1.0 / 120.0;

	int windowWidth, windowHeight;
	SDL_GetWindowSize(window, &windowWidth, &windowHeight);
	while (!exitFlag)
	{
		auto currFrameTime = std::chrono::steady_clock::now();
		float deltaTime = std::chrono::duration<float>(currFrameTime - lastFrameTime).count();
		lastFrameTime = currFrameTime;
		physicsAccumulator += deltaTime;

		input.update_input();

		SDL_Event e;
		while (SDL_PollEvent(&e) != 0)
		{
			if (e.type == SDL_EVENT_QUIT)
				exitFlag = true;
			else if (e.type == SDL_EVENT_WINDOW_RESIZED)
			{
				SDL_GetWindowSize(window, &windowWidth, &windowHeight);
				glViewport(0, 0, windowWidth, windowHeight);
			}
			else
				input.handle_input_event(&e);
		}

		scriptManager.update_all_scripts(deltaTime);
		scriptManager.force_garbage_collect();
		ecs.sync_transforms();

		ecs.update(deltaTime);
		scriptManager.remove_dirty_scripts();

		physics.sync_physics_transforms();
		while (physicsAccumulator >= FIXED_DELTA_TIME)
		{
			physics.physics_update(static_cast<float>(FIXED_DELTA_TIME));
			physicsAccumulator -= FIXED_DELTA_TIME;
		}
		ecs.sync_transforms();

		audio.destroy_dirty();
		audio.top_off_buffer();

		renderer.render(window, windowWidth, windowHeight);
	}
}

int main()
{
	SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
	SDL_Window* window = SDL_CreateWindow("M3D", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
	SDL_SetWindowRelativeMouseMode(window, true);

	if (!window)
	{
		std::cout << "[Engine Error] Failed to create SDL window: " << SDL_GetError() << '\n';
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}

	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context)
	{
		std::cout << "[Engine Error] Failed to create GL context: " << SDL_GetError() << '\n';
		SDL_GL_DestroyContext(context);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}

	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
	{
		std::cout << "[Engine Error] Failed to initialize GLAD\n";
		SDL_GL_DestroyContext(context);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}

	glViewport(0, 0, 1280, 720);
	glEnable(GL_DEPTH_TEST);
	SDL_GL_SetSwapInterval(0);

	std::cout << "OpenGL Loaded\n";
	std::cout << "Vendor: " << glGetString(GL_VENDOR) << '\n';
	std::cout << "Renderer: " << glGetString(GL_RENDERER) << '\n';
	std::cout << "Version: " << glGetString(GL_VERSION) << '\n';

	JPH::RegisterDefaultAllocator();

	game_loop(window);

	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
