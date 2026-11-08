// 自作クラスを使わず、このファイル内の変数と通常の関数で処理する。
// 読む順番: 末尾のWinMain → Gameの初期化・更新・描画・終了 → 各機能。
// namespaceは同名の変数・関数を区別するための名前の整理に使用する。
#define DIRECTINPUT_VERSION 0x0800
#include <Windows.h>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <d3d12.h>
#include <dinput.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <wrl.h>
#include <xaudio2.h>
#include <utility>
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
#endif
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xaudio2.lib")

#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"

// ============================================================================
// 1. 描画に使うデータ構造と行列計算
// ============================================================================


// CPU側とシェーダー側で共有する、描画に必要な基本データ型。
struct Vector2 {
	float x;
	float y;
};

struct Vector3 {
	float x;
	float y;
	float z;
};

struct Vector4 {
	float x;
	float y;
	float z;
	float w;
};

struct Matrix3x3 {
	float m[3][3];
};

struct Matrix4x4 {
	float m[4][4];
};

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

struct MaterialData {
	std::string textureFilePath;
};

struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};

struct Material {
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
};

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

struct DirectionalLightData {
	Vector4 color;
	Vector3 direction;
	float intensity;
};


Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);
Matrix4x4 MakeIdentity4x4();
Matrix4x4 MakeTranslateMatrix(const Vector3& translate);
Matrix4x4 MakeScaleMatrix(const Vector3& scale);
Matrix4x4 MakeRotateXMatrix(float radian);
Matrix4x4 MakeRotateYMatrix(float radian);
Matrix4x4 MakeRotateZMatrix(float radian);
Matrix4x4 MakeAffineMatrix(
	const Vector3& scale, const Vector3& rotate, const Vector3& translate);
Matrix4x4 MakePerspectiveFovMatrix(
	float fovY, float aspectRatio, float nearClip, float farClip);
Matrix4x4 MakeOrthographicMatrix(
	float left, float top, float right, float bottom, float nearClip, float farClip);
Matrix4x4 Inverse(const Matrix4x4& matrix);


Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result = {};
	for (int i = 0; i < 4; ++i) { // 行
		for (int j = 0; j < 4; ++j) { // 列
			for (int k = 0; k < 4; ++k) { // 共通の要素
				result.m[i][j] += m1.m[i][k] * m2.m[k][j];
			}
		}
	}
	return result;
}

Matrix4x4 MakeIdentity4x4() {
	Matrix4x4 result = {};
	result.m[0][0] = 1.0f;
	result.m[1][1] = 1.0f;
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;
	return result;
}

Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
	Matrix4x4 m;
	m.m[0][0] = 1.0f; m.m[0][1] = 0.0f; m.m[0][2] = 0.0f; m.m[0][3] = 0.0f;
	m.m[1][0] = 0.0f; m.m[1][1] = 1.0f; m.m[1][2] = 0.0f; m.m[1][3] = 0.0f;
	m.m[2][0] = 0.0f; m.m[2][1] = 0.0f; m.m[2][2] = 1.0f; m.m[2][3] = 0.0f;

	m.m[3][0] = translate.x;
	m.m[3][1] = translate.y;
	m.m[3][2] = translate.z;
	m.m[3][3] = 1.0f;

	return m;
}

Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
	Matrix4x4 m;
	m.m[0][0] = scale.x; m.m[0][1] = 0.0f;    m.m[0][2] = 0.0f;    m.m[0][3] = 0.0f;
	m.m[1][0] = 0.0f;    m.m[1][1] = scale.y; m.m[1][2] = 0.0f;    m.m[1][3] = 0.0f;
	m.m[2][0] = 0.0f;    m.m[2][1] = 0.0f;    m.m[2][2] = scale.z; m.m[2][3] = 0.0f;
	m.m[3][0] = 0.0f;    m.m[3][1] = 0.0f;    m.m[3][2] = 0.0f;    m.m[3][3] = 1.0f;

	return m;
}

Matrix4x4 MakeRotateXMatrix(float radian) {
	Matrix4x4 m = {
		1,0,0,0,
		0,std::cos(radian),std::sin(radian),0,
		0,-std::sin(radian),std::cos(radian),0,
		0,0,0,1
	};

	return m;
}
Matrix4x4 MakeRotateYMatrix(float radian) {
	Matrix4x4 m = {
		std::cos(radian),0,-std::sin(radian),0,
		0,1,0,0,
		std::sin(radian),0,std::cos(radian),0,
		0,0,0,1
	};

	return m;
}
Matrix4x4 MakeRotateZMatrix(float radian) {
	Matrix4x4 m = {
		std::cos(radian),std::sin(radian),0,0,
		-std::sin(radian),std::cos(radian),0,0,
		0,0,1,0,
		0,0,0,1
	};

	return m;
}

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {

	Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);

	Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);


	Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);
	Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);
	Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);


	Matrix4x4 rotateXYZMatrix = Multiply(rotateXMatrix, Multiply(rotateYMatrix, rotateZMatrix));

	Matrix4x4 worldMatrix = Multiply(scaleMatrix, rotateXYZMatrix);
	worldMatrix = Multiply(worldMatrix, translateMatrix);

	return worldMatrix;

}

Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {

	float cot = 1.0f / std::tan(fovY / 2.0f);

	Matrix4x4 m = {
		(1.0f / aspectRatio) * cot,0,0,0,
		0,cot,0,0,
		0,0,farClip / (farClip - nearClip),1,
		0,0,(-nearClip * farClip) / (farClip - nearClip),0
	};

	return m;

}

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {

	Matrix4x4 m = {
		2 / (right - left),0,0,0,
		0,2 / (top - bottom),0,0,
		0,0,1 / (farClip - nearClip),0,
		(left + right) / (left - right),(top + bottom) / (bottom - top),nearClip / (nearClip - farClip),1
	};

	return m;
}

Matrix4x4 Inverse(const Matrix4x4& m) {
	const float* v = &m.m[0][0]; // アクセスを簡略化
	float res[16];
	float det;

	// 余因子の計算（一部の計算パターンを組み合わせて16要素分算出）
	res[0] = v[5] * v[10] * v[15] - v[5] * v[11] * v[14] - v[9] * v[6] * v[15] + v[9] * v[7] * v[14] + v[13] * v[6] * v[11] - v[13] * v[7] * v[10];
	res[4] = -v[4] * v[10] * v[15] + v[4] * v[11] * v[14] + v[8] * v[6] * v[15] - v[8] * v[7] * v[14] - v[12] * v[6] * v[11] + v[12] * v[7] * v[10];
	res[8] = v[4] * v[9] * v[15] - v[4] * v[11] * v[13] - v[8] * v[5] * v[15] + v[8] * v[7] * v[13] + v[12] * v[5] * v[11] - v[12] * v[7] * v[9];
	res[12] = -v[4] * v[9] * v[14] + v[4] * v[10] * v[13] + v[8] * v[5] * v[14] - v[8] * v[6] * v[13] - v[12] * v[5] * v[10] + v[12] * v[6] * v[9];

	res[1] = -v[1] * v[10] * v[15] + v[1] * v[11] * v[14] + v[9] * v[2] * v[15] - v[9] * v[3] * v[14] - v[13] * v[2] * v[11] + v[13] * v[3] * v[10];
	res[5] = v[0] * v[10] * v[15] - v[0] * v[11] * v[14] - v[8] * v[2] * v[15] + v[8] * v[3] * v[14] + v[12] * v[2] * v[11] - v[12] * v[3] * v[10];
	res[9] = -v[0] * v[9] * v[15] + v[0] * v[11] * v[13] + v[8] * v[1] * v[15] - v[8] * v[3] * v[13] - v[12] * v[1] * v[11] + v[12] * v[3] * v[9];
	res[13] = v[0] * v[9] * v[14] - v[0] * v[10] * v[13] - v[8] * v[1] * v[14] + v[8] * v[2] * v[13] + v[12] * v[1] * v[10] - v[12] * v[2] * v[9];

	res[2] = v[1] * v[6] * v[15] - v[1] * v[7] * v[14] - v[5] * v[2] * v[15] + v[5] * v[3] * v[14] + v[13] * v[2] * v[7] - v[13] * v[3] * v[6];
	res[6] = -v[0] * v[6] * v[15] + v[0] * v[7] * v[14] + v[4] * v[2] * v[15] - v[4] * v[3] * v[14] - v[12] * v[2] * v[7] + v[12] * v[3] * v[6];
	res[10] = v[0] * v[5] * v[15] - v[0] * v[7] * v[13] - v[4] * v[1] * v[15] + v[4] * v[3] * v[13] + v[12] * v[1] * v[7] - v[12] * v[3] * v[5];
	res[14] = -v[0] * v[5] * v[14] + v[0] * v[6] * v[13] + v[4] * v[1] * v[14] - v[4] * v[2] * v[13] - v[12] * v[1] * v[6] + v[12] * v[2] * v[5];

	res[3] = -v[1] * v[6] * v[11] + v[1] * v[7] * v[10] + v[5] * v[2] * v[11] - v[5] * v[3] * v[10] - v[9] * v[2] * v[7] + v[9] * v[3] * v[6];
	res[7] = v[0] * v[6] * v[11] - v[0] * v[7] * v[10] - v[4] * v[2] * v[11] + v[4] * v[3] * v[10] + v[8] * v[2] * v[7] - v[8] * v[3] * v[6];
	res[11] = -v[0] * v[5] * v[11] + v[0] * v[7] * v[9] + v[4] * v[1] * v[11] - v[4] * v[3] * v[9] - v[8] * v[1] * v[7] + v[8] * v[3] * v[5];
	res[15] = v[0] * v[5] * v[10] - v[0] * v[6] * v[9] - v[4] * v[1] * v[10] + v[4] * v[2] * v[9] + v[8] * v[1] * v[6] - v[8] * v[2] * v[5];

	// 行列式の計算
	det = v[0] * res[0] + v[1] * res[4] + v[2] * res[8] + v[3] * res[12];

	// 行列式が0（または極めて近い）場合は逆行列が存在しない
	if (det == 0) {
		return MakeIdentity4x4();
	}

	det = 1.0f / det;
	Matrix4x4 dest;
	for (int i = 0; i < 16; i++) {
		(&dest.m[0][0])[i] = res[i] * det;
	}

	return dest;
}

// ウィンドウプロシージャ

// ============================================================================
// 2. ウィンドウの作成とメッセージ処理
// ============================================================================
namespace LegacyWinApp {
	// 使用するデータと関数の宣言

	void Finalize();


	void Initialize(const wchar_t* title, int32_t clientWidth, int32_t clientHeight);


	LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	constexpr const wchar_t* kWindowClassName = L"CG2WindowClass";
	HINSTANCE instance_ = nullptr;
	HWND hwnd_ = nullptr;

	HWND GetHwnd() { return hwnd_; }

	// 処理の実装


	void Finalize() {
		if (hwnd_ && IsWindow(hwnd_)) {
			DestroyWindow(hwnd_);
		}
		if (instance_) {
			UnregisterClass(kWindowClassName, instance_);
		}
	}

	void Initialize(const wchar_t* title, int32_t clientWidth, int32_t clientHeight) {
		
	}



	LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
			return true;
		}
#endif

		if (msg == WM_DESTROY) {
			PostQuitMessage(0);
			return 0;
		}

		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

} // namespace WinApp

// ============================================================================
// 3. DirectX 12の初期化・描画開始・GPU同期
// ============================================================================
namespace LegacyDirectXCommon {
    // 初期化・描画準備はクラスが担当。終了処理と初期転送だけを残す。
    DirectXCommon* instance = nullptr;

void Finalize() {
		if (instance->fenceEvent_) {
			CloseHandle(instance->fenceEvent_);
			instance->fenceEvent_ = nullptr;
		}
		instance->fence_.Reset();
		instance->depthStencilResource_.Reset();
		instance->dsvDescriptorHeap_.Reset();
		for (auto& resource : instance->swapChainResources_) {
			resource.Reset();
		}
		instance->rtvDescriptorHeap_.Reset();
		instance->swapChain_.Reset();
		instance->commandList_.Reset();
		instance->commandAllocator_.Reset();
		instance->commandQueue_.Reset();
		instance->srvDescriptorHeap_.Reset();
        instance->includeHandler_.Reset();
        instance->dxcCompiler_.Reset();
        instance->dxcUtils_.Reset();
        instance->device_.Reset();
		instance->adapter_.Reset();
		instance->dxgiFactory_.Reset();
#ifdef _DEBUG
		instance->debugController_.Reset();
#endif
	}





void ExecuteCommandListAndWait() {
		HRESULT hr = instance->commandList_->Close();
		assert(SUCCEEDED(hr));
		ID3D12CommandList* commandLists[] = { instance->commandList_.Get() };
		instance->commandQueue_->ExecuteCommandLists(1, commandLists);
		instance->WaitForGPU();
		hr = instance->commandAllocator_->Reset();
		assert(SUCCEEDED(hr));
		hr = instance->commandList_->Reset(instance->commandAllocator_.Get(), nullptr);
		assert(SUCCEEDED(hr));
	}


} // namespace LegacyDirectXCommon

// ============================================================================
// 4. キーボードとマウスの入力
// ============================================================================
namespace LegacyInput {
	// 使用するデータと関数の宣言

	void Initialize(HINSTANCE hInstance, HWND hwnd);
	void Update();
	bool IsKeyPressed(uint8_t keyNumber);
	bool IsKeyReleased(uint8_t keyNumber);
	bool IsKeyTriggered(uint8_t keyNumber);
	bool IsKeyReleasedTriggered(uint8_t keyNumber);


	Microsoft::WRL::ComPtr<IDirectInput8> directInput_;
	Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;
	Microsoft::WRL::ComPtr<IDirectInputDevice8> mouse_;
	BYTE key_[256]{};
	BYTE keyPre_[256]{};
	DIMOUSESTATE2 mouseState_{};

	const DIMOUSESTATE2& GetMouseState() { return mouseState_; }

	// 処理の実装


	void Initialize(HINSTANCE hInstance, HWND hwnd) {

		HRESULT hr = directInput_->CreateDevice(
			GUID_SysMouse,
			mouse_.GetAddressOf(),
			nullptr);
		assert(SUCCEEDED(hr));

		hr = mouse_->SetDataFormat(&c_dfDIMouse2);
		assert(SUCCEEDED(hr));

		hr = mouse_->SetCooperativeLevel(
			hwnd,
			DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
		assert(SUCCEEDED(hr));
	}

	void Update() {

		HRESULT mouseResult = mouse_->Acquire();
		if (SUCCEEDED(mouseResult)) {
			mouseResult = mouse_->GetDeviceState(
				sizeof(DIMOUSESTATE2),
				&mouseState_);
		}
		if (FAILED(mouseResult)) {
			mouseState_ = {};
		}
	}

	bool IsKeyPressed(uint8_t keyNumber) {
		return key_[keyNumber] != 0;
	}

	bool IsKeyReleased(uint8_t keyNumber) {
		return key_[keyNumber] == 0;
	}

	bool IsKeyTriggered(uint8_t keyNumber) {
		return key_[keyNumber] != 0 && keyPre_[keyNumber] == 0;
	}

	bool IsKeyReleasedTriggered(uint8_t keyNumber) {
		return key_[keyNumber] == 0 && keyPre_[keyNumber] != 0;
	}

	void Finalize() {
		directInput_.Reset();
		keyboard_.Reset();
		mouse_.Reset();
	}

} // namespace Input

// ============================================================================
// 5. デバッグカメラの操作
// ============================================================================
namespace DebugCamera {
	// 使用するデータと関数の宣言


	void Initialize();

	void Update();



	// X,Y,Z軸回りのローカル回転角
	Matrix4x4 matRot_ = {};
	// ローカル座標
	Vector3 translation_ = { 0,0,-5 };
	// ビュー行列
	Matrix4x4 viewMatrix_ = {};
	// 射影行列
	Matrix4x4 projectionMatrix_ = {};

	const Matrix4x4& GetViewMatrix() {
		return viewMatrix_;
	}
	const Matrix4x4& GetProjectionMatrix() {
		return projectionMatrix_;
	}

	// 処理の実装

	void Initialize() {
		matRot_ = MakeIdentity4x4();
		viewMatrix_ = MakeIdentity4x4();

		projectionMatrix_ = MakePerspectiveFovMatrix(
			0.45f,
			1280.0f / 720.0f,
			0.1f,
			100.0f);
	}

	void Update() {
		float rotateX = 0.0f;
		float rotateY = 0.0f;
		const float rotateSpeed = 0.02f;

		//if (Input::IsKeyPressed(DIK_UP)) {
		//	rotateX += rotateSpeed;
		//}
		//if (Input::IsKeyPressed(DIK_DOWN)) {
		//	rotateX -= rotateSpeed;
		//}
		//if (Input::IsKeyPressed(DIK_LEFT)) {
		//	rotateY -= rotateSpeed;
		//}
		//if (Input::IsKeyPressed(DIK_RIGHT)) {
		//	rotateY += rotateSpeed;
		//}

		//const DIMOUSESTATE2& mouse = Input::GetMouseState();
		//if (mouse.rgbButtons[1] & 0x80) {
		//	const float mouseRotateSpeed = 0.005f;
		//	rotateX += mouse.lY * mouseRotateSpeed;
		//	rotateY += mouse.lX * mouseRotateSpeed;
		//}

		//const float zoomSpeed = 0.01f;
		//translation_.z += mouse.lZ * zoomSpeed;

		// 今回フレーム分の回転行列
		Matrix4x4 matRotDelta = MakeIdentity4x4();
		matRotDelta = Multiply(
			matRotDelta,
			MakeRotateXMatrix(rotateX));
		matRotDelta = Multiply(
			matRotDelta,
			MakeRotateYMatrix(rotateY));

		// これまでの回転へ今回の回転を追加
		matRot_ = Multiply(matRotDelta, matRot_);

		Matrix4x4 translationMatrix =
			MakeTranslateMatrix(translation_);

		Matrix4x4 cameraMatrix =
			Multiply(translationMatrix, matRot_);

		viewMatrix_ = Inverse(cameraMatrix);
	}

	void Finalize() {}

} // namespace DebugCamera

// ============================================================================
// 6. WAV音声の読み込みと再生
// ============================================================================
namespace Audio {
	// 使用するデータと関数の宣言

	struct SoundData {
		WAVEFORMATEX wfex{};
		BYTE* pBuffer = nullptr;
		unsigned int bufferSize = 0;
	};


	void Initialize();
	void Finalize();
	SoundData LoadWave(const char* filename);
	void Unload(SoundData* soundData);
	void PlayWave(const SoundData& soundData);

	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;

	std::vector<IXAudio2SourceVoice*> sourceVoices;


	// 処理の実装


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


	void Initialize() {
		HRESULT hr = XAudio2Create(
			xAudio2_.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
		assert(SUCCEEDED(hr));

		hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
		assert(SUCCEEDED(hr));
	}

	void Finalize() {
		// 音声バッファを解放する前に、再生中のボイスを破棄する。
		for (auto* voice : sourceVoices) { voice->DestroyVoice(); }
		sourceVoices.clear();
		if (masterVoice_) {
			masterVoice_->DestroyVoice();
			masterVoice_ = nullptr;
		}
		xAudio2_.Reset();
	}

	SoundData LoadWave(const char* filename) {
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

	void Unload(SoundData* soundData) {
		if (!soundData) {
			return;
		}
		delete[] soundData->pBuffer;
		soundData->pBuffer = nullptr;
		soundData->bufferSize = 0;
		soundData->wfex = {};
	}

	void PlayWave(const SoundData& soundData) {
		assert(xAudio2_.Get());
		assert(soundData.pBuffer);

		IXAudio2SourceVoice* sourceVoice = nullptr;
		HRESULT hr = xAudio2_->CreateSourceVoice(
			&sourceVoice, &soundData.wfex);
		assert(SUCCEEDED(hr));

		sourceVoices.push_back(sourceVoice);
		XAUDIO2_BUFFER buffer{};
		buffer.pAudioData = soundData.pBuffer;
		buffer.AudioBytes = soundData.bufferSize;
		buffer.Flags = XAUDIO2_END_OF_STREAM;

		hr = sourceVoice->SubmitSourceBuffer(&buffer);
		assert(SUCCEEDED(hr));
		hr = sourceVoice->Start();
		assert(SUCCEEDED(hr));
	}

} // namespace Audio

// ============================================================================
// 7. シェーダーと描画パイプライン
// ============================================================================
namespace Object3dCommon {
	// 使用するデータと関数の宣言

	void Initialize(DirectXCommon* dxCommon);
	void SetupDraw(ID3D12GraphicsCommandList* commandList);

	void CreateRootSignature(ID3D12Device* device);
	void CreateGraphicsPipeline(ID3D12Device* device);
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
		const std::wstring& filePath, const wchar_t* profile);

	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;


	// 処理の実装

	void Initialize(DirectXCommon* dxCommon) {
        ID3D12Device* device = dxCommon->GetDevice();
        dxcUtils_ = dxCommon->GetDxcUtils();
        dxcCompiler_ = dxCommon->GetDxcCompiler();
        includeHandler_ = dxCommon->GetIncludeHandler();
        CreateRootSignature(device);
		CreateGraphicsPipeline(device);
	}

	void SetupDraw(
		ID3D12GraphicsCommandList* commandList) {
		commandList->SetGraphicsRootSignature(rootSignature_.Get());
		commandList->SetPipelineState(graphicsPipelineState_.Get());
		commandList->IASetPrimitiveTopology(
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void CreateRootSignature(ID3D12Device* device) {
		D3D12_DESCRIPTOR_RANGE descriptorRange{};
		descriptorRange.BaseShaderRegister = 0;
		descriptorRange.NumDescriptors = 1;
		descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptorRange.OffsetInDescriptorsFromTableStart =
			D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_STATIC_SAMPLER_DESC staticSampler{};
		staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
		staticSampler.ShaderRegister = 0;
		staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_PARAMETER rootParameters[4]{};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[0].Descriptor.ShaderRegister = 0;
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[1].Descriptor.ShaderRegister = 0;
		rootParameters[2].ParameterType =
			D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRange;
		rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[3].Descriptor.ShaderRegister = 1;

		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumParameters = _countof(rootParameters);
		rootSignatureDesc.pStaticSamplers = &staticSampler;
		rootSignatureDesc.NumStaticSamplers = 1;

		Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3D12SerializeRootSignature(
			&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			&signatureBlob, &errorBlob);
		if (FAILED(hr)) {
			if (errorBlob) {
				OutputDebugStringA(
					reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
			}
			assert(false);
		}
		hr = device->CreateRootSignature(
			0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&rootSignature_));
		assert(SUCCEEDED(hr));
	}

	void CreateGraphicsPipeline(ID3D12Device* device) {
		D3D12_INPUT_ELEMENT_DESC inputElements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
			 D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
			 D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			 D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		};
		D3D12_INPUT_LAYOUT_DESC inputLayout{
			inputElements, _countof(inputElements) };

		D3D12_BLEND_DESC blendDesc{};
		blendDesc.RenderTarget[0].RenderTargetWriteMask =
			D3D12_COLOR_WRITE_ENABLE_ALL;
		D3D12_RASTERIZER_DESC rasterizerDesc{};
		rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		auto vertexShader = CompileShader(L"Object3d.VS.hlsl", L"vs_6_0");
		auto pixelShader = CompileShader(L"Object3d.PS.hlsl", L"ps_6_0");
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = rootSignature_.Get();
		pipelineDesc.InputLayout = inputLayout;
		pipelineDesc.VS = {
			vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		pipelineDesc.PS = {
			pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
		pipelineDesc.BlendState = blendDesc;
		pipelineDesc.RasterizerState = rasterizerDesc;
		pipelineDesc.DepthStencilState = depthStencilDesc;
		pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pipelineDesc.PrimitiveTopologyType =
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.SampleDesc.Count = 1;
		pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		HRESULT hr = device->CreateGraphicsPipelineState(
			&pipelineDesc, IID_PPV_ARGS(&graphicsPipelineState_));
		assert(SUCCEEDED(hr));
	}

	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
		const std::wstring& filePath, const wchar_t* profile) {
		Logger::Log(std::format(
			L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile));
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource;
		HRESULT hr = dxcUtils_->LoadFile(
			filePath.c_str(), nullptr, &shaderSource);
		assert(SUCCEEDED(hr));
		DxcBuffer sourceBuffer{};
		sourceBuffer.Ptr = shaderSource->GetBufferPointer();
		sourceBuffer.Size = shaderSource->GetBufferSize();
		sourceBuffer.Encoding = DXC_CP_UTF8;
		const wchar_t* arguments[] = {
			filePath.c_str(), L"-E", L"main", L"-T", profile,
			L"-Zi", L"-Qembed_debug", L"-Od", L"-Zpr" };
		Microsoft::WRL::ComPtr<IDxcResult> result;
		hr = dxcCompiler_->Compile(
			&sourceBuffer, arguments, _countof(arguments), includeHandler_.Get(),
			IID_PPV_ARGS(&result));
		assert(SUCCEEDED(hr));
		Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
		if (errors && errors->GetStringLength() != 0) {
			OutputDebugStringA(errors->GetStringPointer());
			assert(false);
		}
		Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
		hr = result->GetOutput(
			DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
		assert(SUCCEEDED(hr));
		Logger::Log(std::format(
			L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile));
		return shaderBlob;
	}

	void Finalize() {
		dxcUtils_.Reset();
		dxcCompiler_.Reset();
		includeHandler_.Reset();
		rootSignature_.Reset();
		graphicsPipelineState_.Reset();
	}

} // namespace Object3dCommon

// ============================================================================
// 8. テクスチャの読み込みと転送
// ============================================================================
namespace TextureManager {
	// 使用するデータと関数の宣言

	using TextureHandle = uint32_t;

	void Initialize(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* srvHeap);
	TextureHandle Load(const std::string& filePath);
	void ReleaseUploadResources();
	void Finalize();
	void SetDescriptorHeap(ID3D12GraphicsCommandList* commandList);

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(TextureHandle handle);


	struct TextureData {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		Microsoft::WRL::ComPtr<ID3D12Resource> uploadResource;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
	};

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(
		const DirectX::TexMetadata& metadata);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(size_t sizeInBytes);

	ID3D12Device* device_ = nullptr;
	ID3D12GraphicsCommandList* commandList_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
	uint32_t descriptorSize_ = 0;
	std::vector<TextureData> textures_;

	ID3D12DescriptorHeap* GetDescriptorHeap() {
		return srvDescriptorHeap_.Get();
	}

	// 処理の実装


	void Initialize(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* srvHeap) {
		device_ = device;
		commandList_ = commandList;
		srvDescriptorHeap_ = srvHeap;
		descriptorSize_ = device_->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		textures_.resize(1); // 0番はImGui用に予約する。
	}

	TextureHandle Load(
		const std::string& filePath) {
		DirectX::ScratchImage image;
		HRESULT hr = DirectX::LoadFromWICFile(
			StringUtility::ConvertString(filePath).c_str(), DirectX::WIC_FLAGS_FORCE_SRGB,
			nullptr, image);
		assert(SUCCEEDED(hr));
		DirectX::ScratchImage mipImages;
		hr = DirectX::GenerateMipMaps(
			image.GetImages(), image.GetImageCount(), image.GetMetadata(),
			DirectX::TEX_FILTER_SRGB, 0, mipImages);
		assert(SUCCEEDED(hr));

		TextureData texture;
		texture.resource = CreateTextureResource(mipImages.GetMetadata());
		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		DirectX::PrepareUpload(
			device_, mipImages.GetImages(), mipImages.GetImageCount(),
			mipImages.GetMetadata(), subresources);
		texture.uploadResource = CreateBuffer(GetRequiredIntermediateSize(
			texture.resource.Get(), 0, static_cast<UINT>(subresources.size())));
		UpdateSubresources(
			commandList_, texture.resource.Get(), texture.uploadResource.Get(),
			0, 0, static_cast<UINT>(subresources.size()), subresources.data());
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = texture.resource.Get();
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
		commandList_->ResourceBarrier(1, &barrier);

		const TextureHandle handle = static_cast<TextureHandle>(textures_.size());
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle =
			srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += static_cast<SIZE_T>(descriptorSize_) * handle;
		texture.gpuHandle = srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart();
		texture.gpuHandle.ptr += static_cast<UINT64>(descriptorSize_) * handle;
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = mipImages.GetMetadata().format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels =
			static_cast<UINT>(mipImages.GetMetadata().mipLevels);
		device_->CreateShaderResourceView(
			texture.resource.Get(), &srvDesc, cpuHandle);
		textures_.push_back(std::move(texture));
		return handle;
	}

	void ReleaseUploadResources() {
		for (auto& texture : textures_) {
			texture.uploadResource.Reset();
		}
	}

	void Finalize() {
		textures_.clear();
		srvDescriptorHeap_.Reset();
		commandList_ = nullptr;
		device_ = nullptr;
	}

	void SetDescriptorHeap(
		ID3D12GraphicsCommandList* commandList) {
		ID3D12DescriptorHeap* heaps[] = { srvDescriptorHeap_.Get() };
		commandList->SetDescriptorHeaps(1, heaps);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(
		TextureHandle handle) {
		assert(handle > 0 && handle < textures_.size());
		return textures_[handle].gpuHandle;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource>
		CreateTextureResource(const DirectX::TexMetadata& metadata) {
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Width = metadata.width;
		resourceDesc.Height = static_cast<UINT>(metadata.height);
		resourceDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
		resourceDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
		resourceDesc.Format = metadata.format;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		HRESULT hr = device_->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource));
		assert(SUCCEEDED(hr));
		return resource;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(
		size_t sizeInBytes) {
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeInBytes;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		HRESULT hr = device_->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
		assert(SUCCEEDED(hr));
		return resource;
	}

} // namespace TextureManager

// ============================================================================
// 9. 3Dモデルの読み込み・更新・描画
// ============================================================================
namespace Object3d {
	// 使用するデータと関数の宣言
	ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

	MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);


	void Initialize(ID3D12Device* device);
	void Update(const Matrix4x4& viewProjectionMatrix);
	void Draw(
		ID3D12GraphicsCommandList* commandList,
		D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
		D3D12_GPU_VIRTUAL_ADDRESS lightAddress);
	void Reset();


	void CreateModelVertices(ID3D12Device* device);

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(
		ID3D12Device* device,
		size_t sizeInBytes);

	uint32_t vertexCount_ = 0;

	Transform transform_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
	TransformationMatrix* transformationData_ = nullptr;

	std::string textureFilePath_;

	Transform& GetTransform() { return transform_; }
	Material& GetMaterial() { return *materialData_; }
	const std::string& GetTextureFilePath() {
		return textureFilePath_;
	}

	// 処理の実装


	ModelData LoadObjFile(
		const std::string& directoryPath,
		const std::string& filename) {

		ModelData modelData;
		std::vector<Vector4> positions;
		std::vector<Vector3> normals;
		std::vector<Vector2> texcoords;
		std::string line;

		std::ifstream file(directoryPath + "/" + filename);
		assert(file.is_open());

		while (std::getline(file, line)) {
			std::string identifier;
			std::istringstream s(line);
			s >> identifier;

			if (identifier == "v") {
				Vector4 position;
				s >> position.x >> position.y >> position.z;
				position.x *= -1.0f;
				position.w = 1.0f;
				positions.push_back(position);

			} else if (identifier == "vt") {
				Vector2 texcoord;
				s >> texcoord.x >> texcoord.y;
				texcoord.y = 1.0f - texcoord.y;
				texcoords.push_back(texcoord);

			} else if (identifier == "vn") {
				Vector3 normal;
				s >> normal.x >> normal.y >> normal.z;
				normal.x *= -1.0f;
				normals.push_back(normal);

			} else if (identifier == "f") {
				VertexData triangle[3];
				for (uint32_t faceVertex = 0;
					faceVertex < 3;
					++faceVertex) {

					std::string vertexDefinition;
					s >> vertexDefinition;

					std::istringstream v(vertexDefinition);
					uint32_t elementIndices[3];

					for (uint32_t element = 0;
						element < 3;
						++element) {

						std::string index;
						std::getline(v, index, '/');
						elementIndices[element] = std::stoi(index);
					}

					const Vector4 position =
						positions[elementIndices[0] - 1];
					const Vector2 texcoord =
						texcoords[elementIndices[1] - 1];
					const Vector3 normal =
						normals[elementIndices[2] - 1];

					triangle[faceVertex] = {
						position,
						texcoord,
						normal,
					};

				}
				modelData.vertices.push_back(triangle[2]);
				modelData.vertices.push_back(triangle[1]);
				modelData.vertices.push_back(triangle[0]);
			} else if (identifier == "mtllib") {
				// materialTemplateLibraryファイルの名前を取得する
				std::string materialFilename;
				s >> materialFilename;
				// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
				modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
			}
		}

		return modelData;
	}

	MaterialData LoadMaterialTemplateFile(
		const std::string& directoryPath,
		const std::string& filename) {
		MaterialData materialData; // 構造するMaterialData
		std::string line; // ファイルから読み込んだ1行を格納するもの
		std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
		assert(file.is_open()); // とりあえず開けなかったら止める

		while (std::getline(file, line)) {
			std::string identifier;
			std::istringstream s(line);
			s >> identifier;

			// identifierに応じた処理
			if (identifier == "map_Kd") {
				std::string textureFilename;
				s >> textureFilename;
				// 連結してファイルパスにする
				materialData.textureFilePath = directoryPath + "/" + textureFilename;
			}
		}

		return materialData;
	}

	void Initialize(ID3D12Device* device) {
		CreateModelVertices(device);

		materialResource_ = CreateBuffer(device, sizeof(Material));
		HRESULT hr = materialResource_->Map(
			0, nullptr, reinterpret_cast<void**>(&materialData_));
		assert(SUCCEEDED(hr));

		transformationResource_ =
			CreateBuffer(device, sizeof(TransformationMatrix));
		hr = transformationResource_->Map(
			0, nullptr, reinterpret_cast<void**>(&transformationData_));
		assert(SUCCEEDED(hr));

		Reset();
	}

	void Update(const Matrix4x4& viewProjectionMatrix) {
		const Matrix4x4 worldMatrix = MakeAffineMatrix(
			transform_.scale, transform_.rotate, transform_.translate);
		transformationData_->WVP = Multiply(worldMatrix, viewProjectionMatrix);
		transformationData_->World = worldMatrix;
	}

	void Draw(
		ID3D12GraphicsCommandList* commandList,
		D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
		D3D12_GPU_VIRTUAL_ADDRESS lightAddress) {
		commandList->SetGraphicsRootConstantBufferView(
			0, materialResource_->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(3, lightAddress);
		commandList->SetGraphicsRootDescriptorTable(2, textureHandle);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
		commandList->SetGraphicsRootConstantBufferView(
			1, transformationResource_->GetGPUVirtualAddress());
		commandList->DrawInstanced(vertexCount_, 1, 0, 0);
	}

	void Reset() {
		transform_ = {
			{0.5f, 0.5f, 0.5f},
			{0.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 0.0f},
		};
		materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		materialData_->enableLighting = true;
		materialData_->uvTransform = MakeIdentity4x4();
		transformationData_->WVP = MakeIdentity4x4();
		transformationData_->World = MakeIdentity4x4();
	}

	void CreateModelVertices(ID3D12Device* device) {
		// OBJファイルから頂点情報を読み込む
		const ModelData modelData =
			LoadObjFile("Resources", "axis.obj");
		textureFilePath_ = modelData.material.textureFilePath;
		// 読み込んだ頂点数を保存する
		vertexCount_ =
			static_cast<uint32_t>(modelData.vertices.size());

		// 頂点バッファを作成する
		const size_t vertexBufferSize =
			sizeof(VertexData) * modelData.vertices.size();

		vertexResource_ =
			CreateBuffer(device, vertexBufferSize);

		// 頂点バッファビューを設定する
		vertexBufferView_.BufferLocation =
			vertexResource_->GetGPUVirtualAddress();

		vertexBufferView_.SizeInBytes =
			static_cast<UINT>(vertexBufferSize);

		vertexBufferView_.StrideInBytes =
			sizeof(VertexData);

		// 頂点バッファへモデルの頂点データをコピーする
		VertexData* vertexData = nullptr;

		const HRESULT hr = vertexResource_->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&vertexData));

		assert(SUCCEEDED(hr));

		std::memcpy(
			vertexData,
			modelData.vertices.data(),
			vertexBufferSize);
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(
		ID3D12Device* device, size_t sizeInBytes) {
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeInBytes;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		HRESULT hr = device->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&resource));
		assert(SUCCEEDED(hr));
		return resource;
	}

	void Finalize() {
		vertexResource_.Reset();
		materialResource_.Reset();
		transformationResource_.Reset();
	}

} // namespace Object3d

// ============================================================================
// 10. 2Dスプライトの更新と描画
// ============================================================================
namespace Sprite {
	// 使用するデータと関数の宣言

	void Initialize(ID3D12Device* device);
	void Update(float screenWidth, float screenHeight);
	void Draw(
		ID3D12GraphicsCommandList* commandList,
		D3D12_GPU_DESCRIPTOR_HANDLE textureHandle);
	void Reset();


	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(
		ID3D12Device* device, size_t sizeInBytes);

	Transform transform_{};
	Transform uvTransform_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
	TransformationMatrix* transformationData_ = nullptr;


	Transform& GetTransform() { return transform_; }
	Transform& GetUVTransform() { return uvTransform_; }
	Material& GetMaterial() { return *materialData_; }

	// 処理の実装


	void Initialize(ID3D12Device* device) {
		vertexResource_ = CreateBuffer(device, sizeof(VertexData) * 6);
		vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
		vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
		vertexBufferView_.StrideInBytes = sizeof(VertexData);

		VertexData* vertices = nullptr;
		HRESULT hr = vertexResource_->Map(
			0, nullptr, reinterpret_cast<void**>(&vertices));
		assert(SUCCEEDED(hr));
		vertices[0] = { {0.0f, 360.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f} };
		vertices[1] = { {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} };
		vertices[2] = { {640.0f, 360.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f} };
		vertices[3] = { {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} };
		vertices[4] = { {640.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f} };
		vertices[5] = { {640.0f, 360.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f} };

		indexResource_ = CreateBuffer(device, sizeof(uint32_t) * 6);

		indexBufferView_.BufferLocation =
			indexResource_->GetGPUVirtualAddress();
		indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
		indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

		uint32_t* indices = nullptr;
		hr = indexResource_->Map(
			0, nullptr, reinterpret_cast<void**>(&indices));
		assert(SUCCEEDED(hr));

		indices[0] = 0;
		indices[1] = 1;
		indices[2] = 2;
		indices[3] = 3;
		indices[4] = 4;
		indices[5] = 5;

		materialResource_ = CreateBuffer(device, sizeof(Material));
		hr = materialResource_->Map(
			0, nullptr, reinterpret_cast<void**>(&materialData_));
		assert(SUCCEEDED(hr));

		transformationResource_ =
			CreateBuffer(device, sizeof(TransformationMatrix));
		hr = transformationResource_->Map(
			0, nullptr, reinterpret_cast<void**>(&transformationData_));
		assert(SUCCEEDED(hr));

		Reset();
	}

	void Update(float screenWidth, float screenHeight) {
		const Matrix4x4 worldMatrix = MakeAffineMatrix(
			transform_.scale, transform_.rotate, transform_.translate);
		const Matrix4x4 projectionMatrix = MakeOrthographicMatrix(
			0.0f, 0.0f, screenWidth, screenHeight, 0.0f, 100.0f);
		transformationData_->WVP = Multiply(worldMatrix, projectionMatrix);
		transformationData_->World = worldMatrix;
		Matrix4x4 uvTransformMatrix =
			MakeScaleMatrix(uvTransform_.scale);
		uvTransformMatrix = Multiply(
			uvTransformMatrix,
			MakeRotateZMatrix(uvTransform_.rotate.z));
		uvTransformMatrix = Multiply(
			uvTransformMatrix,
			MakeTranslateMatrix(uvTransform_.translate));
		materialData_->uvTransform = uvTransformMatrix;
	}

	void Draw(
		ID3D12GraphicsCommandList* commandList,
		D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) {
		commandList->SetGraphicsRootConstantBufferView(
			0, materialResource_->GetGPUVirtualAddress());
		commandList->IASetVertexBuffers(
			0, 1, &vertexBufferView_);
		commandList->IASetIndexBuffer(
			&indexBufferView_);
		commandList->SetGraphicsRootConstantBufferView(
			1, transformationResource_->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(
			2, textureHandle);
		commandList->DrawIndexedInstanced(
			6, 1, 0, 0, 0);
	}

	void Reset() {
		transform_ = {
			{1.0f, 1.0f, 1.0f},
			{0.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 0.0f},
		};
		uvTransform_ = {
		 {1.0f, 1.0f, 1.0f},
		 {0.0f, 0.0f, 0.0f},
		 {0.0f, 0.0f, 0.0f},
		};
		materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		materialData_->enableLighting = false;
		materialData_->uvTransform = MakeIdentity4x4();
		transformationData_->WVP = MakeIdentity4x4();
		transformationData_->World = MakeIdentity4x4();
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(
		ID3D12Device* device, size_t sizeInBytes) {
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeInBytes;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		HRESULT hr = device->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&resource));
		assert(SUCCEEDED(hr));
		return resource;
	}

	void Finalize() {
		vertexResource_.Reset();
		indexResource_.Reset();
		materialResource_.Reset();
		transformationResource_.Reset();
	}

} // namespace Sprite

// ============================================================================
// 11. 平行光源の設定
// ============================================================================
namespace DirectionalLight {
	// 使用するデータと関数の宣言

	void Initialize(ID3D12Device* device);
	void Reset();


	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
	DirectionalLightData* data_ = nullptr;

	DirectionalLightData& GetData() { return *data_; }
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() {
		return resource_->GetGPUVirtualAddress();
	}

	// 処理の実装


	void Initialize(ID3D12Device* device) {
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeof(DirectionalLightData);
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		HRESULT hr = device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&resource_));
		assert(SUCCEEDED(hr));

		hr = resource_->Map(0, nullptr, reinterpret_cast<void**>(&data_));
		assert(SUCCEEDED(hr));
		Reset();
	}

	void Reset() {
		data_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		data_->direction = { 0.0f, -1.0f, 0.0f };
		data_->intensity = 1.0f;
	}

	void Finalize() {
		resource_.Reset();
	}

} // namespace DirectionalLight

// ============================================================================
// 12. ゲーム全体の処理（初期化 → 更新 → 描画 → 終了）
// ============================================================================
namespace Game {

	// 入力クラスのポインタ
	Input* input = nullptr;
	// ウィンドウクラスのポインタ
	WinApp* winApp = nullptr;
    DirectXCommon* dxCommon = nullptr;

	Audio::SoundData soundData{};


	TextureManager::TextureHandle uvCheckerTexture = 0;
	TextureManager::TextureHandle monsterBallTexture = 0;
	Transform cameraTransform{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, -5.0f},
	};
	bool useDebugCamera = true;
	bool useMonsterBall = false;

	void Initialize();
	void Update();
	void Draw();
	void Finalize();
	void Run() {
		Initialize();

		while (true) {
			// 終了メッセージを受け取ったらゲームループを抜ける。
			if (winApp->ProcessMessage()) {
				break;
			}
			Update();
			Draw();
		}

		Finalize();
		// 描画リソース → 入力 → DirectX → ウィンドウの順で破棄する。
		DirectionalLight::Finalize();
		Sprite::Finalize();
		Object3d::Finalize();
		TextureManager::Finalize();
		Object3dCommon::Finalize();
		LegacyDirectXCommon::Finalize();
        delete dxCommon;
        dxCommon = nullptr;
        LegacyDirectXCommon::instance = nullptr;
		// WindowsAPIの終了と解放
		winApp->Finalize();
		delete winApp;
		winApp = nullptr;

#ifdef _DEBUG
		IDXGIDebug1* debug = nullptr;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->Release();
		}
#endif
	}

	// 起動時に一度だけ実行。ウィンドウ・入力・GPU・描画用データを準備する。
	void Initialize() {

		// WindowsAPIの初期化
		winApp = new WinApp();
		winApp->Initialize();
		// 入力クラスの生成と初期化
		input = new Input();
		input->Initialize(winApp);
		dxCommon = new DirectXCommon();
        LegacyDirectXCommon::instance = dxCommon;
        dxCommon->Initialize(winApp);
	

		// XAudioエンジンのインスタンスを生成
		Audio::Initialize();
		// マスターボイスはAudio::Initialize内で生成済み。

		// 音声データを読み込む
		soundData =
			Audio::LoadWave("Resources/fanfare.wav");

		ID3D12Device* device = LegacyDirectXCommon::instance->GetDevice();
		ID3D12GraphicsCommandList* commandList =
			LegacyDirectXCommon::instance->GetCommandList();
		Object3dCommon::Initialize(dxCommon);
		TextureManager::Initialize(device, commandList, dxCommon->GetSrvDescriptorHeap());
		Object3d::Initialize(device);
		Sprite::Initialize(device);
		DirectionalLight::Initialize(device);
		DebugCamera::Initialize();

		uvCheckerTexture =
			TextureManager::Load("Resources/uvChecker.png");
		monsterBallTexture =
			TextureManager::Load(Object3d::GetTextureFilePath());
		LegacyDirectXCommon::ExecuteCommandListAndWait();
		TextureManager::ReleaseUploadResources();


	}

	// 毎フレーム実行。入力と設定画面を反映し、描画用の行列を更新する。
	void Update() {
		// 入力の更新
		input->Update();
		DebugCamera::Update();

		if (input->TriggerKey(DIK_0)) {
			OutputDebugStringW(L"0キーを押した瞬間\n");
		}

#ifdef USE_IMGUI
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("Settings");

		ImGui::Checkbox(
			"Use Debug Camera",
			&useDebugCamera);

		if (ImGui::Button("Play Sound")) {
			Audio::PlayWave(soundData);
		}

		if (ImGui::CollapsingHeader(
			"Sphere", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat3(
				"Translate##Sphere",
				&Object3d::GetTransform().translate.x,
				0.01f);
			ImGui::DragFloat3(
				"Rotate##Sphere",
				&Object3d::GetTransform().rotate.x,
				0.01f);
			ImGui::DragFloat3(
				"Scale##Sphere",
				&Object3d::GetTransform().scale.x,
				0.01f);
			ImGui::ColorEdit4(
				"Color##Sphere",
				&Object3d::GetMaterial().color.x);
			bool lighting =
				Object3d::GetMaterial().enableLighting != 0;
			if (ImGui::Checkbox("Enable Lighting##Sphere", &lighting)) {
				Object3d::GetMaterial().enableLighting = lighting ? 1 : 0;
			}
			ImGui::Checkbox("Use Monster Ball", &useMonsterBall);
		}

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Sprite")) {
			ImGui::DragFloat3(
				"Translate##Sprite",
				&Sprite::GetTransform().translate.x,
				1.0f);
			ImGui::ColorEdit4(
				"Color##Sprite",
				&Sprite::GetMaterial().color.x);
			ImGui::DragFloat2(
				"UV Translate##Sprite",
				&Sprite::GetUVTransform().translate.x,
				0.01f,
				-10.0f,
				10.0f);
			ImGui::DragFloat2(
				"UV Scale##Sprite",
				&Sprite::GetUVTransform().scale.x,
				0.01f,
				-10.0f,
				10.0f);
			ImGui::SliderAngle(
				"UV Rotate##Sprite",
				&Sprite::GetUVTransform().rotate.z);
			bool lighting =
				Sprite::GetMaterial().enableLighting != 0;
			if (ImGui::Checkbox("Enable Lighting##Sprite", &lighting)) {
				Sprite::GetMaterial().enableLighting = lighting ? 1 : 0;
			}
		}

		ImGui::Separator();
		if (ImGui::CollapsingHeader(
			"Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::ColorEdit4(
				"Color##Light",
				&DirectionalLight::GetData().color.x);
			ImGui::DragFloat3(
				"Direction##Light",
				&DirectionalLight::GetData().direction.x,
				0.01f,
				-1.0f,
				1.0f);
			ImGui::DragFloat(
				"Intensity##Light",
				&DirectionalLight::GetData().intensity,
				0.01f,
				0.0f,
				10.0f);
		}

		if (ImGui::Button("Reset")) {
			Object3d::Reset();
			Sprite::Reset();
			DirectionalLight::Reset();
		}
		ImGui::End();
#endif

		const Matrix4x4 cameraMatrix = MakeAffineMatrix(
			cameraTransform.scale,
			cameraTransform.rotate,
			cameraTransform.translate);
		const Matrix4x4 viewMatrix = Inverse(cameraMatrix);
		const Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
			0.45f,
			static_cast<float>(WinApp::kClientWidth) / WinApp::kClientHeight,
			0.1f,
			100.0f);
		const Matrix4x4 viewProjectionMatrix =
			useDebugCamera
			? Multiply(
				DebugCamera::GetViewMatrix(),
				DebugCamera::GetProjectionMatrix())
			: Multiply(viewMatrix, projectionMatrix);
		Object3d::Update(viewProjectionMatrix);
		Sprite::Update(
			static_cast<float>(WinApp::kClientWidth),
			static_cast<float>(WinApp::kClientHeight));
	}

	// 毎フレーム実行。3Dモデル → スプライト → ImGuiの順に描画する。
	void Draw() {
		ID3D12GraphicsCommandList* commandList =
			LegacyDirectXCommon::instance->GetCommandList();
		dxCommon->PreDraw();
		Object3dCommon::SetupDraw(commandList);

		const auto sphereTexture = TextureManager::GetGPUHandle(
			useMonsterBall
			? monsterBallTexture
			: uvCheckerTexture);
		Object3d::Draw(
			commandList,
			sphereTexture,
			DirectionalLight::GetGPUVirtualAddress());
		Sprite::Draw(
			commandList,
			TextureManager::GetGPUHandle(uvCheckerTexture));

#ifdef USE_IMGUI
		ImGui::ShowDemoWindow();
		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif

		dxCommon->PostDraw();
	}

	// 終了時に一度だけ実行。ImGuiと音声を終了する。
	void Finalize() {
#ifdef USE_IMGUI
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
#endif
		// XAudio2を先に解放
		Audio::Finalize();

		// 音声データを解放
		Audio::Unload(&soundData);

		// 入力クラスの解放
		delete input;
		input = nullptr;
	}

} // namespace Game

// ============================================================================
// 13. エントリーポイント
// ============================================================================
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	Game::Run();
	return 0;
}
