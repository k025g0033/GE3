#pragma once

#include <Windows.h>
#include <xaudio2.h>
#include <wrl.h>

class Audio {
public:
	struct SoundData {
		WAVEFORMATEX wfex{};
		BYTE* pBuffer = nullptr;
		unsigned int bufferSize = 0;
	};

	~Audio();

	void Initialize();
	void Finalize();
	SoundData LoadWave(const char* filename);
	void Unload(SoundData* soundData);
	void PlayWave(const SoundData& soundData);

private:
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;
};
