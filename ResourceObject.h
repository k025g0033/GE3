#pragma once
#include <d3d12.h>
#include <wrl.h>

class ResourceObject{
public:
	ResourceObject(ID3D12Resource* resource)
		:resource_(resource)
	{}
	// デストラクタはオブジェクトの寿命が尽きたときに呼ばれる
	~ResourceObject(){
		if (resource_) {
			resource_->Release();
		}
	}
	ID3D12Resource* Get() { return resource_; }
private:
	ID3D12Resource* resource_;
};

