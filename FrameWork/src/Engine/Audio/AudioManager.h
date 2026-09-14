#pragma once
#include "AudioHandle.h"
#include <xaudio2.h>
#include <memory>
#include <string>
#include <vector>

class AudioManager
{
public:
	static constexpr uint32_t AUDIO_MAX = 100;
	static constexpr uint32_t DEFAULT_VOICE_COUNT = 20;

	static AudioManager& GetInstance();
	AudioManager(const AudioManager&) = delete;
	AudioManager& operator=(const AudioManager&) = delete;

	bool Initialize();
	void Update();
	void Terminate();

	AudioHandle Load(const std::string& fullPath, uint32_t voiceCount = DEFAULT_VOICE_COUNT);
	bool Play(AudioHandle handle, bool loop = false);
	bool Stop(AudioHandle handle);
	bool Unload(AudioHandle handle);
	bool IsLoaded(AudioHandle handle) const;

private:
	struct Resource
	{
		std::vector<IXAudio2SourceVoice*> sourceVoices;
		std::vector<BYTE> soundData;
		uint32_t playLength = 0;
		uint32_t nextVoice = 0;
	};

	enum class CommandType { Play, Stop, Unload };
	struct AudioCommand
	{
		CommandType type;
		AudioHandle handle = InvalidAudioHandle;
		bool loop = false;
	};

	static std::unique_ptr<AudioManager> m_instance;
	IXAudio2* m_pXaudio = nullptr;
	IXAudio2MasteringVoice* m_pMasteringVoice = nullptr;
	Resource m_audio[AUDIO_MAX];
	std::vector<AudioCommand> m_commandQueue;

	AudioManager() = default;
	bool PlayImmediate(AudioHandle handle, bool loop);
	bool StopImmediate(AudioHandle handle);
	bool UnloadImmediate(AudioHandle handle);
	void ProcessAudioCommands();
};
