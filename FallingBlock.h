#pragma once
#include "Model.h"
#include "Player.h"
#include "MapChip.h"
#include <d3d12.h>
#include <wrl.h>

// ブロックの種類
enum class BlockType {
    FallOnly = 3,       // 3: プレイヤーが下に来ると落ちる
    Spike = 4,          // 4: 落ちた後、乗ると上がる
    RiseOnTop = 6,      // 6: プレイヤーが真上にいると上がる
    SideAttack = 7,     // 7: プレイヤーが近づくと横に飛ぶ
    FallOnTop = 8,      // 8: プレイヤーが真上にいると落ちる
    StaticHazard = 9,   // 9: [追加] 動かないが、触れると即死するトラップ
    RiseThenFall = 10   // 10: [追加] 上に乗ると上昇し、天井で止まり、下に人が来ると落ちる
};

// ブロックの状態
enum class BlockState {
    Idle,           // 待機中
    Falling,        // 落下中
    Landed,         // 着地（または天井到達）済み待機
    Rising,         // 上昇中
    MovingSide      // 横移動中
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

    // パラメータ
    const float kFallSpeed_ = 0.2f;
    const float kRiseSpeed_ = 0.2f;
    const float kSideSpeed_ = 0.3f;

    // 着地情報
    float landedY_ = 0.0f;
    int lastLandedGridX_ = -1;
    int lastLandedGridMapY_ = -1;

    // 横移動用
    float moveDirX_ = 0.0f;

    // Type 10用: 天井に張り付いているかどうかのフラグ
    bool isCeiling_ = false;
};