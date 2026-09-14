#include "AudioManager.h"
#include "Engine/Core/Debug/Debug.h"
#include <climits>
#include <cstring>
#include <mmiscapi.h>

std::unique_ptr<AudioManager> AudioManager::m_instance;

AudioManager& AudioManager::GetInstance()
{
	if (!m_instance) m_instance.reset(new AudioManager());
	return *m_instance;
}

bool AudioManager::Initialize()
{
	if (m_pXaudio) return true;
	HRESULT result = XAudio2Create(&m_pXaudio, 0);
	if (FAILED(result))
	{
		DBG("AudioManager: XAudio2Create failed (0x%08X).", static_cast<unsigned int>(result));
		m_pXaudio = nullptr;
		return false;
	}
	result = m_pXaudio->CreateMasteringVoice(&m_pMasteringVoice);
	if (FAILED(result))
	{
		DBG("AudioManager: CreateMasteringVoice failed (0x%08X).", static_cast<unsigned int>(result));
		m_pXaudio->Release();
		m_pXaudio = nullptr;
		return false;
	}
	return true;
}

void AudioManager::Update()
{
	ProcessAudioCommands();
}

void AudioManager::Terminate()
{
	m_commandQueue.clear();
	for (AudioHandle handle = 0; handle < AUDIO_MAX; ++handle) UnloadImmediate(handle);
	if (m_pMasteringVoice)
	{
		m_pMasteringVoice->DestroyVoice();
		m_pMasteringVoice = nullptr;
	}
	if (m_pXaudio)
	{
		m_pXaudio->Release();
		m_pXaudio = nullptr;
	}
}

AudioHandle AudioManager::Load(const std::string& fullPath, uint32_t voiceCount)
{
	if (!m_pXaudio || fullPath.empty() || voiceCount == 0) return InvalidAudioHandle;

	AudioHandle handle = InvalidAudioHandle;
	for (AudioHandle candidate = 0; candidate < AUDIO_MAX; ++candidate)
	{
		if (!IsLoaded(candidate)) { handle = candidate; break; }
	}
	if (handle == InvalidAudioHandle) return InvalidAudioHandle;

	std::vector<char> mutablePath(fullPath.begin(), fullPath.end());
	mutablePath.push_back('\0');
	HMMIO file = mmioOpenA(mutablePath.data(), nullptr, MMIO_READ);
	if (!file)
	{
		DBG("AudioManager: Could not open WAV file: %s", fullPath.c_str());
		return InvalidAudioHandle;
	}

	WAVEFORMATEX format{};
	MMCKINFO riff{};
	riff.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	MMCKINFO formatChunk{};
	formatChunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
	MMCKINFO dataChunk{};
	dataChunk.ckid = mmioFOURCC('d', 'a', 't', 'a');

	bool valid = mmioDescend(file, &riff, nullptr, MMIO_FINDRIFF) == MMSYSERR_NOERROR &&
		mmioDescend(file, &formatChunk, &riff, MMIO_FINDCHUNK) == MMSYSERR_NOERROR;
	if (valid)
	{
		if (formatChunk.cksize == sizeof(WAVEFORMATEX))
			valid = mmioRead(file, reinterpret_cast<HPSTR>(&format), sizeof(format)) == sizeof(format);
		else if (formatChunk.cksize >= sizeof(PCMWAVEFORMAT))
		{
			PCMWAVEFORMAT pcm{};
			valid = mmioRead(file, reinterpret_cast<HPSTR>(&pcm), sizeof(pcm)) == sizeof(pcm);
			if (valid) { std::memcpy(&format, &pcm, sizeof(pcm)); format.cbSize = 0; }
		}
		else valid = false;
	}
	if (valid) valid = mmioAscend(file, &formatChunk, 0) == MMSYSERR_NOERROR &&
		mmioDescend(file, &dataChunk, &riff, MMIO_FINDCHUNK) == MMSYSERR_NOERROR &&
		dataChunk.cksize > 0 && format.nBlockAlign > 0;

	std::vector<BYTE> data;
	if (valid)
	{
		data.resize(dataChunk.cksize);
		valid = data.size() <= LONG_MAX && mmioRead(file, reinterpret_cast<HPSTR>(data.data()),
			static_cast<LONG>(data.size())) == static_cast<LONG>(data.size());
	}
	mmioClose(file, 0);
	if (!valid)
	{
		DBG("AudioManager: Invalid or unsupported WAV file: %s", fullPath.c_str());
		return InvalidAudioHandle;
	}

	Resource resource;
	resource.soundData = std::move(data);
	resource.playLength = static_cast<uint32_t>(resource.soundData.size() / format.nBlockAlign);
	resource.sourceVoices.reserve(voiceCount);
	for (uint32_t voiceIndex = 0; voiceIndex < voiceCount; ++voiceIndex)
	{
		IXAudio2SourceVoice* voice = nullptr;
		const HRESULT result = m_pXaudio->CreateSourceVoice(&voice, &format);
		if (FAILED(result) || !voice)
		{
			for (IXAudio2SourceVoice* created : resource.sourceVoices) created->DestroyVoice();
			DBG("AudioManager: CreateSourceVoice failed for %s (0x%08X).",
				fullPath.c_str(), static_cast<unsigned int>(result));
			return InvalidAudioHandle;
		}
		resource.sourceVoices.push_back(voice);
	}
	m_audio[handle] = std::move(resource);
	return handle;
}

bool AudioManager::Play(AudioHandle handle, bool loop)
{
	if (!IsLoaded(handle)) return false;
	m_commandQueue.push_back({ CommandType::Play, handle, loop });
	return true;
}

bool AudioManager::Stop(AudioHandle handle)
{
	if (!IsLoaded(handle)) return false;
	m_commandQueue.push_back({ CommandType::Stop, handle });
	return true;
}

bool AudioManager::Unload(AudioHandle handle)
{
	if (!IsLoaded(handle)) return false;
	m_commandQueue.push_back({ CommandType::Unload, handle });
	return true;
}

bool AudioManager::IsLoaded(AudioHandle handle) const
{
	return handle < AUDIO_MAX && !m_audio[handle].soundData.empty();
}

bool AudioManager::PlayImmediate(AudioHandle handle, bool loop)
{
	if (!IsLoaded(handle)) return false;
	Resource& audio = m_audio[handle];
	IXAudio2SourceVoice* voice = nullptr;
	for (IXAudio2SourceVoice* candidate : audio.sourceVoices)
	{
		XAUDIO2_VOICE_STATE state{};
		candidate->GetState(&state);
		if (state.BuffersQueued == 0) { voice = candidate; break; }
	}
	if (!voice)
	{
		voice = audio.sourceVoices[audio.nextVoice];
		audio.nextVoice = (audio.nextVoice + 1) % static_cast<uint32_t>(audio.sourceVoices.size());
	}
	voice->Stop();
	voice->FlushSourceBuffers();
	XAUDIO2_BUFFER buffer{};
	buffer.AudioBytes = static_cast<UINT32>(audio.soundData.size());
	buffer.pAudioData = audio.soundData.data();
	buffer.PlayLength = audio.playLength;
	if (loop)
	{
		buffer.LoopLength = audio.playLength;
		buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	}
	return SUCCEEDED(voice->SubmitSourceBuffer(&buffer)) && SUCCEEDED(voice->Start());
}

bool AudioManager::StopImmediate(AudioHandle handle)
{
	if (!IsLoaded(handle)) return false;
	for (IXAudio2SourceVoice* voice : m_audio[handle].sourceVoices)
	{
		voice->Stop();
		voice->FlushSourceBuffers();
	}
	return true;
}

bool AudioManager::UnloadImmediate(AudioHandle handle)
{
	if (!IsLoaded(handle)) return false;
	Resource& audio = m_audio[handle];
	for (IXAudio2SourceVoice* voice : audio.sourceVoices)
	{
		voice->Stop();
		voice->FlushSourceBuffers();
		voice->DestroyVoice();
	}
	audio = {};
	return true;
}

void AudioManager::ProcessAudioCommands()
{
	for (const AudioCommand& command : m_commandQueue)
	{
		switch (command.type)
		{
		case CommandType::Play: PlayImmediate(command.handle, command.loop); break;
		case CommandType::Stop: StopImmediate(command.handle); break;
		case CommandType::Unload: UnloadImmediate(command.handle); break;
		}
	}
	m_commandQueue.clear();
}
