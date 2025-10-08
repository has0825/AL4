#include "Model.h"
#include "MathUtil.h"
#include "DataTypes.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <Windows.h> // OutputDebugStringA のために追加

// === このファイル内でのみ使用するヘルパー関数 ===
MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
ModelData LoadOjFile(const std::string& directoryPath, const std::string& filename);

Model* Model::Create(
	const std::string& directoryPath, const std::string& filename, ID3D12Device* device) {
	Model* model = new Model();
	model->Initialize(directoryPath, filename, device);
	return model;
}

void Model::Initialize(
	const std::string& directoryPath, const std::string& filename, ID3D12Device* device) {

	ModelData modelData = LoadOjFile(directoryPath, filename);

	// ★修正点: 頂点データがなければ処理を中断
	if (modelData.vertices.empty()) {
		return;
	}
	vertices_ = modelData.vertices;

	vertexResource_ = CreateBufferResource(device, sizeof(VertexData) * vertices_.size());
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * vertices_.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	VertexData* vertexData = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, vertices_.data(), sizeof(VertexData) * vertices_.size());

	materialResource_ = CreateBufferResource(device, sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->enableLighting = true;
	materialData->uvTransform = MakeIdentity4x4();

	wvpResource_ = CreateBufferResource(device, sizeof(TransformationMatrix));
	wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
	wvpData_->WVP = MakeIdentity4x4();
	wvpData_->World = MakeIdentity4x4();
}

void Model::Update() {
	// 将来的なアニメーション更新などで使用
}

void Model::Draw(
	ID3D12GraphicsCommandList* commandList,
	const Matrix4x4& viewProjectionMatrix,
	D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {

	// ★修正点: 頂点データがなければ描画しない
	if (vertices_.empty()) {
		return;
	}

	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	wvpData_->WVP = Multiply(worldMatrix, viewProjectionMatrix);
	wvpData_->World = worldMatrix;

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);
	commandList->SetGraphicsRootConstantBufferView(3, lightGpuAddress);

	commandList->DrawInstanced(UINT(vertices_.size()), 1, 0, 0);
}


// === このファイル内でのみ使用するヘルパー関数 ===
MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
	MaterialData materialData;
	std::string line;
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}

ModelData LoadOjFile(const std::string& directoryPath, const std::string& filename)
{
	ModelData modelData;
	std::vector<Vector4> positions;
	std::vector<Vector3> normals;
	std::vector<Vector2> texcoords;
	std::string line;
	std::string fullpath = directoryPath + "/" + filename;
	std::ifstream file(fullpath);

	// ★修正点: ファイルが開けなかった場合に警告を出す
	if (!file.is_open()) {
		std::string message = "Error: Cannot open model file: " + fullpath + "\n";
		OutputDebugStringA(message.c_str());
		assert(false);
		return modelData; // 空のモデルデータを返す
	}

	while (std::getline(file, line)) {
		std::string identifiler;
		std::istringstream s(line);
		s >> identifiler;
		if (identifiler == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.x *= -1.0f;
			position.w = 1.0f;
			positions.push_back(position);
		} else if (identifiler == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifiler == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (identifiler == "f") {
			VertexData triangle[3];
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				triangle[faceVertex] = { position, texcoord, normal };
			}
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		} else if (identifiler == "mtllib") {
			std::string materialFilename;
			s >> materialFilename;
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}
	return modelData;
}