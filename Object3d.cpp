#include "Object3d.h"

#include "MathFunctions.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstring>

namespace {
std::unordered_map<std::string, MaterialData> LoadMaterialTemplateFiles(
	const std::string& directoryPath,
	const std::string& filename) {
	std::unordered_map<std::string, MaterialData> materials;
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	MaterialData* currentMaterial = nullptr;
	std::string line;
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "newmtl") {
			std::string materialName;
			s >> materialName;
			MaterialData& material = materials[materialName];
			material.name = materialName;
			currentMaterial = &material;
		} else if (identifier == "map_Kd" && currentMaterial) {
			std::string textureFilename;
			s >> textureFilename;
			currentMaterial->textureFilePath =
				directoryPath + "/" + textureFilename;
		}
	}
	return materials;
}
}

ModelData LoadObjFile(
	const std::string& directoryPath,
	const std::string& filename) {

	ModelData modelData;
	std::vector<Vector4> positions;
	std::vector<Vector3> normals;
	std::vector<Vector2> texcoords;
	std::unordered_map<std::string, MaterialData> materials;
	std::string currentMaterialName;
	std::string line;

	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.x *= -1.0f;
			position.w = 1.0f;
			positions.push_back(position);

		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);

		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z; 
			normal.x *= -1.0f;
			normals.push_back(normal);

		} else if (identifier == "f") {
			if (modelData.subMeshes.empty() ||
				modelData.subMeshes.back().material.name !=
					currentMaterialName) {
				SubMeshData subMesh{};
				subMesh.vertexStart =
					static_cast<uint32_t>(modelData.vertices.size());
				auto materialIt = materials.find(currentMaterialName);
				if (materialIt != materials.end()) {
					subMesh.material = materialIt->second;
				} else {
					subMesh.material.name = currentMaterialName;
				}
				modelData.subMeshes.push_back(subMesh);
			}
			VertexData triangle[3];
			for (uint32_t faceVertex = 0;
				faceVertex < 3;
				++faceVertex) {

				std::string vertexDefinition;
				s >> vertexDefinition;

				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];

				for (uint32_t element = 0;
					element < 3;
					++element) {

					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				const Vector4 position =
					positions[elementIndices[0] - 1];
				const Vector2 texcoord =
					texcoords[elementIndices[1] - 1];
				const Vector3 normal =
					normals[elementIndices[2] - 1];

				triangle[faceVertex] = {
					position,
					texcoord,
					normal,
				};

			}
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
			modelData.subMeshes.back().vertexCount += 3;
		} else if (identifier == "mtllib") {
			// materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			materials =
				LoadMaterialTemplateFiles(directoryPath, materialFilename);
		} else if (identifier == "usemtl") {
			s >> currentMaterialName;
		}
	}

	if (!modelData.subMeshes.empty()) {
		modelData.material = modelData.subMeshes.front().material;
	}
	return modelData;
}

MaterialData LoadMaterialTemplateFile(
	const std::string& directoryPath,
	const std::string& filename) {
	MaterialData materialData; // 構造するMaterialData
	std::string line; // ファイルから読み込んだ1行を格納するもの
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず開けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identifierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			// 連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	return materialData;
}

void Object3d::Initialize(
	ID3D12Device* device,
	const std::string& directoryPath,
	const std::string& filename) {
	CreateModelVertices(device, directoryPath, filename);
	CreateObjectResources(device);
}

void Object3d::InitializeSphere(
	ID3D12Device* device,
	const std::string& textureFilePath,
	uint32_t subdivision) {
	assert(subdivision >= 3);
	CreateSphereVertices(device, subdivision);
	textureFilaPath_ = textureFilePath;
	CreateObjectResources(device);
}

void Object3d::CreateObjectResources(ID3D12Device* device) {
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
	commandList->DrawInstanced(vertexCount_, 1, 0, 0);
}

void Object3d::Draw(
	ID3D12GraphicsCommandList* commandList,
	const std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>& textureHandles,
	D3D12_GPU_VIRTUAL_ADDRESS lightAddress) const {
	assert(textureHandles.size() == subMeshes_.size());
	commandList->SetGraphicsRootConstantBufferView(
		0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(3, lightAddress);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(
		1, transformationResource_->GetGPUVirtualAddress());

	for (size_t index = 0; index < subMeshes_.size(); ++index) {
		commandList->SetGraphicsRootDescriptorTable(
			2, textureHandles[index]);
		commandList->DrawInstanced(
			subMeshes_[index].vertexCount,
			1,
			subMeshes_[index].vertexStart,
			0);
	}
}

void Object3d::Reset() {
	transform_ = {
		{0.5f, 0.5f, 0.5f},
		{0.0f, 3.14159265f, 0.0f},
		{0.0f, 0.0f, 0.0f},
	};
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = true;
	materialData_->lightingMode = 1;
	materialData_->uvTransform = MakeIdentity4x4();
	transformationData_->WVP = MakeIdentity4x4();
	transformationData_->World = MakeIdentity4x4();
}

void Object3d::CreateModelVertices(
	ID3D12Device* device,
	const std::string& directoryPath,
	const std::string& filename) {
	// OBJファイルから頂点情報を読み込む
	const ModelData modelData =
		LoadObjFile(directoryPath, filename);
	textureFilaPath_ = modelData.material.textureFilePath;
	subMeshes_ = modelData.subMeshes;
	textureFilePaths_.clear();
	for (const SubMeshData& subMesh : subMeshes_) {
		textureFilePaths_.push_back(
			subMesh.material.textureFilePath);
	}
	// 読み込んだ頂点数を保存する
	vertexCount_ =
		static_cast<uint32_t>(modelData.vertices.size());

	// 頂点バッファを作成する
	const size_t vertexBufferSize =
		sizeof(VertexData) * modelData.vertices.size();

	vertexResource_ =
		CreateBuffer(device, vertexBufferSize);

	// 頂点バッファビューを設定する
	vertexBufferView_.BufferLocation =
		vertexResource_->GetGPUVirtualAddress();

	vertexBufferView_.SizeInBytes =
		static_cast<UINT>(vertexBufferSize);

	vertexBufferView_.StrideInBytes =
		sizeof(VertexData);

	// 頂点バッファへモデルの頂点データをコピーする
	VertexData* vertexData = nullptr;

	const HRESULT hr = vertexResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&vertexData));

	assert(SUCCEEDED(hr));

	std::memcpy(
		vertexData,
		modelData.vertices.data(),
		vertexBufferSize);
}

void Object3d::CreateSphereVertices(
	ID3D12Device* device,
	uint32_t subdivision) {
	constexpr float pi = 3.14159265358979323846f;
	const float latitudeStep = pi / static_cast<float>(subdivision);
	const float longitudeStep =
		2.0f * pi / static_cast<float>(subdivision);

	std::vector<VertexData> vertices;
	vertices.reserve(
		static_cast<size_t>(subdivision) * subdivision * 6);

	auto makeVertex = [](float latitude, float longitude,
		float u, float v) {
		const Vector4 position{
			std::cos(latitude) * std::cos(longitude),
			std::sin(latitude),
			std::cos(latitude) * std::sin(longitude),
			1.0f,
		};
		return VertexData{
			position,
			{u, v},
			{position.x, position.y, position.z},
		};
	};

	for (uint32_t latitudeIndex = 0;
		latitudeIndex < subdivision;
		++latitudeIndex) {
		const float latitude =
			-pi / 2.0f + latitudeStep * latitudeIndex;
		const float nextLatitude = latitude + latitudeStep;
		const float v =
			1.0f - static_cast<float>(latitudeIndex) / subdivision;
		const float nextV =
			1.0f - static_cast<float>(latitudeIndex + 1) / subdivision;

		for (uint32_t longitudeIndex = 0;
			longitudeIndex < subdivision;
			++longitudeIndex) {
			const float longitude =
				longitudeStep * longitudeIndex;
			const float nextLongitude = longitude + longitudeStep;
			const float u =
				static_cast<float>(longitudeIndex) / subdivision;
			const float nextU =
				static_cast<float>(longitudeIndex + 1) / subdivision;

			const VertexData lowerLeft =
				makeVertex(latitude, longitude, u, v);
			const VertexData upperLeft =
				makeVertex(nextLatitude, longitude, u, nextV);
			const VertexData lowerRight =
				makeVertex(latitude, nextLongitude, nextU, v);
			const VertexData upperRight =
				makeVertex(nextLatitude, nextLongitude, nextU, nextV);

			vertices.push_back(lowerLeft);
			vertices.push_back(upperLeft);
			vertices.push_back(lowerRight);
			vertices.push_back(lowerRight);
			vertices.push_back(upperLeft);
			vertices.push_back(upperRight);
		}
	}

	vertexCount_ = static_cast<uint32_t>(vertices.size());
	const size_t vertexBufferSize =
		sizeof(VertexData) * vertices.size();
	vertexResource_ = CreateBuffer(device, vertexBufferSize);
	vertexBufferView_.BufferLocation =
		vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes =
		static_cast<UINT>(vertexBufferSize);
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	VertexData* vertexData = nullptr;
	const HRESULT hr = vertexResource_->Map(
		0, nullptr, reinterpret_cast<void**>(&vertexData));
	assert(SUCCEEDED(hr));
	std::memcpy(vertexData, vertices.data(), vertexBufferSize);
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
