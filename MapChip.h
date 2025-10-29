#pragma once
#include <vector>
#include <string>
#include "Model.h"
#include "MathTypes.h"
#include <d3d12.h> // ID3D12Device を使うために追加

class MapChip {
public:
    // マップチップのサイズ（定数）
    static const float kBlockSize; // (定義は .cpp で)

    // デストラクタ
    ~MapChip();

    void Initialize();

    // 引数に device を追加
    void Load(const std::string& filePath, ID3D12Device* device);

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
    std::vector<Model*> models_;
};