#include "audio.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <crossguid/guid.hpp>

#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <iostream>

#include "../rendering/camera.h"
#include "../ecs/ecs.h"

#define M_PI 3.14159265358979323846

Audio* Audio::instance = nullptr;

Audio::Audio()
{
	Audio::instance = this;
	stream = nullptr;
	masterGain = 1.0f;
}

Audio::~Audio()
{
	Audio::instance = nullptr;
	SDL_AudioStream* destroyStream = nullptr;
	destroyStream = stream;
	stream = nullptr;
	SDL_DestroyAudioStream(destroyStream);
	sounds.clear();
	sources.clear();
	wavPaths.clear();
}

bool Audio::initialize(int outputSampleRate)
{
	if (stream)
		return true;
	
	if (outputSampleRate <= 0)
		return false;
	
	streamSpec = {};
	streamSpec.format = SDL_AUDIO_F32;
	streamSpec.channels = 2;
	streamSpec.freq = outputSampleRate;

	targetBytes = static_cast<int>(0.1f * streamSpec.freq * streamSpec.channels * static_cast<int>(sizeof(float)));

	stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &streamSpec, nullptr, nullptr);
	if (!stream)
		return false;
	
	if (!SDL_ResumeAudioStreamDevice(stream))
	{
		SDL_DestroyAudioStream(stream);
		stream = nullptr;
		return false;
	}
	
	return true;
}

SoundID Audio::load_wav(std::string wavPath)
{
	if (wavPaths.find(wavPath) != wavPaths.end())
		return wavPaths[wavPath];

	std::vector<float> monoSamples;

	if (!stream)
	{
		std::cout << "[Audio Error] No audio stream (Audio not initialized)\n";
		return xg::Guid();
	}

	SDL_AudioSpec srcSpec = {};
	uint8_t* srcData = nullptr;
	uint32_t srcLen = 0;
	if (!SDL_LoadWAV(wavPath.c_str(), &srcSpec, &srcData, &srcLen))
	{
		std::cout << "[SDL Error] " << SDL_GetError() << '\n';
		return xg::Guid();
	}
	
	SDL_AudioSpec dstSpec = {};
	dstSpec.format = SDL_AUDIO_F32;
    dstSpec.channels = 1;
    dstSpec.freq = streamSpec.freq;

	uint8_t* dstData = nullptr;
	int dstLen = 0;
	bool converted = SDL_ConvertAudioSamples(&srcSpec, srcData, static_cast<int>(srcLen), &dstSpec, &dstData, &dstLen);
	SDL_free(srcData);

	if (!converted)
	{
		std::cout << "[SDL Error] " << SDL_GetError() << '\n';
		return xg::Guid();
	}
	
	size_t sampleCount = static_cast<size_t>(dstLen) / sizeof(float);
	float* samples = reinterpret_cast<float*>(dstData);
	monoSamples.assign(samples, samples + sampleCount);
	SDL_free(dstData);

	SoundBuffer buff;
	buff.monoSamples = std::move(monoSamples);
	buff.durationSec = static_cast<float>(sampleCount) / static_cast<float>(streamSpec.freq);
	buff.references = 0;
	buff.wavPath = wavPath;
	SoundID soundID = xg::newGuid();
	sounds[soundID] = buff;
	wavPaths[wavPath] = soundID;
	return soundID;
}

void Audio::unload_sound(SoundID soundID)
{
	if (!check_valid_sound(soundID))
		return;
	
	sounds[soundID].monoSamples.clear();
	wavPaths.erase(sounds[soundID].wavPath);
	sounds.erase(soundID);
}

SourceID Audio::create_source(SoundID soundID, bool isLooping)
{
	if (!check_valid_sound(soundID))
	{
		std::cout << "[Audio Error] Invalid sound id\n";
		return xg::Guid();
	}
	
	AudioSource source;
	source.soundID = soundID;
	sounds[soundID].references++;
	source.isLooping = isLooping;
	source.isPlaying = false;
	source.playhead = 0;
	source.gain = 1.0f;
	source.fullVolumeRadius = 15.0f;
	source.attenuation = 0.02f;

	SourceID sourceID = xg::newGuid();
	sources[sourceID] = source;
	return sourceID;
}

SourceID Audio::create_source()
{
	AudioSource source;
	source.soundID = xg::Guid();
	source.isLooping = false;
	source.isPlaying = false;
	source.playhead = 0;
	source.gain = 1.0f;
	source.fullVolumeRadius = 15.0f;
	source.attenuation = 0.02f;

	SourceID sourceID = xg::newGuid();
	sources[sourceID] = source;
	return sourceID;
}

void Audio::destroy_source(SourceID sourceID)
{
	if (!check_valid_source(sourceID))
		return;

	SoundID soundID = sources[sourceID].soundID;
	if (soundID.isValid())
	{
		auto it = sounds.find(soundID);
		if (it != sounds.end())
		{
			it->second.references--;
			if (it->second.references == 0)
				dirtySounds.push(soundID);
		}
	}
	
	sources.erase(sourceID);
}

void Audio::set_source_entity_id(SourceID sourceID, EntityID entityID)
{
	if (sources.at(sourceID).entityID.isValid())
		return;
	
	sources.at(sourceID).entityID = entityID;
}

void Audio::top_off_buffer()
{
	int queued = SDL_GetAudioStreamQueued(stream);
	int additionalAmount = std::max(0, targetBytes - queued);
	additionalAmount -= additionalAmount % (2 * static_cast<int>(sizeof(float)));
	if (additionalAmount > 0)
		mix_into_stream(additionalAmount);
}

void Audio::destroy_dirty()
{
	while (!dirtySounds.empty())
	{
		auto it = sounds.find(dirtySounds.top());
		if (it != sounds.end() && it->second.references == 0)
			unload_sound(dirtySounds.top());
		dirtySounds.pop();
	}
}

void Audio::set_master_gain(float gain)
{
	masterGain = std::max(0.0f, gain);
}

void Audio::set_source_sound(SourceID sourceID, SoundID soundID)
{
	if (!check_valid_sound(soundID) || !check_valid_source(sourceID))
		return;

	AudioSource& source = sources[sourceID];
	if (source.soundID.isValid())
	{
		sounds[source.soundID].references--;
		if (sounds[sources[sourceID].soundID].references == 0)
			dirtySounds.push(sources[sourceID].soundID);
	}
	
	source.playhead = 0;
	source.isPlaying = false;
	source.soundID = soundID;

	sounds[soundID].references++;
}

void Audio::set_source_gain(SourceID sourceID, float gain)
{
	if (!check_valid_source(sourceID))
		return;
	sources[sourceID].gain = gain;
}

void Audio::set_source_full_volume_radius(SourceID sourceID, float fullVolumeRadius)
{
	sources[sourceID].fullVolumeRadius = fullVolumeRadius;
}

void Audio::set_source_attenuation(SourceID sourceID, float attenuation)
{
	sources[sourceID].attenuation = attenuation;
}

void Audio::set_source_playing(SourceID sourceID, bool isPlaying)
{
	if (!check_valid_source(sourceID))
		return;
	sources[sourceID].isPlaying = isPlaying;
}

void Audio::set_source_looping(SourceID sourceID, bool isLooping)
{
	if (!check_valid_source(sourceID))
		return;
	sources[sourceID].isLooping = isLooping;
}

void Audio::set_source_to_time(SourceID sourceID, float timeStampSec)
{
	if (!check_valid_source(sourceID))
		return;

	AudioSource& source = sources[sourceID];
	std::vector<float>& samples = sounds[source.soundID].monoSamples;

	if (samples.size() == 0)
	{
		source.playhead = 0;
		return;
	}

	size_t idx = static_cast<size_t>(std::max(0.0f, timeStampSec) * static_cast<float>(streamSpec.freq));
	source.playhead = std::min(idx, samples.size() - 1);
}

float Audio::get_source_time(SourceID sourceID)
{
	if (!check_valid_source(sourceID))
		return 0.0f;
	
	AudioSource &source = sources[sourceID];
	return static_cast<float>(source.playhead) / static_cast<float>(streamSpec.freq);
}

float Audio::get_source_gain(SourceID sourceID)
{
	if (!check_valid_source(sourceID))
		return 0.0f;
	
	return sources[sourceID].gain;
}

bool Audio::get_source_playing(SourceID sourceID)
{
	if (!check_valid_source(sourceID))
		return false;
	
	return sources[sourceID].isPlaying;
}

bool Audio::get_source_looping(SourceID sourceID)
{
	if (!check_valid_source(sourceID))
		return false;
	
	return sources[sourceID].isLooping;
}

void Audio::mix_into_stream(int numBytes)
{
	if (numBytes <= 0)
		return;

	const int numChannels = 2;
	const int frames = numBytes / static_cast<int>(sizeof(float) * numChannels);
	if (frames <= 0)
		return;
	
	std::vector<float> mix(static_cast<size_t>(frames) * numChannels, 0.0f);

	if (sounds.size() == 0 || sources.size() == 0)
	{
		SDL_PutAudioStreamData(stream, mix.data(), static_cast<int>(mix.size() * sizeof(float)));
		return;
	}

	bool isCamValid = true;
	if (Camera::instance == nullptr)
		isCamValid = false;

	if (isCamValid && !Camera::instance->get_entity_id().isValid())
		isCamValid = false;

	if (isCamValid && !ECS::instance->is_entity_valid(Camera::instance->get_entity_id()))
		isCamValid = false;

	if (!isCamValid)
	{
		SDL_PutAudioStreamData(stream, mix.data(), static_cast<int>(mix.size() * sizeof(float)));
		return;
	}

	glm::mat4 listenerMat = ECS::instance->get_world_matrix(Camera::instance->get_entity_id());
	glm::vec3 listenerPos = glm::vec3(listenerMat[3].x, listenerMat[3].y, listenerMat[3].z);
	glm::vec3 listenerForward = -glm::normalize(glm::vec3(listenerMat[2]));
	float listenerYawRadians = std::atan2(listenerForward.x, -listenerForward.z);

	for (auto& s : sources)
	{
		AudioSource& source = s.second;
		if (!source.isPlaying)
			continue;
		if (!check_valid_sound(source.soundID))
			continue;
		
		std::vector<float>& samples = sounds[source.soundID].monoSamples;
		if (samples.size() == 0)
			continue;

		if (!ECS::instance->is_entity_valid(source.entityID))
			continue;
		
		glm::mat4 sourceMat = ECS::instance->get_world_matrix(source.entityID);
		glm::vec3 sourcePos = glm::vec3(sourceMat[3].x, sourceMat[3].y, sourceMat[3].z);

		float dist = glm::distance(sourcePos, listenerPos);
		glm::vec2 localXZ = glm::rotate(glm::vec2(sourcePos.x - listenerPos.x, sourcePos.z - listenerPos.z), -listenerYawRadians);
		float horizDist = glm::length(localXZ);

		constexpr float kPanDepthBias = 0.2f;
		float panDenom = std::sqrt((horizDist * horizDist) + (kPanDepthBias * kPanDepthBias));
		float rawPan = glm::clamp(localXZ.x / std::max(panDenom, 0.001f), -1.0f, 1.0f);
		
		constexpr float kPanCurveExponent = 1.6f;
		constexpr float kPanWidth = 0.9f;
		float curvedPan = std::copysign(std::pow(std::fabs(rawPan), kPanCurveExponent), rawPan);
		float pan = glm::clamp(curvedPan * kPanWidth, -1.0f, 1.0f);

		float panAngle = (pan + 1.0f) * (M_PI * 0.25f);
		float leftPan = std::cos(panAngle);
		float rightPan = std::sin(panAngle);

		float effDist = std::max(0.0f, dist - source.fullVolumeRadius);
		float atten = 1.0f / (1.0f + (source.attenuation * effDist * effDist));
		float sourceGain = source.gain;
		float gain = sourceGain * masterGain;

		for (int i = 0; i < frames; ++i)
		{
			if (source.playhead >= samples.size())
			{
				if (source.isLooping)
					source.playhead = 0;
				else
				{
					source.isPlaying = false;
					break;
				}
			}
		
			float t = samples[source.playhead++] * atten * gain;
			mix[static_cast<size_t>(i) * 2] += t * leftPan;
			mix[static_cast<size_t>(i) * 2 + 1] += t * rightPan;
		}
	}

	for (float& sample : mix)
		sample = glm::clamp(sample, -1.0f, 1.0f);

	bool x = SDL_PutAudioStreamData(stream, mix.data(), static_cast<int>(mix.size() * sizeof(float)));
	if (!x)
		std::cout << "[SDL Error] " << SDL_GetError() << '\n';
}

bool Audio::check_valid_source(SourceID sourceID)
{
	return sources.find(sourceID) != sources.end();
}

bool Audio::check_valid_sound(SoundID soundID)
{
	return sounds.find(soundID) != sounds.end();
}
