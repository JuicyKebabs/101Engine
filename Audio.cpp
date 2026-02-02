
#include <d3d11.h>
#include <DirectXMath.h>
#include <mmiscapi.h>
using namespace DirectX;
#include "keyboard.h"

#include "audio.h"

void AudioManager::InitAudio()
{
	// XAudio生成
	XAudio2Create(&m_pXaudio, 0);

	// マスタリングボイス生成
	m_pXaudio->CreateMasteringVoice(&m_pMasteringVoice);
}

void AudioManager::UninitAudio()
{
	m_pMasteringVoice->DestroyVoice();
	m_pXaudio->Release();
}

int AudioManager::LoadAudio(const char *FileName, int voiceCount)
{
	int index = -1;

	for (int i = 0; i < AUDIO_MAX; i++)
	{
		if (m_pAudio[i].soundData == nullptr)
		{
			index = i;
			break;
		}
	}

	if (index == -1) return -1;

	// Load WAV file
	WAVEFORMATEX wfx = { 0 };

	{
		HMMIO hmmio = NULL;
		MMIOINFO mmioinfo = { 0 };
		MMCKINFO riffchunkinfo = { 0 };
		MMCKINFO datachunkinfo = { 0 };
		MMCKINFO mmckinfo = { 0 };
		UINT32 buflen;
		LONG readlen;

		hmmio = mmioOpen((LPSTR)FileName, &mmioinfo, MMIO_READ);
		assert(hmmio);

		riffchunkinfo.fccType = mmioFOURCC('W', 'A', 'V', 'E');
		mmioDescend(hmmio, &riffchunkinfo, NULL, MMIO_FINDRIFF);

		mmckinfo.ckid = mmioFOURCC('f', 'm', 't', ' ');
		mmioDescend(hmmio, &mmckinfo, &riffchunkinfo, MMIO_FINDCHUNK);

		if (mmckinfo.cksize >= sizeof(WAVEFORMATEX))
		{
			mmioRead(hmmio, (HPSTR)&wfx, sizeof(wfx));
		}
		else
		{
			PCMWAVEFORMAT pcmwf = { 0 };
			mmioRead(hmmio, (HPSTR)&pcmwf, sizeof(pcmwf));
			memset(&wfx, 0x00, sizeof(wfx));
			memcpy(&wfx, &pcmwf, sizeof(pcmwf));
			wfx.cbSize = 0;
		}
		mmioAscend(hmmio, &mmckinfo, 0);

		datachunkinfo.ckid = mmioFOURCC('d', 'a', 't', 'a');
		mmioDescend(hmmio, &datachunkinfo, &riffchunkinfo, MMIO_FINDCHUNK);

		buflen = datachunkinfo.cksize;
		m_pAudio[index].soundData = new unsigned char[buflen];
		readlen = mmioRead(hmmio, (HPSTR)m_pAudio[index].soundData, buflen);

		m_pAudio[index].length = readlen;
		m_pAudio[index].playLength = readlen / wfx.nBlockAlign;

		mmioClose(hmmio, 0);
	}

	// Prepare source voices
	m_pAudio[index].sourceVoices.resize(voiceCount);
	m_pAudio[index].nextVoice = 0;

	// Generate source voices
	for (int i = 0; i < voiceCount; i++)
	{
		m_pXaudio->CreateSourceVoice(&m_pAudio[index].sourceVoices[i], &wfx);
		assert(m_pAudio[index].sourceVoices[i]);
	}

	return index;
}

void AudioManager::UnloadAudio(int Index)
{
	for (auto& voice : m_pAudio[Index].sourceVoices)
	{
		voice->Stop();
		voice->FlushSourceBuffers();
		voice->DestroyVoice();
		voice = nullptr;
	}
	m_pAudio[Index].sourceVoices.clear();

	delete[] m_pAudio[Index].soundData;
	m_pAudio[Index].soundData = nullptr;

	m_pAudio[Index].length = 0;
	m_pAudio[Index].playLength = 0;
	m_pAudio[Index].nextVoice = 0;
}

void AudioManager::PlayAudio(int Index, bool Loop)
{
	if (m_pAudio[Index].sourceVoices.empty()) return;

	IXAudio2SourceVoice* voice = nullptr;

	for(auto& v : m_pAudio[Index].sourceVoices)
	{
		XAUDIO2_VOICE_STATE state;
		v->GetState(&state);
		if (state.BuffersQueued == 0)
		{
			voice = v;
			break;
		}
	}

	if(!voice)
	{
		int& nextVoice = m_pAudio[Index].nextVoice;
		voice = m_pAudio[Index].sourceVoices[nextVoice];
		nextVoice = (nextVoice + 1) % static_cast<int>(m_pAudio[Index].sourceVoices.size());
	}

	voice->Stop();
	voice->FlushSourceBuffers();

	// Buffer settings
	XAUDIO2_BUFFER bufinfo;
	memset(&bufinfo, 0x00, sizeof(bufinfo));
	bufinfo.AudioBytes = m_pAudio[Index].length;
	bufinfo.pAudioData = m_pAudio[Index].soundData;
	bufinfo.PlayBegin = 0;
	bufinfo.PlayLength = m_pAudio[Index].playLength;

	// Loop settings
	if (Loop)
	{
		bufinfo.LoopBegin = 0;
		bufinfo.LoopLength = m_pAudio[Index].playLength;
		bufinfo.LoopCount = XAUDIO2_LOOP_INFINITE;
	}

	voice->SubmitSourceBuffer(&bufinfo, NULL);
	voice->Start();
}

void AudioManager::StopAudio(int Index)
{
	m_pAudio[Index].sourceVoices[0]->Stop();
	m_pAudio[Index].sourceVoices[0]->FlushSourceBuffers();
}

// AudioManager class methods
void AudioManager::Initialize()
{
	InitAudio();
}

void AudioManager::Update()
{
	ProcessAudioCommands();
}

void AudioManager::Terminate()
{
	for (size_t i = 0; i < m_audioInfoList.size(); i++)
	{
		if(m_audioInfoList[i].isLoaded)
		{
			UnloadAudio(m_audioInfoList[i].id);
		}
	}

	m_audioInfoList.clear();

	UninitAudio();
}

void AudioManager::ProcessAudioCommands()
{
	for (const auto& command : m_audioCommandQueue)
	{
		switch (command.command)
		{
		case AudioData::COMMAND_TYPE::COMMAND_LOAD:
			LoadAudioCommand(command);
			break;

		case AudioData::COMMAND_TYPE::COMMAND_PLAY:
			PlayAudioCommand(command);
			break;

		case AudioData::COMMAND_TYPE::COMMAND_STOP:
			StopAudioCommand(command);
			break;

		case AudioData::COMMAND_TYPE::COMMAND_UNLOAD:
			UnloadAudioCommand(command);;
			break;

		default:
			break;
		}
	}

	m_audioCommandQueue.clear();
}

void AudioManager::LoadAudioCommand(const AudioData::AudioCommand& command)
{
	std::string filePath{};

	switch (command.type)
	{
	case AudioData::AUDIO_TYPE::AUDIO_NONE:
		break;

	case AudioData::AUDIO_TYPE::AUDIO_BGM_TITLE:
		filePath = "asset/audio/BGM_title.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_BGM_GAME_NORMAL:
		filePath = "asset/audio/BGM_game_normal.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_BGM_GAME_BOSS:
		filePath = "asset/audio/BGM_game_boss.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_BGM_RESULT_GAME_OVER:
		filePath = "asset/audio/BGM_result_game_over.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_BGM_RESULT_CLEAR:
		filePath = "asset/audio/BGM_result_clear.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_SE_PRESS_BUTTON:
		filePath = "asset/audio/SE_button.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_SE_PLAYER_DAMAGE:
		filePath = "asset/audio/SE_player_damage.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_SE_PLAYER_SHOT:
		filePath = "asset/audio/SE_player_shot.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_SE_PLAYER_BOOST:
		filePath = "asset/audio/SE_player_boost.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_SE_ENEMY_DAMAGE:
		filePath = "asset/audio/SE_enemy_damage.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_SE_TOMOS_SHOT:
		filePath = "asset/audio/SE_tomos_shot.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_SE_BOSS_SHOT:
		filePath = "asset/audio/SE_boss_shot.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_SE_BOSS_GUARD:
		filePath = "asset/audio/SE_boss_guard.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_SE_EXPLOSION:
		filePath = "asset/audio/SE_explosion.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_SE_EXPLOSION_MIDDLE:
		filePath = "asset/audio/SE_explosion_middle.wav";
		break;

	case AudioData::AUDIO_TYPE::AUDIO_SE_EXPLOSION_LARGE:
		filePath = "asset/audio/SE_explosion_large.wav";
		break;

	default:
		break;
	}

	const int VOICE_COUNT = 20;
	int id = LoadAudio(filePath.c_str(), VOICE_COUNT);

	// Update audio info list
	for(auto& info : m_audioInfoList)
	{
		if(info.type == command.type)
		{
			info.id = id;
			info.filePath = filePath;
			info.isLoaded = true;
			return;
		}
	}

	// If not found, add new entry
	m_audioInfoList.push_back({ command.type, filePath, id, true });
}

void AudioManager::PlayAudioCommand(const AudioData::AudioCommand& command)
{
	int id = -1;

	id = GetIdFromList(command.type);

	if (id != -1)
	{
		PlayAudio(id, command.loop);
		GetAudioInfoFromList(id)->isPlaying = true;
	}
}

void AudioManager::StopAudioCommand(const AudioData::AudioCommand& command)
{
	int id = -1;

	id = GetIdFromList(command.type);

	if (id != -1 && GetAudioInfoFromList(id)->isPlaying && GetAudioInfoFromList(id)->isLoaded)
	{
		StopAudio(id);
		GetAudioInfoFromList(id)->isPlaying = false;
	}
}

void AudioManager::UnloadAudioCommand(const AudioData::AudioCommand& command)
{
	int id = -1;

	id = GetIdFromList(command.type);

	if (id != -1 && GetAudioInfoFromList(id)->isLoaded)
	{
		UnloadAudio(id);
		GetAudioInfoFromList(id)->isLoaded = false;
	}
}

int AudioManager::GetIdFromList(AudioData::AUDIO_TYPE type)
{
	int id = -1;

	for (auto& info : m_audioInfoList)
	{
		if (info.type == type) id = info.id;
	}

	return id;
}

AudioData::AudioInfo* AudioManager::GetAudioInfoFromList(int id)
{
	for (auto& info : m_audioInfoList)
	{
		if (info.id == id) return &info;
	}
	return nullptr;
}
