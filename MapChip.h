#pragma once
#include <vector>
#include <string>
#include "Model.h"
#include "MathTypes.h"

class MapChip {
public:
    // マップチップのサイズ（定数）
    static const float kBlockSize;

    void Initialize();
    void Load(const std::string& filePath);
    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const Matrix4x4& viewProjectionMatrix,
        D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle);

    // 指定されたワールド座標がブロックと衝突しているか判定する
    bool CheckCollision(const Vector3& worldPos);

    // マップの行数を取得
    size_t GetRowCount() const { return data_.size(); }
    // マップの列数を取得
    size_t GetColCount() const { return data_.empty() ? 0 : data_[0].size(); }


private:
    std::vector<std::vector<int>> data_;
    Model* model_ = nullptr;
};