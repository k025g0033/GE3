#include "TextureManager.h"

#include "externals/DirectXTex/d3dx12.h"

#include <cassert>

namespace {
std::wstring ToWideString(const std::string& text) {
	if (text.empty()) {
		return {};
	}
	const int length = MultiByteToWideChar(
		CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
		nullptr, 0);
	assert(length > 0);
	std::wstring result(length, L'\0');
	MultiByteToWideChar(
		CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
		result.data(), length);
	return result;
}
}

void TextureManager::Initialize(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList) {
	device_ = device;
	commandList_ = commandList;
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 128;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HRESULT hr = device_->CreateDescriptorHeap(
		&heapDesc, IID_PPV_ARGS(&srvDescriptorHeap_));
	assert(SUCCEEDED(hr));
	descriptorSize_ = device_->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textures_.resize(1); // 0番はImGui用に予約する。
}

TextureManager::TextureHandle TextureManager::Load(
	const std::string& filePath) {
	DirectX::ScratchImage image;
	HRESULT hr = DirectX::LoadFromWICFile(
		ToWideString(filePath).c_str(), DirectX::WIC_FLAGS_FORCE_SRGB,
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

void TextureManager::ReleaseUploadResources() {
	for (auto& texture : textures_) {
		texture.uploadResource.Reset();
	}
}

void TextureManager::Finalize() {
	textures_.clear();
	srvDescriptorHeap_.Reset();
	commandList_ = nullptr;
	device_ = nullptr;
}

void TextureManager::SetDescriptorHeap(
	ID3D12GraphicsCommandList* commandList) const {
	ID3D12DescriptorHeap* heaps[] = {srvDescriptorHeap_.Get()};
	commandList->SetDescriptorHeaps(1, heaps);
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetGPUHandle(
	TextureHandle handle) const {
	assert(handle > 0 && handle < textures_.size());
	return textures_[handle].gpuHandle;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
TextureManager::CreateTextureResource(const DirectX::TexMetadata& metadata) {
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

Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::CreateBuffer(
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
