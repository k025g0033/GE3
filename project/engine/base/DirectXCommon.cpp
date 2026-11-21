#include "DirectXCommon.h"
#include <cassert>
#include <format>
#include "Logger.h"
#include "StringUtility.h"
#include "externals/DirectXTex/d3dx12.h"
#include <vector>
#include <thread>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#endif
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")



void DirectXCommon::Initialize(WinApp* winApp) {
    InitializeFixFPS();
    assert(winApp);
    winApp_ = winApp;
#ifdef _DEBUG
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController_)))) {
        debugController_->EnableDebugLayer();
        debugController_->SetEnableGPUBasedValidation(TRUE);
    }
#endif
    // 初期化の順番: デバイス → コマンド → 画面 → 描画用データ → 補助機能。
    CreateDevice();
    SetupDebugInfoQueue();
    CreateCommandObjects();
    CreateSwapChain(winApp_->GetHwnd(), WinApp::kClientWidth, WinApp::kClientHeight);
    InitializeDescriptorHeaps();
    CreateDepthStencil(WinApp::kClientWidth, WinApp::kClientHeight);
    CreateRenderTargets();
    CreateFence();
    InitializeViewport();
    InitializeScissorRect();
    InitializeDxc();
    InitializeImGui();
}

void DirectXCommon::CreateDevice() {
		HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory_));
		assert(SUCCEEDED(hr));

		for (UINT index = 0; dxgiFactory_->EnumAdapterByGpuPreference(
			index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&adapter_)) != DXGI_ERROR_NOT_FOUND;
			++index) {
			DXGI_ADAPTER_DESC3 adapterDesc{};
			hr = adapter_->GetDesc3(&adapterDesc);
			assert(SUCCEEDED(hr));
			if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
				Logger::Log(std::format(L"Use Adapter:{}\n", adapterDesc.Description));
				break;
			}
			adapter_.Reset();
		}
		assert(adapter_ != nullptr);

		const D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_12_2,
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
		};
		const char* featureLevelNames[] = { "12.2", "12.1", "12.0" };
		for (size_t index = 0; index < _countof(featureLevels); ++index) {
			hr = D3D12CreateDevice(
				adapter_.Get(), featureLevels[index], IID_PPV_ARGS(&device_));
			if (SUCCEEDED(hr)) {
				Logger::Log(std::format("FeatureLevel :{}\n", featureLevelNames[index]));
				break;
			}
		}
		assert(device_ != nullptr);
		Logger::Log("Complete create D3D12Device!!!\n");
	}

void DirectXCommon::SetupDebugInfoQueue() {
#ifdef _DEBUG
		Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
		if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

			D3D12_MESSAGE_ID denyIds[] = {
				D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
			};
			D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
			D3D12_INFO_QUEUE_FILTER filter{};
			filter.DenyList.NumIDs = _countof(denyIds);
			filter.DenyList.pIDList = denyIds;
			filter.DenyList.NumSeverities = _countof(severities);
			filter.DenyList.pSeverityList = severities;
			infoQueue->PushStorageFilter(&filter);
		}
#endif
	}

void DirectXCommon::CreateCommandObjects() {
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
		HRESULT hr = device_->CreateCommandQueue(
			&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
		assert(SUCCEEDED(hr));

		hr = device_->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
		assert(SUCCEEDED(hr));

		hr = device_->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr,
			IID_PPV_ARGS(&commandList_));
		assert(SUCCEEDED(hr));
	}

void DirectXCommon::CreateSwapChain(
		HWND hwnd, int32_t clientWidth, int32_t clientHeight) {
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
		swapChainDesc.Width = clientWidth;
		swapChainDesc.Height = clientHeight;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(
			commandQueue_.Get(), hwnd, &swapChainDesc, nullptr, nullptr,
			reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
		assert(SUCCEEDED(hr));
	}

void DirectXCommon::CreateRenderTargets() {
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		for (UINT index = 0; index < swapChainResources_.size(); ++index) {
			HRESULT hr = swapChain_->GetBuffer(
				index, IID_PPV_ARGS(&swapChainResources_[index]));
			assert(SUCCEEDED(hr));
			rtvHandles_[index] = GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), descriptorSizeRTV_, index);
			device_->CreateRenderTargetView(
				swapChainResources_[index].Get(), &rtvDesc, rtvHandles_[index]);
		}
	}

void DirectXCommon::CreateDepthStencil(
		int32_t clientWidth, int32_t clientHeight) {
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Width = clientWidth;
		resourceDesc.Height = clientHeight;
		resourceDesc.MipLevels = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_CLEAR_VALUE clearValue{};
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		HRESULT hr = device_->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
			IID_PPV_ARGS(&depthStencilResource_));
		assert(SUCCEEDED(hr));

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		device_->CreateDepthStencilView(
			depthStencilResource_.Get(), &dsvDesc,
			GetCPUDescriptorHandle(dsvDescriptorHeap_.Get(), descriptorSizeDSV_, 0));
	}

void DirectXCommon::CreateFence() {
		HRESULT hr = device_->CreateFence(
			fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
		assert(SUCCEEDED(hr));
		fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		assert(fenceEvent_ != nullptr);
	}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
		DirectXCommon::CreateDescriptorHeap(
			D3D12_DESCRIPTOR_HEAP_TYPE type, UINT descriptorCount, bool shaderVisible) {
		D3D12_DESCRIPTOR_HEAP_DESC desc{};
		desc.Type = type;
		desc.NumDescriptors = descriptorCount;
        desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
		HRESULT hr = device_->CreateDescriptorHeap(
			&desc, IID_PPV_ARGS(&descriptorHeap));
		assert(SUCCEEDED(hr));
		return descriptorHeap;
	}

void DirectXCommon::InitializeDescriptorHeaps() {
    descriptorSizeRTV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    descriptorSizeDSV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    rtvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2);
    srvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
    dsvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
}

void DirectXCommon::InitializeViewport() {
    viewport_.Width = static_cast<float>(WinApp::kClientWidth);
    viewport_.Height = static_cast<float>(WinApp::kClientHeight);
    viewport_.MaxDepth = 1.0f;
}

void DirectXCommon::InitializeScissorRect() {
    scissorRect_.right = WinApp::kClientWidth;
    scissorRect_.bottom = WinApp::kClientHeight;
}

void DirectXCommon::InitializeDxc() {
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));
    hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
    assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeImGui() {
#ifdef USE_IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(winApp_->GetHwnd());
    ImGui_ImplDX12_Init(device_.Get(), 2, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        srvDescriptorHeap_.Get(), GetSRVCPUDescriptorHandle(0), GetSRVGPUDescriptorHandle(0));
    ImGui::GetIO().Fonts->Build();
#endif
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(
    ID3D12DescriptorHeap* heap, uint32_t size, uint32_t index) {
    auto handle = heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(size) * index;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(
    ID3D12DescriptorHeap* heap, uint32_t size, uint32_t index) {
    auto handle = heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(size) * index;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index) const {
    assert(index < 128);
    return GetCPUDescriptorHandle(srvDescriptorHeap_.Get(), descriptorSizeSRV_, index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index) const {
    assert(index < 128);
    return GetGPUDescriptorHandle(srvDescriptorHeap_.Get(), descriptorSizeSRV_, index);
}

// 描画前処理: 描画先へ切り替え、色・深度をクリアして描画範囲を設定する。
void DirectXCommon::PreDraw() {
		const UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		commandList_->ResourceBarrier(1, &barrier);

		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
			dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
		commandList_->OMSetRenderTargets(
			1, &rtvHandles_[backBufferIndex], false, &dsvHandle);
		const float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
		commandList_->ClearRenderTargetView(
			rtvHandles_[backBufferIndex], clearColor, 0, nullptr);
		commandList_->ClearDepthStencilView(
			dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		// 描画で使用するSRVヒープを設定する。
        ID3D12DescriptorHeap* heaps[] = { srvDescriptorHeap_.Get() };
        commandList_->SetDescriptorHeaps(1, heaps);
        commandList_->RSSetViewports(1, &viewport_);
		commandList_->RSSetScissorRects(1, &scissorRect_);
	}

// 描画後処理: コマンドをGPUへ送り、表示・完了待ち・次フレームの準備を行う。
void DirectXCommon::PostDraw() {
		const UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		commandList_->ResourceBarrier(1, &barrier);

		HRESULT hr = commandList_->Close();
		assert(SUCCEEDED(hr));
		ID3D12CommandList* commandLists[] = { commandList_.Get() };
		commandQueue_->ExecuteCommandLists(1, commandLists);
		hr = swapChain_->Present(1, 0);
        assert(SUCCEEDED(hr));
		WaitForGPU();
        // VSyncとGPUの完了待ちを含めても短すぎるフレームだけ、追加で待つ。
        UpdateFixFPS();

		hr = commandAllocator_->Reset();
		assert(SUCCEEDED(hr));
		hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
		assert(SUCCEEDED(hr));
	}

// フェンスでGPUの実行完了を待つ。
void DirectXCommon::WaitForGPU() {
		++fenceValue_;
		HRESULT hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
		assert(SUCCEEDED(hr));
		if (fence_->GetCompletedValue() < fenceValue_) {
			hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
			assert(SUCCEEDED(hr));
			WaitForSingleObject(fenceEvent_, INFINITE);
		}
	}

// HLSLをDXCでコンパイルする。
Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(
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

// GPUテクスチャとCPU書き込み用バッファの生成。
Microsoft::WRL::ComPtr<ID3D12Resource>
		DirectXCommon::CreateTextureResource(const DirectX::TexMetadata& metadata) {
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
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(
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

// ファイル読み込みはDirectXの状態に依存しないのでstatic関数にする。
DirectX::ScratchImage DirectXCommon::LoadTexture(const std::string& filePath) {
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

    return mipImages;
}

// 戻り値の中間バッファは、GPU転送完了まで呼び出し側で保持する。
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::UploadTextureData(
    const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
    const DirectX::ScratchImage& mipImages) {
		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		DirectX::PrepareUpload(
			device_.Get(), mipImages.GetImages(), mipImages.GetImageCount(),
			mipImages.GetMetadata(), subresources);
		auto intermediateResource = CreateBufferResource(GetRequiredIntermediateSize(
			texture.Get(), 0, static_cast<UINT>(subresources.size())));
		UpdateSubresources(
			commandList_.Get(), texture.Get(), intermediateResource.Get(),
			0, 0, static_cast<UINT>(subresources.size()), subresources.data());
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = texture.Get();
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
		commandList_->ResourceBarrier(1, &barrier);

    return intermediateResource;
}

void DirectXCommon::Finalize() {
		if (fenceEvent_) {
			CloseHandle(fenceEvent_);
			fenceEvent_ = nullptr;
		}
		fence_.Reset();
		depthStencilResource_.Reset();
		dsvDescriptorHeap_.Reset();
		for (auto& resource : swapChainResources_) {
			resource.Reset();
		}
		rtvDescriptorHeap_.Reset();
		swapChain_.Reset();
		commandList_.Reset();
		commandAllocator_.Reset();
		commandQueue_.Reset();
		srvDescriptorHeap_.Reset();
        includeHandler_.Reset();
        dxcCompiler_.Reset();
        dxcUtils_.Reset();
        device_.Reset();
		adapter_.Reset();
		dxgiFactory_.Reset();
#ifdef _DEBUG
		debugController_.Reset();
#endif
	}

void DirectXCommon::ExecuteCommandListAndWait() {
		HRESULT hr = commandList_->Close();
		assert(SUCCEEDED(hr));
		ID3D12CommandList* commandLists[] = { commandList_.Get() };
		commandQueue_->ExecuteCommandLists(1, commandLists);
		WaitForGPU();
		hr = commandAllocator_->Reset();
		assert(SUCCEEDED(hr));
		hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
		assert(SUCCEEDED(hr));
	}

// 初期化時点を最初のフレームの基準時刻にする。
void DirectXCommon::InitializeFixFPS() {
    reference_ = std::chrono::steady_clock::now();
}

void DirectXCommon::UpdateFixFPS() {
    // 1/60秒。マイクロ秒単位なので約16.67msとなる。
    constexpr std::chrono::microseconds kMinTime(1000000 / 60);
    // 約60HzのVSyncに対して二重に待ちすぎないための判定値。
    constexpr std::chrono::microseconds kMinCheckTime(1000000 / 65);

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);
    if (elapsed < kMinCheckTime) {
        // sleepは要求どおりの精度とは限らないので、起床ごとに時刻を確認する。
        while (std::chrono::steady_clock::now() - reference_ < kMinTime) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }
    // 処理が重いフレームは追加待機せず、現在時刻から次のフレームを計る。
    reference_ = std::chrono::steady_clock::now();
}
