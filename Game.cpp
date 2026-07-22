#include "Game.h"

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
		state_->textureManager.Load("Resources/monsterBall.png");
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
}
