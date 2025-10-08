#pragma once
#include "D3D12Util.h"
#include "DataTypes.h"
#include "MathUtil.h"
#include <string>
#include <vector>

class Model {
public:
    static Model* Create(
        const std::string& directoryPath, const std::string& filename, ID3D12Device* device);

    void Update();

    // 修正: lightGpuAddress引数を削除
    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const Matrix4x4& viewProjectionMatrix,
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle);

public:
    Transform transform;
    Material* materialData = nullptr;

private:
    void Initialize(
        const std::string& directoryPath, const std::string& filename, ID3D12Device* device);

private:
    std::vector<VertexData> vertices_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;

    Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
    TransformationMatrix* wvpData_ = nullptr;
};