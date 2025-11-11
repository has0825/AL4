#pragma once
#include <vector>
#include <string>
#include "Model.h"
#include "MathTypes.h"
#include <d3d12.h> // ID3D12Device を使うために追加

// 3(落ちる) 4(スパイク) のトリガー情報を格納する構造体
struct DynamicBlockData {
    Vector3 position; // 3, 4 が書かれていたセルの中心ワールド座標
    int type; // 3 or 4
};

class MapChip {
public:
    // マップチップのサイズ（定数）
    static const float kBlockSize; // (定義は .cpp で 0.7f に設定)

    // デストラクタ
    ~MapChip();

    void Initialize(); // 中身は空

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

    // プレイヤーの初期位置を取得するゲッター
    const Vector3& GetStartPosition() const { return startPosition_; }

    // 読み込んだ動的ブロックのリストを取得
    const std::vector<DynamicBlockData>& GetDynamicBlocks() const { return dynamicBlocks_; }

    // ゴールとの当たり判定
    bool CheckGoalCollision(const Vector3& playerPos, float playerHalfSize) const;

    // ゴールの位置を取得するゲッター
    const Vector3& GetGoalPosition() const { return goalPos_; }
    bool HasGoal() const { return hasGoal_; }

    // --- ▼▼▼ ★★★ 修正・追加 (ここから) ★★★ ---
    // ワールド座標からグリッド座標 (x, mapY) を取得 (失敗したら -1)
    void GetGridCoordinates(const Vector3& worldPos, int& outX, int& outMapY) const;

    // グリッド座標 (x, mapY) の data_ の値を書き換える
    void SetGridCell(int x, int mapY, int value);
    // --- ▲▲▲ ★★★ 修正・追加 (ここまで) ★★★ ---

private:
    std::vector<std::vector<int>> data_;
    std::vector<Model*> models_;
    // プレイヤーの初期位置を格納するメンバ変数
    Vector3 startPosition_{ 0.0f, 0.0f, 0.0f }; // デフォルト値

    // 読み込んだ動的ブロック情報 (3, 4)
    std::vector<DynamicBlockData> dynamicBlocks_;

    // ゴール(5)の座標
    Vector3 goalPos_{};
    bool hasGoal_ = false; // ゴールがマップにあるか
};