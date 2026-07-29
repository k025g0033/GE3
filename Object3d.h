#pragma once

#include "MathTypes.h"

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <string>

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

// 3Dオブジェクト1個分の形状・姿勢・マテリアル・描画処理を管理するクラス。
class Object3d {
public:
	void Initialize(
		ID3D12Device* device,
		const std::string& directoryPath,
		const std::string& filename);
	void InitializeSphere(
		ID3D12Device* device,
		const std::string& textureFilePath,
		uint32_t subdivision = 16);
	void Update(const Matrix4x4& viewProjectionMatrix);
	void Draw(
		ID3D12GraphicsCommandList* commandList,
		D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
		D3D12_GPU_VIRTUAL_ADDRESS lightAddress) const;
	void Reset();

	Transform& GetTransform() { return transform_; }
	Material& GetMaterial() { return *materialData_; }
	const std::string& GetTextureFilePath()const {
		return textureFilaPath_;
	}

private:
	void CreateModelVertices(
		ID3D12Device* device,
		const std::string& directoryPath,
		const std::string& filename);
	void CreateSphereVertices(
		ID3D12Device* device,
		uint32_t subdivision);
	void CreateObjectResources(ID3D12Device* device);

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

	std::string textureFilaPath_;
};
