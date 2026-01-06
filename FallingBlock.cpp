#include "FallingBlock.h"
#include <cmath> // std::abs
#include "DirectXCommon.h" 

FallingBlock::~FallingBlock() {
    delete model_;
}

void FallingBlock::Initialize(ID3D12Device* device, const Vector3& initialPos, BlockType type) {
    // モデル読み込み
    model_ = Model::Create("Resources/block", "block.obj", device);
    initialPos_ = initialPos;
    type_ = type;

    // サイズ設定
    model_->transform.scale = { MapChip::kBlockSize, MapChip::kBlockSize, MapChip::kBlockSize };

    // 初期化
    state_ = BlockState::Idle;
    model_->transform.translate = initialPos_;
    landedY_ = initialPos_.y;

    // -1 は「マップに壁として登録していない」状態を表す
    lastLandedGridX_ = -1;
    lastLandedGridMapY_ = -1;
    moveDirX_ = 0.0f;
}

void FallingBlock::Reset(MapChip* mapChip) {
    // もしマップに壁(1)として登録したままなら解除する
    if (lastLandedGridX_ != -1 && lastLandedGridMapY_ != -1) {
        mapChip->SetGridCell(lastLandedGridX_, lastLandedGridMapY_, 0);
    }

    // 状態リセット
    model_->transform.translate = initialPos_;
    state_ = BlockState::Idle;
    landedY_ = initialPos_.y;
    lastLandedGridX_ = -1;
    lastLandedGridMapY_ = -1;
    moveDirX_ = 0.0f;
}

void FallingBlock::Update(Player* player, MapChip* mapChip) {
    if (!player->IsAlive()) {
        return;
    }

    const Vector3& playerPos = player->GetPosition();
    Vector3& blockPos = model_->transform.translate;

    // 距離計算用
    float dx = std::abs(playerPos.x - blockPos.x);
    float dy = playerPos.y - blockPos.y;
    float distSq = (playerPos.x - blockPos.x) * (playerPos.x - blockPos.x) +
        (playerPos.y - blockPos.y) * (playerPos.y - blockPos.y);

    float halfSize = MapChip::kBlockSize / 2.0f;

    // === ステートマシン ===
    switch (state_) {

    case BlockState::Idle:
    {
        // ★ 待機中はマップに「壁」として登録し、足場にする
        if (lastLandedGridX_ == -1) {
            int gridX, gridMapY;
            mapChip->GetGridCoordinates(blockPos, gridX, gridMapY);
            if (gridX != -1) {
                mapChip->SetGridCell(gridX, gridMapY, 1); // 壁にする
                lastLandedGridX_ = gridX;
                lastLandedGridMapY_ = gridMapY;
            }
        }

        // トリガー判定
        bool isAlignX = (dx < halfSize);
        bool shouldAct = false;

        // --- Type 3 & 4 (落下系) ---
        if (type_ == BlockType::FallOnly || type_ == BlockType::Spike) {
            float kSearchRange = MapChip::kBlockSize * 5.0f;
            if (isAlignX && dy < 0 && dy > -kSearchRange) {
                state_ = BlockState::Falling;
                shouldAct = true;
            }
        }
        // --- Type 6 (真上にいると上昇) ---
        else if (type_ == BlockType::RiseOnTop) {
            if (isAlignX && dy > 0) {
                state_ = BlockState::Rising;
                shouldAct = true;
            }
        }
        // --- Type 7 (横移動) ---
        else if (type_ == BlockType::SideAttack) {
            float kTriggerRadius = MapChip::kBlockSize * 6.0f;
            if (distSq < kTriggerRadius * kTriggerRadius) {
                state_ = BlockState::MovingSide;
                shouldAct = true;
                moveDirX_ = (playerPos.x > blockPos.x) ? 1.0f : -1.0f;
            }
        }
        // --- Type 8 (真上にいると落下) ---
        else if (type_ == BlockType::FallOnTop) {
            if (isAlignX && dy > 0) {
                state_ = BlockState::Falling;
                shouldAct = true;
            }
        }

        // 動き出すなら、登録していた壁を消す
        if (shouldAct && lastLandedGridX_ != -1) {
            mapChip->SetGridCell(lastLandedGridX_, lastLandedGridMapY_, 0); // 空に戻す
            lastLandedGridX_ = -1;
            lastLandedGridMapY_ = -1;
        }
    }
    break;

    case BlockState::Falling:
    {
        // 落下中は危険判定 (接触＝死)
        if (CheckCollision(player)) {
            player->Die();
        }

        blockPos.y -= kFallSpeed_;

        // 地面衝突判定
        Vector3 footPos = { blockPos.x, blockPos.y - halfSize - 0.001f, 0.0f };
        if (mapChip->CheckCollision(footPos)) {
            // 着地処理
            state_ = BlockState::Landed;
            blockPos.y = floor(footPos.y / MapChip::kBlockSize) * MapChip::kBlockSize + MapChip::kBlockSize + halfSize;
            landedY_ = blockPos.y;

            // ★ 着地したら即座にマップへ「壁」として登録（足場になる）
            int gridX, gridMapY;
            mapChip->GetGridCoordinates(blockPos, gridX, gridMapY);
            if (gridX != -1) {
                mapChip->SetGridCell(gridX, gridMapY, 1);
                lastLandedGridX_ = gridX;
                lastLandedGridMapY_ = gridMapY;
            }
        }
    }
    break;

    case BlockState::Landed:
    {
        // 着地済み＝壁として登録済みなので、プレイヤーは乗れる。
        // ここでの CheckCollision -> Die は行わない。

        // --- Type 4 (Spike) の再上昇判定 ---
        if (type_ == BlockType::Spike) {
            bool isPlayerAbove = (dx < halfSize) && (playerPos.y > blockPos.y);

            // プレイヤーが上に乗ったら上昇開始
            if (isPlayerAbove) {
                state_ = BlockState::Rising;

                // 動き出すので壁登録を解除
                if (lastLandedGridX_ != -1) {
                    mapChip->SetGridCell(lastLandedGridX_, lastLandedGridMapY_, 0);
                    lastLandedGridX_ = -1;
                    lastLandedGridMapY_ = -1;
                }
            }
        }
    }
    break;

    case BlockState::Rising:
    {
        // 上昇中 (上がっていくブロック) は危険判定
        if (CheckCollision(player)) {
            player->Die();
        }

        // 画面外へ消えるまで上昇し続ける
        blockPos.y += kRiseSpeed_;
    }
    break;

    case BlockState::MovingSide:
    {
        // 横移動中も危険判定
        if (CheckCollision(player)) {
            player->Die();
        }
        blockPos.x += kSideSpeed_ * moveDirX_;
    }
    break;
    }

    model_->transform = model_->transform;
}

void FallingBlock::Draw(
    ID3D12GraphicsCommandList* commandList,
    const Matrix4x4& viewProjectionMatrix,
    D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {

    model_->Draw(commandList, viewProjectionMatrix, lightGpuAddress, textureSrvHandle);
}

bool FallingBlock::CheckCollision(Player* player) {
    Vector3 pPos = player->GetPosition();
    float pSize = player->GetHalfSize();
    float pLeft = pPos.x - pSize;
    float pRight = pPos.x + pSize;
    float pTop = pPos.y + pSize;
    float pBottom = pPos.y - pSize;

    Vector3 wPos = model_->transform.translate;
    float halfSize = MapChip::kBlockSize / 2.0f;
    float wLeft = wPos.x - halfSize;
    float wRight = wPos.x + halfSize;
    float wTop = wPos.y + halfSize;
    float wBottom = wPos.y - halfSize;

    if (pLeft > wRight || pRight < wLeft || pTop < wBottom || pBottom > wTop) {
        return false;
    }
    return true;
}