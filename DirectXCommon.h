#pragma once
#include "WinApp.h"
#include <dxcapi.h>

#include <string>
#include <d3d12.h>
#include "externals/DirectXTex/DirectXTex.h"
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <cstdint>
#include <chrono>



class DirectXCommon {
public:
    // DirectX基盤を生成し、描画に必要なデータを準備する。
    void Initialize(WinApp* winApp);
    // シーン描画の前後に毎フレーム呼び出す。
    void PreDraw();
    void PostDraw();
    // 初期転送コマンドの実行と終了時の解放。
    void ExecuteCommandListAndWait();
    void Finalize();
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const wchar_t* profile);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);
    // 中間バッファの寿命はGPU転送完了まで維持する。
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(
        const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages);
    static DirectX::ScratchImage LoadTexture(const std::string& filePath);

	ID3D12Device* GetDevice() const { return device_.Get(); }
	IDXGIFactory7* GetDxgiFactory() const { return dxgiFactory_.Get(); }
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

    // 0番のSRVはImGui用。テクスチャは1番以降を使う。
    ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return srvDescriptorHeap_.Get(); }



    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index) const;

private:
    // FPS固定の初期化と、毎フレームの待機。
    void InitializeFixFPS();
    void UpdateFixFPS();
    std::chrono::steady_clock::time_point reference_{};

    void WaitForGPU();
    void InitializeDescriptorHeaps();
    void InitializeViewport();
    void InitializeScissorRect();
    void InitializeDxc();
    void InitializeImGui();
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
        ID3D12DescriptorHeap* heap, uint32_t size, uint32_t index);
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
        ID3D12DescriptorHeap* heap, uint32_t size, uint32_t index);
    void CreateDevice();
    void SetupDebugInfoQueue();
    void CreateCommandObjects();
    void CreateSwapChain(HWND hwnd, int32_t clientWidth, int32_t clientHeight);
    void CreateRenderTargets();
    void CreateDepthStencil(int32_t clientWidth, int32_t clientHeight);
    void CreateFence();
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE type, UINT descriptorCount, bool shaderVisible = false);


    WinApp* winApp_ = nullptr; // 所有権は持たない
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    uint32_t descriptorSizeRTV_ = 0;
    uint32_t descriptorSizeSRV_ = 0;
    uint32_t descriptorSizeDSV_ = 0;
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;

    // DirectX基盤のデータ
#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController_;
#endif
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter_;
	Microsoft::WRL::ComPtr<ID3D12Device> device_;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources_;
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 2> rtvHandles_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT scissorRect_{};

};
