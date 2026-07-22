#pragma once

#include "MathTypes.h"

#include <d3d12.h>
#include <wrl.h>

// 平行光源の設定とGPUへ渡す定数バッファを管理するクラス。
class DirectionalLight {
public:
	void Initialize(ID3D12Device* device);
	void Reset();

	DirectionalLightData& GetData() { return *data_; }
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const {
		return resource_->GetGPUVirtualAddress();
	}

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
	DirectionalLightData* data_ = nullptr;
};
