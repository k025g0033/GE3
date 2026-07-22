#include "Object3d.h"

#include "MathFunctions.h"

#include <cassert>
#include <cmath>

void Object3d::Initialize(ID3D12Device* device) {
	CreateSphereVertices(device);

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

void Object3d::Update(const Matrix4x4& viewProjectionMatrix) {
	transform_.rotate.y += 0.02f;
	const Matrix4x4 worldMatrix = MakeAffineMatrix(
		transform_.scale, transform_.rotate, transform_.translate);
	transformationData_->WVP = Multiply(worldMatrix, viewProjectionMatrix);
	transformationData_->World = worldMatrix;
}

void Object3d::Draw(
	ID3D12GraphicsCommandList* commandList,
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
	D3D12_GPU_VIRTUAL_ADDRESS lightAddress) const {
	commandList->SetGraphicsRootConstantBufferView(
		0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(3, lightAddress);
	commandList->SetGraphicsRootDescriptorTable(2, textureHandle);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(
		1, transformationResource_->GetGPUVirtualAddress());
	commandList->DrawInstanced(kVertexCount, 1, 0, 0);
}

void Object3d::Reset() {
	transform_ = {
		{0.5f, 0.5f, 0.5f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f},
	};
	materialData_->color = {1.0f, 1.0f, 1.0f, 1.0f};
	materialData_->enableLighting = true;
	transformationData_->WVP = MakeIdentity4x4();
	transformationData_->World = MakeIdentity4x4();
}

void Object3d::CreateSphereVertices(ID3D12Device* device) {
	vertexResource_ = CreateBuffer(
		device, sizeof(VertexData) * kVertexCount);
	vertexBufferView_.BufferLocation =
		vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	VertexData* vertices = nullptr;
	HRESULT hr = vertexResource_->Map(
		0, nullptr, reinterpret_cast<void**>(&vertices));
	assert(SUCCEEDED(hr));

	constexpr float pi = 3.1415926535f;
	constexpr float latitudeStep = pi / kSubdivision;
	constexpr float longitudeStep = 2.0f * pi / kSubdivision;
	for (uint32_t latitudeIndex = 0;
		 latitudeIndex < kSubdivision; ++latitudeIndex) {
		const float latitude =
			-pi / 2.0f + latitudeStep * latitudeIndex;
		for (uint32_t longitudeIndex = 0;
			 longitudeIndex < kSubdivision; ++longitudeIndex) {
			const float longitude = longitudeStep * longitudeIndex;
			const uint32_t start =
				(latitudeIndex * kSubdivision + longitudeIndex) * 6;
			const float nextLatitude = latitude + latitudeStep;
			const float nextLongitude = longitude + longitudeStep;

			const Vector4 positions[4] = {
				{std::cos(latitude) * std::cos(longitude),
				 std::sin(latitude),
				 std::cos(latitude) * std::sin(longitude), 1.0f},
				{std::cos(nextLatitude) * std::cos(longitude),
				 std::sin(nextLatitude),
				 std::cos(nextLatitude) * std::sin(longitude), 1.0f},
				{std::cos(latitude) * std::cos(nextLongitude),
				 std::sin(latitude),
				 std::cos(latitude) * std::sin(nextLongitude), 1.0f},
				{std::cos(nextLatitude) * std::cos(nextLongitude),
				 std::sin(nextLatitude),
				 std::cos(nextLatitude) * std::sin(nextLongitude), 1.0f},
			};
			const uint32_t positionIndices[6] = {0, 1, 2, 2, 1, 3};
			const float u = static_cast<float>(longitudeIndex) / kSubdivision;
			const float v = 1.0f -
				static_cast<float>(latitudeIndex) / kSubdivision;
			const float nextU =
				static_cast<float>(longitudeIndex + 1) / kSubdivision;
			const float nextV = 1.0f -
				static_cast<float>(latitudeIndex + 1) / kSubdivision;
			const Vector2 texcoords[4] = {
				{u, v}, {u, nextV}, {nextU, v}, {nextU, nextV}};

			for (uint32_t index = 0; index < 6; ++index) {
				const uint32_t source = positionIndices[index];
				vertices[start + index].position = positions[source];
				vertices[start + index].normal = {
					positions[source].x,
					positions[source].y,
					positions[source].z,
				};
				vertices[start + index].texcoord = texcoords[source];
			}
		}
	}
}

Microsoft::WRL::ComPtr<ID3D12Resource> Object3d::CreateBuffer(
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
