#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <cstdint>

// DirectX 12を使うための基盤となるオブジェクトを管理するクラス。
class DirectXCommon {
public:
	~DirectXCommon();

	void Initialize(HWND hwnd, int32_t clientWidth, int32_t clientHeight);
	void Finalize();
	void BeginFrame();
	void EndFrame();
	void ExecuteCommandListAndWait();

	ID3D12Device* GetDevice() const { return device_.Get(); }
	IDXGIFactory7* GetDxgiFactory() const { return dxgiFactory_.Get(); }
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

private:
	void CreateDevice();
	void CreateCommandObjects();
	void CreateSwapChain(HWND hwnd, int32_t clientWidth, int32_t clientHeight);
	void CreateRenderTargets();
	void CreateDepthStencil(int32_t clientWidth, int32_t clientHeight);
	void CreateFence();
	void SetupDebugInfoQueue();
	void WaitForGPU();
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE type, UINT descriptorCount);

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
