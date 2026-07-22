#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <string>
#include <vector>
#include "externals/DirectXTex/DirectXTex.h"

class TextureManager {
public:
	using TextureHandle = uint32_t;

	void Initialize(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList);
	TextureHandle Load(const std::string& filePath);
	void ReleaseUploadResources();
	void Finalize();
	void SetDescriptorHeap(ID3D12GraphicsCommandList* commandList) const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(TextureHandle handle) const;
	ID3D12DescriptorHeap* GetDescriptorHeap() const {
		return srvDescriptorHeap_.Get();
	}

private:
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
};
