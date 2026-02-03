#pragma once
#include <vector>
#include <string>
#include "Model.h"
#include "MathTypes.h"
#include <d3d12.h> 

// 動的ブロック（罠や落下ブロック）の初期配置情報
struct DynamicBlockData {
    Vector3 position;
    int type;
};

class MapChip {
public:
    static const float kBlockSize;

    ~MapChip();

    void Initialize();

    void Load(const std::string& filePath, ID3D12Device* device);

    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const Matrix4x4& viewProjectionMatrix,
        D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle);

    bool CheckCollision(const Vector3& worldPos);

    size_t GetRowCount() const { return data_.size(); }
    size_t GetColCount() const { return data_.empty() ? 0 : data_[0].size(); }

    const Vector3& GetStartPosition() const { return startPosition_; }

    const std::vector<DynamicBlockData>& GetDynamicBlocks() const { return dynamicBlocks_; }

    bool CheckGoalCollision(const Vector3& playerPos, float playerHalfSize) const;

    const Vector3& GetGoalPosition() const { return goalPos_; }
    bool HasGoal() const { return hasGoal_; }

    // ★追加: ゲーム中にゴールの位置を変更するための関数
    void SetGoalPosition(const Vector3& newPos) { goalPos_ = newPos; }

    void GetGridCoordinates(const Vector3& worldPos, int& outX, int& outMapY) const;

    void SetGridCell(int x, int mapY, int value);

    int GetGridValue(int x, int mapY) const {
        if (x < 0 || mapY < 0 || mapY >= data_.size()) return -1;
        if (x >= data_[mapY].size()) return -1;
        return data_[mapY][x];
    }

    // ★追加: 指定したタイプのブロックが最初に見つかった場所を探す
    bool FindBlock(int type, int& outGridX, int& outMapY) const;

    // ★追加: グリッド座標からワールド座標を計算して返す
    Vector3 GetWorldPosFromGrid(int gridX, int gridMapY) const;

private:
    std::vector<std::vector<int>> data_;
    std::vector<Model*> models_;
    Vector3 startPosition_ = { 0, 0, 0 };
    Vector3 goalPos_ = { 0, 0, 0 };
    bool hasGoal_ = false;
    std::vector<DynamicBlockData> dynamicBlocks_;
};