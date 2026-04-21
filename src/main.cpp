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
#include "scene/scene_loader.h"

void game_loop(SDL_Window *window)
{
	bool exitFlag = false;

	Input input;
	Audio audio;
	ScriptManager scriptManager;
	Renderer renderer("shaders/vertex_shader.vs", "shaders/fragment_shader.fs");
	Physics physics;
	SceneLoader sceneLoader;	
	ECS ecs;
	Camera cam;

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

	sceneLoader.load_and_initialize_scene("assets/scenes/test_scene.json");

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
			physics.dispatch_collision_events();
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
	SDL_Window *window = SDL_CreateWindow("M3D", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
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

	JPH::RegisterDefaultAllocator();

	game_loop(window);

	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
