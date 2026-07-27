#include "Game.h"

#include "Audio.h"
#include <xaudio2.h>
#include <fstream>

#pragma comment(lib, "xaudio2.lib")

#include "DirectXCommon.h"
#include "DirectionalLight.h"
#include "MathFunctions.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "WinApp.h"

#include <Windows.h>
#include <cassert>
#include <dxgidebug.h>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

namespace {
	constexpr int32_t kClientWidth = 1280;
	constexpr int32_t kClientHeight = 720;

	// チャンクヘッダ
	struct ChunkHeader {
		char id[4];
		int32_t size;
	};

	// RIFFヘッダ
	struct RiffHeader {
		ChunkHeader chunk;
		char type[4];
	};

	// fmtチャンク
	struct FormatChunk {
		ChunkHeader chunk;
		WAVEFORMATEX fmt;
	};

	// 音声データ
	struct SoundData {
		// 波形フォーマット
		WAVEFORMATEX wfex;
		// バッファの先頭アドレス
		BYTE* pBuffer;
		// バッファのサイズ
		unsigned int bufferSize;
	};

	SoundData SoundLoadWave(const char* filename) {
		// ファイル入力ストリームのインスタンス
		std::ifstream file;
		// .wavファイルをバイナリモードで開く
		file.open(filename, std::ios_base::binary);
		// ファイルオープン失敗を検出する
		assert(file.is_open());

		// RIFFヘッダーの読み込み
		RiffHeader riff;
		file.read((char*)&riff, sizeof(riff));
		// ファイルガRIFFかチェック
		if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
			assert(0);
		}
		// タイプがWAVEかチェック
		if (strncmp(riff.type, "WAVE", 4) != 0) {
			assert(0);
		}
		// Formatチャンクの読み込み
		FormatChunk format = {};
		// チャンクヘッダーの確認
		file.read((char*)&format, sizeof(ChunkHeader));
		if (strncmp(format.chunk.id, "fmt", 4) != 0) {
			assert(0);
		}
		// チャンク本体の読み込み
		assert(format.chunk.size <= sizeof(format.fmt));
		file.read((char*)&format.fmt, format.chunk.size);
		// Dataチャンクの読み込み
		ChunkHeader data;
		file.read((char*)&data, sizeof(data));
		// JUNKチャンクを検出した場合
		if (strncmp(data.id, "JUNK", 4) == 0) {
			// 読み取り位置をJUNKチャンクの終わりまで進める
			file.seekg(data.size, std::ios_base::cur);
			// 再読み込み
			file.read((char*)&data, sizeof(data));
		}
		if (strncmp(data.id, "data", 4) != 0) {
			assert(0);
		}
		// Dataチャンクのデータ部(波形データ)の読み込み
		char* pBuffer = new char[data.size];
		file.read(pBuffer, data.size);
		// Waveファイルを閉じる
		file.close();

		// returnする為の音声データ
		SoundData soundData = {};

		soundData.wfex = format.fmt;
		soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
		soundData.bufferSize = data.size;

		return soundData;
	}

	void SoundUnload(SoundData* soundData) {
		delete[] soundData->pBuffer;
		soundData->pBuffer = nullptr;
		soundData->bufferSize = 0;
		soundData->wfex = {};
	}

	void SoundPlayWave(
		IXAudio2* xAudio2,
		const SoundData& soundData) {

		HRESULT hr;

		// ソースボイスを生成
		IXAudio2SourceVoice* sourceVoice = nullptr;
		hr = xAudio2->CreateSourceVoice(
			&sourceVoice,
			&soundData.wfex);
		assert(SUCCEEDED(hr));

		// 再生する波形データを設定
		XAUDIO2_BUFFER buffer{};
		buffer.pAudioData = soundData.pBuffer;
		buffer.AudioBytes = soundData.bufferSize;
		buffer.Flags = XAUDIO2_END_OF_STREAM;

		// 波形データを送って再生
		hr = sourceVoice->SubmitSourceBuffer(&buffer);
		assert(SUCCEEDED(hr));

		hr = sourceVoice->Start();
		assert(SUCCEEDED(hr));
	}
}

struct Game::State {
	// 宣言順の逆順に破棄されるため、DirectXCommonを最初に置く。
	WinApp winApp;
	DirectXCommon dxCommon;
	Object3dCommon object3dCommon;
	TextureManager textureManager;
	Object3d sphere;
	Sprite sprite;
	DirectionalLight directionalLight;

	Audio audio;
	Audio::SoundData soundData{};

	TextureManager::TextureHandle uvCheckerTexture = 0;
	TextureManager::TextureHandle monsterBallTexture = 0;
	Transform cameraTransform{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, -5.0f},
	};
	bool useMonsterBall = false;
};

Game::Game() = default;
Game::~Game() = default;

void Game::Run() {
	Initialize();

	MSG msg{};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			Update();
			Draw();
		}
	}

	Finalize();
	state_.reset();

#ifdef _DEBUG
	IDXGIDebug1* debug = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}
#endif
	CoUninitialize();
}

void Game::Initialize() {
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	assert(SUCCEEDED(hr));

	state_ = std::make_unique<State>();
	state_->winApp.Initialize(L"CG2", kClientWidth, kClientHeight);
	state_->dxCommon.Initialize(
		state_->winApp.GetHwnd(), kClientWidth, kClientHeight);

	// XAudioエンジンのインスタンスを生成
	state_->audio.Initialize();
	// マスターボイスを生成

	// 音声データを読み込む
	state_->soundData =
		state_->audio.LoadWave("Resources/fanfare.wav");

	// 音声を再生
	state_->audio.PlayWave(state_->soundData);

	ID3D12Device* device = state_->dxCommon.GetDevice();
	ID3D12GraphicsCommandList* commandList =
		state_->dxCommon.GetCommandList();
	state_->object3dCommon.Initialize(device);
	state_->textureManager.Initialize(device, commandList);
	state_->sphere.Initialize(device);
	state_->sprite.Initialize(device);
	state_->directionalLight.Initialize(device);

	state_->uvCheckerTexture =
		state_->textureManager.Load("Resources/uvChecker.png");
	state_->monsterBallTexture =
		state_->textureManager.Load(state_->sphere.GetTextureFilePath());
	state_->dxCommon.ExecuteCommandListAndWait();
	state_->textureManager.ReleaseUploadResources();

#ifdef USE_IMGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(state_->winApp.GetHwnd());
	ID3D12DescriptorHeap* srvHeap =
		state_->textureManager.GetDescriptorHeap();
	ImGui_ImplDX12_Init(
		device,
		2,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		srvHeap,
		srvHeap->GetCPUDescriptorHandleForHeapStart(),
		srvHeap->GetGPUDescriptorHandleForHeapStart());
	ImGui::GetIO().Fonts->Build();
#endif
}

void Game::Update() {
#ifdef USE_IMGUI
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Settings");

	if (ImGui::CollapsingHeader(
		"Sphere", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3(
			"Translate##Sphere",
			&state_->sphere.GetTransform().translate.x,
			0.01f);
		ImGui::DragFloat3(
			"Rotate##Sphere",
			&state_->sphere.GetTransform().rotate.x,
			0.01f);
		ImGui::DragFloat3(
			"Scale##Sphere",
			&state_->sphere.GetTransform().scale.x,
			0.01f);
		ImGui::ColorEdit4(
			"Color##Sphere",
			&state_->sphere.GetMaterial().color.x);
		bool lighting =
			state_->sphere.GetMaterial().enableLighting != 0;
		if (ImGui::Checkbox("Enable Lighting##Sphere", &lighting)) {
			state_->sphere.GetMaterial().enableLighting = lighting ? 1 : 0;
		}
		ImGui::Checkbox("Use Monster Ball", &state_->useMonsterBall);
	}

	ImGui::Separator();
	if (ImGui::CollapsingHeader("Sprite")) {
		ImGui::DragFloat3(
			"Translate##Sprite",
			&state_->sprite.GetTransform().translate.x,
			1.0f);
		ImGui::ColorEdit4(
			"Color##Sprite",
			&state_->sprite.GetMaterial().color.x);
		ImGui::DragFloat2(
			"UV Translate##Sprite",
			&state_->sprite.GetUVTransform().translate.x,
			0.01f,
			-10.0f,
			10.0f);
		ImGui::DragFloat2(
			"UV Scale##Sprite",
			&state_->sprite.GetUVTransform().scale.x,
			0.01f,
			-10.0f,
			10.0f);
		ImGui::SliderAngle(
			"UV Rotate##Sprite",
			&state_->sprite.GetUVTransform().rotate.z);
		bool lighting =
			state_->sprite.GetMaterial().enableLighting != 0;
		if (ImGui::Checkbox("Enable Lighting##Sprite", &lighting)) {
			state_->sprite.GetMaterial().enableLighting = lighting ? 1 : 0;
		}
	}

	ImGui::Separator();
	if (ImGui::CollapsingHeader(
		"Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::ColorEdit4(
			"Color##Light",
			&state_->directionalLight.GetData().color.x);
		ImGui::DragFloat3(
			"Direction##Light",
			&state_->directionalLight.GetData().direction.x,
			0.01f,
			-1.0f,
			1.0f);
		ImGui::DragFloat(
			"Intensity##Light",
			&state_->directionalLight.GetData().intensity,
			0.01f,
			0.0f,
			10.0f);
	}

	if (ImGui::Button("Reset")) {
		state_->sphere.Reset();
		state_->sprite.Reset();
		state_->directionalLight.Reset();
	}
	ImGui::End();
#endif

	const Matrix4x4 cameraMatrix = MakeAffineMatrix(
		state_->cameraTransform.scale,
		state_->cameraTransform.rotate,
		state_->cameraTransform.translate);
	const Matrix4x4 viewMatrix = Inverse(cameraMatrix);
	const Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
		0.45f,
		static_cast<float>(kClientWidth) / kClientHeight,
		0.1f,
		100.0f);
	state_->sphere.Update(Multiply(viewMatrix, projectionMatrix));
	state_->sprite.Update(
		static_cast<float>(kClientWidth),
		static_cast<float>(kClientHeight));
}

void Game::Draw() {
	ID3D12GraphicsCommandList* commandList =
		state_->dxCommon.GetCommandList();
	state_->dxCommon.BeginFrame();
	state_->object3dCommon.SetupDraw(commandList);
	state_->textureManager.SetDescriptorHeap(commandList);

	const auto sphereTexture = state_->textureManager.GetGPUHandle(
		state_->useMonsterBall
		? state_->monsterBallTexture
		: state_->uvCheckerTexture);
	state_->sphere.Draw(
		commandList,
		sphereTexture,
		state_->directionalLight.GetGPUVirtualAddress());
	state_->sprite.Draw(
		commandList,
		state_->textureManager.GetGPUHandle(state_->uvCheckerTexture));

#ifdef USE_IMGUI
	ImGui::ShowDemoWindow();
	ImGui::Render();
	state_->textureManager.SetDescriptorHeap(commandList);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif

	state_->dxCommon.EndFrame();
}

void Game::Finalize() {
#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
	// XAudio2を先に解放
	state_->audio.Finalize();

	// 音声データを解放
	state_->audio.Unload(&state_->soundData);
}
