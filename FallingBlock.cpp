#include "FallingBlock.h"
#include <cmath> // std::abs
#include "DirectXCommon.h" 

FallingBlock::~FallingBlock() {
    delete model_;
}

void FallingBlock::Initialize(ID3D12Device* device, const Vector3& initialPos, BlockType type) {
    // 1 と同じ "block.obj" を読み込む
    model_ = Model::Create("Resources/block", "block.obj", device);
    initialPos_ = initialPos;
    type_ = type;

    // kBlockSize をモデルのスケールに設定
    model_->transform.scale = { MapChip::kBlockSize, MapChip::kBlockSize, MapChip::kBlockSize };

    // Reset() は main で呼ばれる (mapChip が必要なため)
    // ただし、初期化はしておく
    state_ = BlockState::Idle;
    model_->transform.translate = initialPos_;
    landedY_ = initialPos_.y;
    lastLandedGridX_ = -1;
    lastLandedGridMapY_ = -1;
}

void FallingBlock::Reset(MapChip* mapChip) {

    // ▼▼▼ ★★★ 修正・追加 (ここから) ★★★ ▼▼▼
    // もし既に着地してマップ(data_)を 1 に変更していたら
    if (lastLandedGridX_ != -1 && lastLandedGridMapY_ != -1) {
        // マップを元の状態 (3 or 4) に戻す
        mapChip->SetGridCell(lastLandedGridX_, lastLandedGridMapY_, (int)type_);
    }
    // ▲▲▲ ★★★ 修正・追加 (ここまで) ★★★ ▲▲▲

    // 位置を初期位置に戻し、待機状態にする
    model_->transform.translate = initialPos_;
    state_ = BlockState::Idle;
    landedY_ = initialPos_.y; // 着地Y座標もリセット
    lastLandedGridX_ = -1;
    lastLandedGridMapY_ = -1;
}

void FallingBlock::Update(Player* player, MapChip* mapChip) {
    // プレイヤーが死んでいたら更新しない
    if (!player->IsAlive()) {
        return;
    }

    const Vector3& playerPos = player->GetPosition();
    Vector3& blockPos = model_->transform.translate;

    // プレイヤーとブロックのX座標の距離
    float dx = std::abs(playerPos.x - blockPos.x);
    // プレイヤーとブロックのY座標の距離
    float dy = playerPos.y - blockPos.y; // (プレイヤーが下ならマイナス)

    // ブロックの当たり判定サイズ
    float halfSize = MapChip::kBlockSize / 2.0f;

    // === ステートマシン ===
    switch (state_) {

    case BlockState::Idle:
    {
        // 待機中
        // ▼▼▼ ★★★ 修正 ★★★ ▼▼▼
        // トリガーを厳しく (ブロックの幅の 1/4 = 0.25f 倍)
        bool isJustUnder = (dx < (halfSize * 0.5f));
        // ▲▲▲ ★★★ 修正 ★★★ ▲▲▲

        // プレイヤーがブロックの *真下* (Xが近い) かつ、
        // プレイヤーがブロックより一定距離 (kTriggerDistanceY_) 下にいたら
        if (isJustUnder && dy < -kTriggerDistanceY_) {
            state_ = BlockState::Falling;
        }
    }
    break;

    case BlockState::Falling:
    {
        // 落下中
        blockPos.y -= kFallSpeed_;

        // 1. プレイヤーに当たったら死亡
        if (CheckCollision(player)) {
            player->Die();
        }

        // 2. 地面 (MapChip 1) に当たったかチェック
        // ブロックの足元座標
        Vector3 footPos = { blockPos.x, blockPos.y - halfSize - 0.001f, 0.0f };

        if (mapChip->CheckCollision(footPos)) {
            // 地面 (1) に着地
            state_ = BlockState::Landed;
            // Y座標を地面にスナップ
            blockPos.y = floor(footPos.y / MapChip::kBlockSize) * MapChip::kBlockSize + MapChip::kBlockSize + halfSize;
            landedY_ = blockPos.y; // 着地Y座標を記憶

            // ▼▼▼ ★★★ 修正・追加 (ここから) ★★★ ▼▼▼
            // ★ マップチップの data_ を 1 (壁) に書き換えて、乗れるようにする
            int gridX, gridMapY;
            mapChip->GetGridCoordinates(blockPos, gridX, gridMapY);
            if (gridX != -1) {
                mapChip->SetGridCell(gridX, gridMapY, 1);
                // 書き換えた場所を記憶 (Reset / Rising 用)
                lastLandedGridX_ = gridX;
                lastLandedGridMapY_ = gridMapY;
            }
            // ▲▲▲ ★★★ 修正・追加 (ここまで) ★★★ ▲▲▲
        }
    }
    break;

    case BlockState::Landed:
    {
        // 着地済み
        // Type 4 (スパイク) の場合のみ、上昇トリガーをチェック
        if (type_ == BlockType::Spike) {

            // プレイヤーがブロックの上に乗ったか (Xが近く、Yがわずかに上)
            bool isPlayerOnTop = (dx < halfSize) &&
                (playerPos.y > blockPos.y) &&
                (playerPos.y < blockPos.y + kTriggerDistanceY_);

            if (isPlayerOnTop) {
                state_ = BlockState::Rising;

                // ▼▼▼ ★★★ 修正・追加 (ここから) ★★★ ▼▼▼
                // ★ 上昇する瞬間に、乗れなくする (data_ を 4 に戻す)
                if (lastLandedGridX_ != -1) {
                    mapChip->SetGridCell(lastLandedGridX_, lastLandedGridMapY_, (int)type_);
                    lastLandedGridX_ = -1; // 1回戻したらリセット
                    lastLandedGridMapY_ = -1;
                }
                // ▲▲▲ ★★★ 修正・追加 (ここまで) ★★★ ▲▲▲
            }
        }
    }
    break;

    case BlockState::Rising:
    {
        // 上昇中 (Type 4 のみ)
        blockPos.y += kRiseSpeed_;

        // 1. プレイヤーに当たったら死亡
        if (CheckCollision(player)) {
            player->Die();
        }

        // 2. 初期位置 (天井) まで戻ったら、再び落下状態に遷移
        if (blockPos.y >= initialPos_.y) {
            blockPos.y = initialPos_.y; // 天井にスナップ
            state_ = BlockState::Falling; // すぐに落下を再開
            // (この時点では data_ は 4 のままなので、何もしなくて良い)
        }
    }
    break;
    }

    // モデルのTransformを更新
    model_->transform = model_->transform;
}

void FallingBlock::Draw(
    ID3D12GraphicsCommandList* commandList,
    const Matrix4x4& viewProjectionMatrix,
    D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {

    // main.cpp から渡されたテクスチャハンドル (block.png) を使って描画
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