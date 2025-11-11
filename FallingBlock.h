#pragma once
#include "Model.h"
#include "Player.h"
#include "MapChip.h"
#include <d3d12.h>
#include <wrl.h>

// ブロックの種類
enum class BlockType {
    FallOnly = 3, // 3: 落ちるだけ
    Spike = 4     // 4: 落ちた後、プレイヤーが乗ると上がる
};

// ブロックの状態
enum class BlockState {
    Idle,       // 待機中
    Falling,    // 落下中
    Landed,     // 着地済み
    Rising      // スパイクが上昇中
};

class FallingBlock {
public:
    ~FallingBlock();

    // 初期化 (モデル, 初期Y座標, ブロックタイプ)
    void Initialize(ID3D12Device* device, const Vector3& initialPos, BlockType type);

    // 更新
    void Update(Player* player, MapChip* mapChip);

    // 描画
    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const Matrix4x4& viewProjectionMatrix,
        D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle);

    // プレイヤーが死んだ時にリセット
    // ▼▼▼ ★★★ 修正 ★★★ ▼▼▼
    void Reset(MapChip* mapChip);
    // ▲▲▲ ★★★ 修正 ★★★ ▲▲▲

private:
    // プレイヤーとの衝突判定
    bool CheckCollision(Player* player);

private:
    Model* model_ = nullptr;
    Vector3 initialPos_{};
    BlockType type_ = BlockType::FallOnly;
    BlockState state_ = BlockState::Idle;

    // スパイク (Type 4) が着地したY座標
    float landedY_ = 0.0f;

    // ▼▼▼ ★★★ 修正 ★★★ ▼▼▼
    // 速度 (遅くする)
    const float kFallSpeed_ = 0.05f; // 0.1f から 0.05f に変更
    // ▲▲▲ ★★★ 修正 ★★★ ▲▲▲
    const float kRiseSpeed_ = 0.3f; // 上昇は高速

    // トリガーとなる距離 (Y)
    const float kTriggerDistanceY_ = MapChip::kBlockSize * 1.5f; // 1.5ブロック分

    // ▼▼▼ ★★★ 修正・追加 (ここから) ★★★ ▼▼▼
    // 最後に着地したグリッド座標 (MapChip::data_ を 1 にした場所)
    int lastLandedGridX_ = -1;
    int lastLandedGridMapY_ = -1;
    // ▲▲▲ ★★★ 修正・追加 (ここまで) ★★★ ▲▲▲
};