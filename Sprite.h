#pragma once

#include "MathTypes.h"

#include <d3d12.h>
#include <wrl.h>

// 2Dスプライト1枚分の頂点・姿勢・マテリアル・描画処理を管理するクラス。
class Sprite {
public:
	void Initialize(ID3D12Device* device);
	void Update(float screenWidth, float screenHeight);
	void Draw(
		ID3D12GraphicsCommandList* commandList,
		D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) const;
	void Reset();

	Transform& GetTransform() { return transform_; }
	Material& GetMaterial() { return *materialData_; }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(
		ID3D12Device* device, size_t sizeInBytes);

	Transform transform_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
	TransformationMatrix* transformationData_ = nullptr;

};
