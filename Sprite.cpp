#include "Sprite.h"

#include "MathFunctions.h"

#include <cassert>

void Sprite::Initialize(ID3D12Device* device) {
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

void Sprite::Update(float screenWidth, float screenHeight) {
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

void Sprite::Draw(
	ID3D12GraphicsCommandList* commandList,
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) const {
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

void Sprite::Reset() {
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
	materialData_->lightingMode = 0;
	materialData_->uvTransform = MakeIdentity4x4();
	transformationData_->WVP = MakeIdentity4x4();
	transformationData_->World = MakeIdentity4x4();
}

Microsoft::WRL::ComPtr<ID3D12Resource> Sprite::CreateBuffer(
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
