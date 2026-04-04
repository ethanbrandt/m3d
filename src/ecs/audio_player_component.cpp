#include "audio_player_component.h"
#include "ecs.h"

void AudioPlayer::on_initialize()
{
	Audio::instance->set_source_entity_id(sourceID, get_entity_id());
}

AudioPlayer::AudioPlayer()
{
	sourceID = Audio::instance->create_source();
}

AudioPlayer::~AudioPlayer()
{
	Audio::instance->destroy_source(sourceID);
}

void AudioPlayer::start(EntityID entityID)
{
}

void AudioPlayer::update(EntityID entityID, float deltaTime)
{
	
}

void AudioPlayer::on_destroy(EntityID entityID)
{
}

void AudioPlayer::set_sound(std::string wavPath)
{
	SoundID soundID = Audio::instance->load_wav(wavPath);
	Audio::instance->set_source_sound(sourceID, soundID);
}

void AudioPlayer::set_gain(float gain)
{
	Audio::instance->set_source_gain(sourceID, gain);
}

void AudioPlayer::set_full_volume_radius(float fullVolumeRadius)
{
	Audio::instance->set_source_full_volume_radius(sourceID, fullVolumeRadius);
}

void AudioPlayer::set_attenuation(float attenuation)
{
	Audio::instance->set_source_attenuation(sourceID, attenuation);
}

void AudioPlayer::set_playing(bool isPlaying)
{
	Audio::instance->set_source_playing(sourceID, isPlaying);
}

void AudioPlayer::set_looping(bool isLooping)
{
	Audio::instance->set_source_looping(sourceID, isLooping);
}

void AudioPlayer::set_to_time(float timeStampSec)
{
	Audio::instance->set_source_to_time(sourceID, timeStampSec);
}

float AudioPlayer::get_time()
{
	return Audio::instance->get_source_time(sourceID);
}

float AudioPlayer::get_gain()
{
	return Audio::instance->get_source_gain(sourceID);
}

bool AudioPlayer::get_looping()
{
	return Audio::instance->get_source_looping(sourceID);
}

bool AudioPlayer::get_playing()
{
	return Audio::instance->get_source_playing(sourceID);
}
