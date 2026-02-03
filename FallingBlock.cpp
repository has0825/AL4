#include "FallingBlock.h"
#include <cmath> // std::abs
#include "DirectXCommon.h" 

FallingBlock::~FallingBlock() {
    delete model_;
}

void FallingBlock::Initialize(ID3D12Device* device, const Vector3& initialPos, BlockType type) {
    model_ = Model::Create("Resources/Trap", "Trap.obj", device);
    initialPos_ = initialPos;
    type_ = type;
    model_->transform.scale = { MapChip::kBlockSize, MapChip::kBlockSize, MapChip::kBlockSize };
    state_ = BlockState::Idle;
    model_->transform.translate = initialPos_;
    landedY_ = initialPos_.y;
    lastLandedGridX_ = -1;
    lastLandedGridMapY_ = -1;
    moveDirX_ = 0.0f;
    isCeiling_ = false;
}

void FallingBlock::Reset(MapChip* mapChip) {
    if (lastLandedGridX_ != -1 && lastLandedGridMapY_ != -1) {
        mapChip->SetGridCell(lastLandedGridX_, lastLandedGridMapY_, 0);
    }
    model_->transform.translate = initialPos_;
    state_ = BlockState::Idle;
    landedY_ = initialPos_.y;
    lastLandedGridX_ = -1;
    lastLandedGridMapY_ = -1;
    moveDirX_ = 0.0f;
    isCeiling_ = false;
}

void FallingBlock::Update(Player* player, MapChip* mapChip) {
    // プレイヤー生存時のみ接触判定（即死）を行う
    if (player->IsAlive() && CheckCollision(player)) {
        player->Die();
        return;
    }

    const Vector3& playerPos = player->GetPosition();
    Vector3& blockPos = model_->transform.translate;

    float dx = std::abs(playerPos.x - blockPos.x);
    float dy = playerPos.y - blockPos.y;
    float distSq = (playerPos.x - blockPos.x) * (playerPos.x - blockPos.x) +
        (playerPos.y - blockPos.y) * (playerPos.y - blockPos.y);

    float halfSize = MapChip::kBlockSize / 2.0f;

    // マップの上端Y座標を計算 (rowCount * size)
    // マップチップの仕様上、一番下のブロックの中心Yは kBlockSize/2
    // 一番上のブロックの中心Yは (rowCount-1)*size + size/2
    float mapTopY = static_cast<float>(mapChip->GetRowCount()) * MapChip::kBlockSize;

    switch (state_) {
    case BlockState::Idle:
    {
        if (lastLandedGridX_ == -1 && type_ != BlockType::StaticHazard && !isCeiling_) {
            int gridX, gridMapY;
            mapChip->GetGridCoordinates(blockPos, gridX, gridMapY);
            if (gridX != -1) {
                mapChip->SetGridCell(gridX, gridMapY, 1);
                lastLandedGridX_ = gridX;
                lastLandedGridMapY_ = gridMapY;
            }
        }

        bool isAlignX = (dx < halfSize);
        bool shouldAct = false;

        if (player->IsAlive()) {
            if (type_ == BlockType::FallOnly || type_ == BlockType::Spike) {
                float kSearchRange = MapChip::kBlockSize * 5.0f;
                if (isAlignX && dy < 0 && dy > -kSearchRange) {
                    state_ = BlockState::Falling;
                    shouldAct = true;
                }
            } else if (type_ == BlockType::RiseOnTop) {
                if (isAlignX && dy > 0) {
                    state_ = BlockState::Rising;
                    shouldAct = true;
                }
            } else if (type_ == BlockType::SideAttack) {
                float kTriggerRadius = MapChip::kBlockSize * 6.0f;
                if (distSq < kTriggerRadius * kTriggerRadius) {
                    state_ = BlockState::MovingSide;
                    shouldAct = true;
                    moveDirX_ = (playerPos.x > blockPos.x) ? 1.0f : -1.0f;
                }
            } else if (type_ == BlockType::FallOnTop) {
                if (isAlignX && dy > 0) {
                    state_ = BlockState::Falling;
                    shouldAct = true;
                }
            } else if (type_ == BlockType::RiseThenFall) {
                if (isAlignX && dy > 0) {
                    state_ = BlockState::Rising;
                    shouldAct = true;
                    isCeiling_ = false;
                }
            }
        }

        if (shouldAct && lastLandedGridX_ != -1) {
            mapChip->SetGridCell(lastLandedGridX_, lastLandedGridMapY_, 0);
            lastLandedGridX_ = -1;
            lastLandedGridMapY_ = -1;
        }
    }
    break;

    case BlockState::Falling:
    {
        blockPos.y -= kFallSpeed_;
        Vector3 footPos = { blockPos.x, blockPos.y - halfSize - 0.01f, 0.0f };
        if (mapChip->CheckCollision(footPos)) {
            int gx, gy;
            mapChip->GetGridCoordinates(footPos, gx, gy);
            int tileType = mapChip->GetGridValue(gx, gy);
            // プレイヤー初期位置(2)はすり抜けるが、それ以外のブロック(1)なら着地
            if (tileType == 2) {
                // mapChip->SetGridCell(gx, gy, 0); // ここで消すとリスポーン地点が消える可能性があるので注意
            } else {
                state_ = BlockState::Landed;
                blockPos.y = floor(footPos.y / MapChip::kBlockSize) * MapChip::kBlockSize + MapChip::kBlockSize + halfSize;
                landedY_ = blockPos.y;
                isCeiling_ = false;
                if (gx != -1) {
                    mapChip->SetGridCell(gx, gy, 1);
                    lastLandedGridX_ = gx;
                    lastLandedGridMapY_ = gy;
                }
            }
        }
    }
    break;

    case BlockState::Landed:
    {
        if (lastLandedGridX_ == -1 && type_ != BlockType::StaticHazard && !isCeiling_) {
            int gridX, gridMapY;
            mapChip->GetGridCoordinates(blockPos, gridX, gridMapY);
            if (gridX != -1) {
                mapChip->SetGridCell(gridX, gridMapY, 1);
                lastLandedGridX_ = gridX;
                lastLandedGridMapY_ = gridMapY;
            }
        }
        if (player->IsAlive()) {
            bool isAlignX = (dx < halfSize);
            if (type_ == BlockType::Spike) {
                if (isAlignX && playerPos.y > blockPos.y) {
                    state_ = BlockState::Rising;
                    if (lastLandedGridX_ != -1) {
                        mapChip->SetGridCell(lastLandedGridX_, lastLandedGridMapY_, 0);
                        lastLandedGridX_ = -1;
                        lastLandedGridMapY_ = -1;
                    }
                }
            } else if (type_ == BlockType::RiseThenFall && isCeiling_) {
                float kSearchRange = MapChip::kBlockSize * 10.0f;
                if (isAlignX && dy < 0 && dy > -kSearchRange) {
                    state_ = BlockState::Falling;
                    isCeiling_ = false;
                    if (lastLandedGridX_ != -1) {
                        mapChip->SetGridCell(lastLandedGridX_, lastLandedGridMapY_, 0);
                        lastLandedGridX_ = -1;
                        lastLandedGridMapY_ = -1;
                    }
                }
            }
        }
    }
    break;

    case BlockState::Rising:
    {
        blockPos.y += kRiseSpeed_;

        // ★修正点: マップ上端チェック
        // ブロックの上端がマップの最上部を超えたら強制停止させる
        float blockTop = blockPos.y + halfSize;
        bool hitCeiling = false;

        // 1. マップ範囲外（上）に出たか？
        if (blockTop >= mapTopY) {
            hitCeiling = true;
            // 位置補正: マップ上端の内側に収める（天井に張り付く）
            blockPos.y = mapTopY - halfSize;
        }
        // 2. 通常のブロック衝突判定
        else {
            Vector3 headPos = { blockPos.x, blockPos.y + halfSize + 0.01f, 0.0f };
            if (mapChip->CheckCollision(headPos)) {
                int gx, gy;
                mapChip->GetGridCoordinates(headPos, gx, gy);
                int tileType = mapChip->GetGridValue(gx, gy);
                if (tileType == 2) {
                    // mapChip->SetGridCell(gx, gy, 0); 
                } else {
                    hitCeiling = true;
                    // 位置補正: 衝突したブロックの下側に収める
                    blockPos.y = floor(headPos.y / MapChip::kBlockSize) * MapChip::kBlockSize - halfSize;
                }
            }
        }

        // 天井にぶつかった（またはマップ端に達した）場合の処理
        if (hitCeiling) {
            state_ = BlockState::Landed;
            if (type_ == BlockType::RiseThenFall) {
                isCeiling_ = true;
                // ここで lastLandedGridX_ を更新すべきだが、天井に張り付いた状態を
                // マップチップとして「1」にしてしまうと、自分が埋まってしまう可能性があるため注意。
                // 今回は「天井待機中」として扱い、マップデータは書き換えないか、
                // あるいは書き換えるなら座標を再取得する。
                int gx, gy;
                mapChip->GetGridCoordinates(blockPos, gx, gy);
                if (gx != -1) {
                    mapChip->SetGridCell(gx, gy, 1);
                    lastLandedGridX_ = gx;
                    lastLandedGridMapY_ = gy;
                }
            }
        }
    }
    break;

    case BlockState::MovingSide:
    {
        blockPos.x += kSideSpeed_ * moveDirX_;
    }
    break;
    }
    model_->transform.translate = blockPos;
}

void FallingBlock::Draw(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjectionMatrix, D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {
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
    if (pLeft > wRight || pRight < wLeft || pTop < wBottom || pBottom > wTop) { return false; }
    return true;
}