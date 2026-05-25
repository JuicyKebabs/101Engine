#pragma once
#include <xaudio2.h>
#include <vector>
#include <memory>
#include "AudioData.h"

// Audio structure
struct AUDIO
{
	std::vector<IXAudio2SourceVoice*> sourceVoices{};
	BYTE* soundData{};

	int	length{};
	int	playLength{};
	int	nextVoice{};
};

// AudioManager class
class AudioManager
{
public:
	static constexpr int AUDIO_MAX = 100; //ç≈ëÂìØéûçƒê∂êî
public:
	static AudioManager& GetInstance()
	{
		if(!m_instance){
			m_instance = std::unique_ptr<AudioManager>(new AudioManager());
		}
		return *m_instance;
	}

	AudioManager(const AudioManager&) = delete;
	AudioManager& operator=(const AudioManager&) = delete;

	void Initialize();
	void Update();
	void Terminate();

	void Load(AudioData::AUDIO_TYPE type)
	{
		PushAudioCommand({ AudioData::COMMAND_TYPE::COMMAND_LOAD, type });
	}
	void Play(AudioData::AUDIO_TYPE type, bool loop = false)
	{
		PushAudioCommand({ AudioData::COMMAND_TYPE::COMMAND_PLAY, type, loop });
	}
	void Stop(AudioData::AUDIO_TYPE type)
	{
		PushAudioCommand({ AudioData::COMMAND_TYPE::COMMAND_STOP, type });
	}
	void Unload(AudioData::AUDIO_TYPE type)
	{
		PushAudioCommand({ AudioData::COMMAND_TYPE::COMMAND_UNLOAD, type });
	}

private:
	static inline std::unique_ptr<AudioManager> m_instance;

	IXAudio2* m_pXaudio{};
	IXAudio2MasteringVoice* m_pMasteringVoice{};
	AUDIO m_pAudio[AUDIO_MAX]{};

	std::vector<AudioData::AudioInfo> m_audioInfoList;
	std::vector<AudioData::AudioCommand> m_audioCommandQueue;

private:
	AudioManager() = default;	// Constructor

	void InitAudio();	// Initialize audio
	void UninitAudio();	// Uninitialize audio

	int LoadAudio(const char* FileName, int voiceCount);	// Load audio file
	void UnloadAudio(int Index);							// Unload audio file
	void PlayAudio(int Index, bool Loop = false);			// Play audio
	void StopAudio(int Index);								// Stop audio

	void ProcessAudioCommands();
	void PushAudioCommand(const AudioData::AudioCommand& command)
	{
		m_audioCommandQueue.push_back(command);
	}

	void LoadAudioCommand(const AudioData::AudioCommand& command);
	void PlayAudioCommand(const AudioData::AudioCommand& command);
	void StopAudioCommand(const AudioData::AudioCommand& command);
	void UnloadAudioCommand(const AudioData::AudioCommand& command);

	int GetIdFromList(AudioData::AUDIO_TYPE type);
	AudioData::AudioInfo* GetAudioInfoFromList(int id);
};



