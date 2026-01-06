#pragma once
#include "Model.h"
#include "Player.h"
#include "MapChip.h"
#include <d3d12.h>
#include <wrl.h>

// ブロックの種類
enum class BlockType {
    FallOnly = 3,     // 3: プレイヤーが下に来ると落ちる
    Spike = 4,        // 4: 落ちた後、乗ると上がる
    RiseOnTop = 6,    // 6: [新] プレイヤーが真上にいると上がる
    SideAttack = 7,   // 7: [新] プレイヤーが近づくと横に飛ぶ
    FallOnTop = 8     // 8: [新] プレイヤーが真上にいると落ちる
};

// ブロックの状態
enum class BlockState {
    Idle,           // 待機中
    Falling,        // 落下中
    Landed,         // 着地済み
    Rising,         // 上昇中
    MovingSide      // [新] 横移動中
};

class FallingBlock {
public:
    ~FallingBlock();

    void Initialize(ID3D12Device* device, const Vector3& initialPos, BlockType type);
    void Update(Player* player, MapChip* mapChip);
    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const Matrix4x4& viewProjectionMatrix,
        D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle);
    void Reset(MapChip* mapChip);

private:
    bool CheckCollision(Player* player);

private:
    Model* model_ = nullptr;
    Vector3 initialPos_{};
    BlockType type_ = BlockType::FallOnly;
    BlockState state_ = BlockState::Idle;

    // 落下速度等のパラメータ
    const float kFallSpeed_ = 0.2f;
    const float kRiseSpeed_ = 0.1f;
    const float kSideSpeed_ = 0.3f; // 横移動の速度

    // 着地情報
    float landedY_ = 0.0f;
    int lastLandedGridX_ = -1;
    int lastLandedGridMapY_ = -1;

    // Type 7用: 移動方向 (1:右, -1:左)
    float moveDirX_ = 0.0f;
};