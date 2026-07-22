#pragma once

#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>

// Object3dとSpriteが共通利用するシェーダーと描画パイプラインを管理する。
class Object3dCommon {
public:
	void Initialize(ID3D12Device* device);
	void SetupDraw(ID3D12GraphicsCommandList* commandList) const;

private:
	void CreateRootSignature(ID3D12Device* device);
	void CreateGraphicsPipeline(ID3D12Device* device);
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
		const std::wstring& filePath, const wchar_t* profile);

	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
};
