#pragma once

#include "ecs_component.h"
#include "../audio/audio.h"

class AudioPlayer : public Component
{
private:
	SourceID sourceID = xg::Guid();

protected:
	void on_initialize() override;

public:
	AudioPlayer();
	~AudioPlayer() override;
	

	void start(EntityID entityID) override;
	void update(EntityID entityID, float deltaTime) override;
	void on_destroy(EntityID entityID) override;

	void set_sound(std::string wavPath);
	void set_gain(float gain);
	void set_full_volume_radius(float fullVolumeRadius);
	void set_attenuation(float attenuation);
	void set_playing(bool isPlaying);
	void set_looping(bool isLooping);
	void set_to_time(float timeStampSec);

	float get_time();
	float get_gain();
	bool get_looping();
	bool get_playing();
};
