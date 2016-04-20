#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <map>
#include <string>
#include <vector>

#include "MusicFormat.h"
#include "SoundFormat.h"

// Music is for looping background music (MIDI, MP3, or Ogg). Sound is for short 
// to medium duration sounds and speech (Ogg or WAV).

// The default format for both music and sound will be Ogg.

// It appears that volumes can only be set after a sound starts playing, but
// the sound can be muted first and then unmuted once the properties are set.

// I think the audio manager shouldn't use random numbers itself to decide
// which music name to pick from a music type. Let the program do that somewhere
// else.

enum class MusicName;
enum class SoundName;

struct FMOD_SYSTEM;
struct FMOD_SOUND;
struct FMOD_CHANNEL;

class AudioManager
{
private:
	static const std::string MUSIC_PATH;
	static const std::string SOUNDS_PATH;

	FMOD_SYSTEM *system;
	FMOD_CHANNEL *musicChannel, *soundChannel;
	std::map<std::string, FMOD_SOUND*> objects;
	MusicFormat musicFormat;
	SoundFormat soundFormat;

	bool isLoaded(FMOD_SOUND *object) const;

	void loadMusic(const std::string &filename);
	void loadSound(const std::string &filename);
public:
	AudioManager(MusicFormat musicFormat, SoundFormat soundFormat, int maxChannels);
	~AudioManager();

	static const double MIN_VOLUME;
	static const double MAX_VOLUME;

	double getMusicVolume() const;
	double getSoundVolume() const;
	bool musicIsPlaying() const;

	// All music will continue to loop until changed by an outside force.
	void playMusic(const std::string &filename);
	void playMusic(MusicName musicName);
	void playSound(const std::string &filename);
	void playSound(SoundName soundName);

	void toggleMusic();
	void stopMusic();
	void stopSound();

	// Percent is [0.0, 1.0].
	void setMusicVolume(double percent);
	void setSoundVolume(double percent);
};

#endif
