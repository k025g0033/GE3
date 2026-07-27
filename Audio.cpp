#include "Audio.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>

#pragma comment(lib, "xaudio2.lib")

namespace {
struct ChunkHeader {
	char id[4];
	int32_t size;
};

struct RiffHeader {
	ChunkHeader chunk;
	char type[4];
};

struct FormatChunk {
	ChunkHeader chunk;
	WAVEFORMATEX fmt;
};
}

Audio::~Audio() {
	Finalize();
}

void Audio::Initialize() {
	HRESULT hr = XAudio2Create(
		xAudio2_.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(hr));

	hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
	assert(SUCCEEDED(hr));
}

void Audio::Finalize() {
	if (masterVoice_) {
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
	}
	xAudio2_.Reset();
}

Audio::SoundData Audio::LoadWave(const char* filename) {
	std::ifstream file(filename, std::ios_base::binary);
	assert(file.is_open());

	RiffHeader riff{};
	file.read(reinterpret_cast<char*>(&riff), sizeof(riff));
	assert(strncmp(riff.chunk.id, "RIFF", 4) == 0);
	assert(strncmp(riff.type, "WAVE", 4) == 0);

	FormatChunk format{};
	file.read(reinterpret_cast<char*>(&format.chunk), sizeof(ChunkHeader));
	assert(strncmp(format.chunk.id, "fmt ", 4) == 0);
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read(
		reinterpret_cast<char*>(&format.fmt),
		format.chunk.size);

	ChunkHeader data{};
	file.read(reinterpret_cast<char*>(&data), sizeof(data));
	if (strncmp(data.id, "JUNK", 4) == 0) {
		file.seekg(data.size, std::ios_base::cur);
		file.read(reinterpret_cast<char*>(&data), sizeof(data));
	}
	assert(strncmp(data.id, "data", 4) == 0);

	char* buffer = new char[data.size];
	file.read(buffer, data.size);
	file.close();

	SoundData soundData{};
	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(buffer);
	soundData.bufferSize = data.size;
	return soundData;
}

void Audio::Unload(SoundData* soundData) {
	if (!soundData) {
		return;
	}
	delete[] soundData->pBuffer;
	soundData->pBuffer = nullptr;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

void Audio::PlayWave(const SoundData& soundData) {
	assert(xAudio2_.Get());
	assert(soundData.pBuffer);

	IXAudio2SourceVoice* sourceVoice = nullptr;
	HRESULT hr = xAudio2_->CreateSourceVoice(
		&sourceVoice, &soundData.wfex);
	assert(SUCCEEDED(hr));

	XAUDIO2_BUFFER buffer{};
	buffer.pAudioData = soundData.pBuffer;
	buffer.AudioBytes = soundData.bufferSize;
	buffer.Flags = XAUDIO2_END_OF_STREAM;

	hr = sourceVoice->SubmitSourceBuffer(&buffer);
	assert(SUCCEEDED(hr));
	hr = sourceVoice->Start();
	assert(SUCCEEDED(hr));
}
