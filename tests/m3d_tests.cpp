#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#define private public
#include "audio/audio.h"
#include "ecs/audio_player_component.h"
#include "ecs/mesh_renderer_component.h"
#include "ecs/script_component.h"
#include "rendering/renderer.h"
#include "script_manager.h"
#undef private

#include "ecs.h"
#include "input.h"

Renderer* Renderer::instance = nullptr;

Renderer::Renderer(const char* vertexShaderPath, const char* fragmentShaderPath)
{
	(void)vertexShaderPath;
	(void)fragmentShaderPath;
	Renderer::instance = this;
}

Renderer::~Renderer()
{
	if (Renderer::instance == this)
		Renderer::instance = nullptr;
}

ModelID Renderer::load_model(const std::string& modelPath)
{
	auto it = pathToModel.find(modelPath);
	if (it != pathToModel.end())
	{
		modelReferenceCounts[it->second]++;
		return it->second;
	}

	ModelID id = xg::newGuid();
	pathToModel[modelPath] = id;
	modelReferenceCounts[id] = 1;
	return id;
}

void Renderer::release_model(ModelID modelID)
{
	auto it = modelReferenceCounts.find(modelID);
	if (it == modelReferenceCounts.end())
		return;

	it->second--;
	if (it->second > 0)
		return;

	modelReferenceCounts.erase(it);

	for (auto pathIt = pathToModel.begin(); pathIt != pathToModel.end();)
	{
		if (pathIt->second == modelID)
			pathIt = pathToModel.erase(pathIt);
		else
			++pathIt;
	}
}

void Renderer::queue_render(EntityID entityID, ModelID modelID)
{
	renderQueue.push_back({ entityID, modelID });
}

void Renderer::render(SDL_Window* window, int windowWidth, int windowHeight)
{
	(void)window;
	(void)windowWidth;
	(void)windowHeight;
	renderQueue.clear();
}

namespace
{
	struct TrackingComponent final : Component
	{
		static inline int startedCount = 0;
		static inline int updatedCount = 0;
		static inline int destroyedCount = 0;
		static inline int destructedCount = 0;

		static void reset()
		{
			startedCount = 0;
			updatedCount = 0;
			destroyedCount = 0;
			destructedCount = 0;
		}

		~TrackingComponent() override
		{
			++destructedCount;
		}

		void on_initialize() override
		{
		}

		void start(EntityID) override
		{
			++startedCount;
		}

		void update(EntityID, float) override
		{
			++updatedCount;
		}

		void on_destroy(EntityID) override
		{
			++destroyedCount;
		}
	};

	class ScopedCoutCapture
	{
	public:
		ScopedCoutCapture() : previous(std::cout.rdbuf(stream.rdbuf()))
		{
		}

		~ScopedCoutCapture()
		{
			std::cout.rdbuf(previous);
		}

		std::string str() const
		{
			return stream.str();
		}

	private:
		std::ostringstream stream;
		std::streambuf* previous;
	};

	void assert_contains(const std::string& haystack, const std::string& needle)
	{
		assert(haystack.find(needle) != std::string::npos);
	}

	void assert_near(float actual, float expected, float epsilon = 0.0001f)
	{
		assert(std::fabs(actual - expected) <= epsilon);
	}

	void assert_vec3_eq(const glm::vec3& actual, const glm::vec3& expected, float epsilon = 0.0001f)
	{
		assert_near(actual.x, expected.x, epsilon);
		assert_near(actual.y, expected.y, epsilon);
		assert_near(actual.z, expected.z, epsilon);
	}

	void test_ecs_component_lifecycle()
	{
		TrackingComponent::reset();

		ECS ecs;
		EntityID entityID = ecs.create_entity_record("test entity");

		ComponentID componentID = ecs.attach_component(entityID, std::make_unique<TrackingComponent>());
		assert(componentID.isValid());
		assert(ecs.is_entity_valid(entityID));

		ecs.start(entityID);
		ecs.update(0.016f);

		assert(TrackingComponent::startedCount == 1);
		assert(TrackingComponent::updatedCount == 1);
		assert(TrackingComponent::destroyedCount == 0);
		assert(TrackingComponent::destructedCount == 0);

		ecs.remove_component(entityID, componentID);
		assert(TrackingComponent::destroyedCount == 1);
		assert(TrackingComponent::destructedCount == 1);

		ComponentID invalidAttach = ecs.attach_component(xg::Guid(), std::make_unique<TrackingComponent>());
		assert(!invalidAttach.isValid());
		assert(TrackingComponent::destructedCount == 2);

		ComponentID survivorID = ecs.attach_component(entityID, std::make_unique<TrackingComponent>());
		assert(survivorID.isValid());

		ecs.destroy_entity_record(entityID);
		assert(!ecs.is_entity_valid(entityID));
		assert(TrackingComponent::destroyedCount == 2);
		assert(TrackingComponent::destructedCount == 3);
	}

	void test_audio_player_component_forwards_state_and_cleans_up()
	{
		ECS ecs;
		Audio audio;

		EntityID entityID = ecs.create_entity_record("speaker");

		auto player = std::make_unique<AudioPlayer>();
		AudioPlayer* playerPtr = player.get();
		ComponentID componentID = ecs.attach_component(entityID, std::move(player));

		assert(componentID.isValid());
		assert(audio.sources.size() == 1);

		const SourceID sourceID = audio.sources.begin()->first;
		const Audio::AudioSource& source = audio.sources.begin()->second;
		assert(source.entityID == entityID);
		assert_near(source.gain, 1.0f);
		assert(!source.isLooping);
		assert(!source.isPlaying);

		playerPtr->set_gain(0.25f);
		playerPtr->set_looping(true);
		playerPtr->set_playing(true);

		assert_near(playerPtr->get_gain(), 0.25f);
		assert(playerPtr->get_looping());
		assert(playerPtr->get_playing());
		assert_near(audio.sources.at(sourceID).gain, 0.25f);
		assert(audio.sources.at(sourceID).isLooping);
		assert(audio.sources.at(sourceID).isPlaying);

		ecs.remove_component(entityID, componentID);
		assert(audio.sources.empty());

		ComponentID invalidAttach = ecs.attach_component(xg::Guid(), std::make_unique<AudioPlayer>());
		assert(!invalidAttach.isValid());
		assert(audio.sources.empty());
	}

	void test_mesh_renderer_shares_model_ids_and_releases_them()
	{
		ECS ecs;
		Renderer renderer("unused.vert", "unused.frag");

		EntityID entityA = ecs.create_entity_record("mesh a");
		auto meshA = std::make_unique<MeshRenderer>();
		MeshRenderer* meshAPtr = meshA.get();
		ComponentID componentA = ecs.attach_component(entityA, std::move(meshA));

		assert(componentA.isValid());
		ecs.update(0.016f);
		assert(renderer.renderQueue.empty());

		meshAPtr->load_model("assets/backpack.obj");
		assert(renderer.pathToModel.size() == 1);

		const ModelID sharedModelID = renderer.pathToModel.at("assets/backpack.obj");
		assert(sharedModelID.isValid());
		assert(renderer.modelReferenceCounts.at(sharedModelID) == 1);

		ecs.update(0.016f);
		assert(renderer.renderQueue.size() == 1);
		assert(renderer.renderQueue.front().entityID == entityA);
		assert(renderer.renderQueue.front().modelID == sharedModelID);
		renderer.renderQueue.clear();

		EntityID entityB = ecs.create_entity_record("mesh b");
		auto meshB = std::make_unique<MeshRenderer>();
		MeshRenderer* meshBPtr = meshB.get();
		ComponentID componentB = ecs.attach_component(entityB, std::move(meshB));

		assert(componentB.isValid());
		meshBPtr->load_model("assets/backpack.obj");
		assert(renderer.modelReferenceCounts.at(sharedModelID) == 2);
		assert(renderer.pathToModel.at("assets/backpack.obj") == sharedModelID);

		ecs.update(0.016f);
		assert(renderer.renderQueue.size() == 2);

		std::vector<EntityID> queuedEntities;
		for (const Renderer::RenderInfo& info : renderer.renderQueue)
		{
			assert(info.modelID == sharedModelID);
			queuedEntities.push_back(info.entityID);
		}

		assert(std::find(queuedEntities.begin(), queuedEntities.end(), entityA) != queuedEntities.end());
		assert(std::find(queuedEntities.begin(), queuedEntities.end(), entityB) != queuedEntities.end());
		renderer.renderQueue.clear();

		ecs.remove_component(entityA, componentA);
		assert(renderer.modelReferenceCounts.at(sharedModelID) == 1);
		assert(renderer.pathToModel.at("assets/backpack.obj") == sharedModelID);

		ecs.destroy_entity_record(entityB);
		assert(renderer.modelReferenceCounts.empty());
		assert(renderer.pathToModel.empty());
	}

	void test_script_component_registers_and_runs_wren_callbacks()
	{
		ECS ecs;
		Input input;
		ScriptManager scripts;

		assert(scripts.initialize());

		EntityID entityA = ecs.create_entity_record("Object 1");
		auto scriptA = std::make_unique<ScriptComponent>();
		ScriptComponent* scriptAPtr = scriptA.get();
		scriptAPtr->set_module_name("test");
		ComponentID componentA = ecs.attach_component(entityA, std::move(scriptA));
		assert(componentA.isValid());

		EntityID entityB = ecs.create_entity_record("Object 2");
		auto scriptB = std::make_unique<ScriptComponent>();
		ScriptComponent* scriptBPtr = scriptB.get();
		scriptBPtr->set_module_name("test2");
		ComponentID componentB = ecs.attach_component(entityB, std::move(scriptB));
		assert(componentB.isValid());

		assert(scripts.scripts.size() == 2);
		assert(scripts.scripts.at(componentA).moduleName == "test");
		assert(scripts.scripts.at(componentA).scriptHandle != nullptr);
		assert(scripts.scripts.at(componentB).moduleName == "test2");
		assert(scripts.scripts.at(componentB).scriptHandle != nullptr);

		ScopedCoutCapture capture;

		ecs.start(entityA);
		ecs.start(entityB);
		assert_vec3_eq(ecs.get_entity_local_pos(entityA), glm::vec3(2.0f, 3.0f, 4.0f));

		SDL_Event motionEvent = {};
		motionEvent.type = SDL_EVENT_MOUSE_MOTION;
		motionEvent.motion.x = 12.0f;
		motionEvent.motion.y = 34.0f;
		motionEvent.motion.xrel = 1.0f;
		motionEvent.motion.yrel = -2.0f;
		input.handle_input_event(&motionEvent);

		SDL_Event mouseDownEvent = {};
		mouseDownEvent.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
		mouseDownEvent.button.button = SDL_BUTTON_LEFT;
		input.handle_input_event(&mouseDownEvent);

		ecs.update(0.016f);

		const std::string output = capture.str();
		assert_contains(output, "Object 1 has started");
		assert_contains(output, "Object 2 has started");
		assert_contains(output, "mouse pos:");
		assert_contains(output, "Object 2 exploded lol");
	}
}

int main()
{
	test_ecs_component_lifecycle();
	test_audio_player_component_forwards_state_and_cleans_up();
	test_mesh_renderer_shares_model_ids_and_releases_them();
	test_script_component_registers_and_runs_wren_callbacks();
	return 0;
}
