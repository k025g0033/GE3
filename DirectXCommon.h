#pragma once
#include "WinApp.h"
#include <dxcapi.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <cstdint>

// 終了処理と初期テクスチャ転送は、次の移植段階までmain.cppに残す。
namespace LegacyDirectXCommon {
    void Finalize();

    void ExecuteCommandListAndWait();

}

class DirectXCommon {
public:
    // DirectX基盤を生成し、描画に必要なデータを準備する。
    void Initialize(WinApp* winApp);
    // シーン描画の前後に毎フレーム呼び出す。
    void PreDraw();
    void PostDraw();
	ID3D12Device* GetDevice() const { return device_.Get(); }
	IDXGIFactory7* GetDxgiFactory() const { return dxgiFactory_.Get(); }
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

    // 0番のSRVはImGui用。テクスチャは1番以降を使う。
    ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return srvDescriptorHeap_.Get(); }
    IDxcUtils* GetDxcUtils() const { return dxcUtils_.Get(); }
    IDxcCompiler3* GetDxcCompiler() const { return dxcCompiler_.Get(); }
    IDxcIncludeHandler* GetIncludeHandler() const { return includeHandler_.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index) const;

private:
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

    // 移植途中の既存処理だけに、同じ描画データへのアクセスを許可する。
    friend void LegacyDirectXCommon::Finalize();

    friend void LegacyDirectXCommon::ExecuteCommandListAndWait();


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
