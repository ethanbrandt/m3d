#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <crossguid/guid.hpp>

#include <string>
#include <vector>
#include <stack>
#include <unordered_map>

#include "../ecs/ecs.h"

using SoundID = xg::Guid;
using SourceID = xg::Guid;

class Audio
{
private:
	struct SoundBuffer
	{
		std::vector<float> monoSamples;
		float durationSec = 0.0f;
		int references = 0;
		std::string wavPath;
	};

	struct AudioSource
	{
		EntityID entityID;
		SoundID soundID;
		size_t playhead;
		bool isLooping;
		bool isPlaying;
		float gain;
		float fullVolumeRadius;
		float attenuation;
	};

	void mix_into_stream(int numBytes);
	bool check_valid_source(SourceID sourceID);
	bool check_valid_sound(SoundID soundID);
	void unload_sound(SoundID soundID);

	SDL_AudioStream* stream;
	SDL_AudioSpec streamSpec;
	std::unordered_map<SoundID, SoundBuffer> sounds;
	std::unordered_map<SourceID, AudioSource> sources;
	std::unordered_map<std::string, SoundID> wavPaths;
	std::stack<SoundID> dirtySounds;
	float masterGain;
	int targetBytes;

public:
	static Audio* instance;
	Audio();
	~Audio();

	bool initialize(int outputSampleRate = 48000);

	SoundID load_wav(std::string wavPath);

	SourceID create_source();
	SourceID create_source(SoundID soundID, bool isLooping);
	void destroy_source(SourceID sourceID);

	void set_source_entity_id(SourceID sourceID, EntityID entityID);

	void top_off_buffer();
	void destroy_dirty();
	void set_master_gain(float gain);

	void set_source_sound(SourceID sourceID, SoundID soundID);
	void set_source_gain(SourceID sourceID, float gain);
	void set_source_full_volume_radius(SourceID sourceID, float fullVolumeRadius);
	void set_source_attenuation(SourceID sourceID, float attenuation);
	void set_source_playing(SourceID sourceID, bool isPlaying);
	void set_source_looping(SourceID sourceID, bool isLooping);
	void set_source_to_time(SourceID sourceID, float timeStampSec);
	
	float get_source_time(SourceID sourceID);
	float get_source_gain(SourceID sourceID);
	bool get_source_playing(SourceID sourceID);
	bool get_source_looping(SourceID sourceID);
};