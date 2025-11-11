#include "Trap.h"
#include <cmath> // std::abs
#include <cassert> // assert

Trap::~Trap() {
    delete wall_;
}

// (Initialize は変更なし)
void Trap::Initialize(ID3D12Device* device, float triggerY, AttackSide side, float stopMargin) {
    // "cube.obj" を使用
    wall_ = Model::Create("Resources/cube", "cube.obj", device);

    wallHalfSize_ = MapChip::kBlockSize / 2.0f;
    wall_->transform.scale = { MapChip::kBlockSize, MapChip::kBlockSize, MapChip::kBlockSize };

    // --- パラメータをメンバ変数に保存 ---
    trapY_ = triggerY;         // このトラップが作動するY座標
    side_ = side;             // 攻撃方向
    stopMargin_ = stopMargin;   // 停止マージン

    // --- 座標を決定 ---
    mapWidth_ = 20.0f * MapChip::kBlockSize;
    offscreenMargin_ = MapChip::kBlockSize * 3.0f;

    Reset();
}


// (Reset は変更なし)
void Trap::Reset() {
    // --- 状態をリセット ---
    currentState_ = State::Idle; // ★ 必ず Idle に戻す
    waitTimer_ = 0.0f;
    isPlayerInZone_ = false; // Yゾーンから出たことにする

    // --- 壁を画面外の待機位置に戻す ---
    if (side_ == AttackSide::FromLeft) {
        wall_->transform.translate = { -offscreenMargin_, trapY_, 0.0f };
    } else { // FromRight
        wall_->transform.translate = { mapWidth_ + offscreenMargin_, trapY_, 0.0f };
    }
}


void Trap::Update(Player* player) {
    // 状態が Finished なら、何もせず終了
    if (currentState_ == State::Finished) {
        return;
    }

    // プレイヤーが死んでいたら
    if (!player->IsAlive()) {
        return;
    }

    const Vector3& playerPos = player->GetPosition();
    Vector3& wallPos = wall_->transform.translate; // 壁の現在位置 (参照)

    const float kDeltaTime = 1.0f / 60.0f; // 60FPS固定と仮定

    bool wasInZone = isPlayerInZone_;
    isPlayerInZone_ = (std::abs(playerPos.y - trapY_) < MapChip::kBlockSize * 0.5f);


    // === ステートマシン ===
    switch (currentState_) {

        // (Idle は変更なし)
    case State::Idle:
    {
        if (isPlayerInZone_ && !wasInZone) {

            currentState_ = State::Attacking;

            // stopMargin が 0.6f 未満なら「ひょこっと出る」モード
            bool isShortTrap = (stopMargin_ < (MapChip::kBlockSize * 0.8f));

            if (side_ == AttackSide::FromLeft) {
                startX_ = -offscreenMargin_;
                if (isShortTrap) {
                    targetX_ = wallHalfSize_ + stopMargin_;
                } else {
                    targetX_ = playerPos.x - stopMargin_ - player->GetHalfSize();
                    if (targetX_ < wallHalfSize_) { targetX_ = wallHalfSize_; }
                }
            } else {
                startX_ = mapWidth_ + offscreenMargin_;
                if (isShortTrap) {
                    targetX_ = mapWidth_ - wallHalfSize_ - stopMargin_;
                } else {
                    targetX_ = playerPos.x + stopMargin_ + player->GetHalfSize();
                    if (targetX_ > mapWidth_ - wallHalfSize_) { targetX_ = mapWidth_ - wallHalfSize_; }
                }
            }
            returnX_ = startX_;
            wallPos = { startX_, trapY_, 0.0f };
        }
    }
    break;

    case State::Attacking:
        // ▼▼▼ ★★★ ここを修正 (削除) ★★★ ▼▼▼
        // プレイヤーがYゾーンから出ても (ジャンプしても) 戻らないように、
        // isPlayerInZone_ のチェックを削除します。
        /*
        if (!isPlayerInZone_ && currentState_ != State::Returning) {
            currentState_ = State::Returning;
            break;
        }
        */
        // ▲▲▲ ★★★ 修正完了 ★★★ ▲▲▲

        // ターゲットに向かって移動
        if (side_ == AttackSide::FromLeft) {
            wallPos.x += kSpeed_;
            if (wallPos.x >= targetX_) {
                wallPos.x = targetX_;
                currentState_ = State::Waiting;
                waitTimer_ = kWaitTime_;
            }
        } else { // FromRight
            wallPos.x -= kSpeed_;
            if (wallPos.x <= targetX_) {
                wallPos.x = targetX_;
                currentState_ = State::Waiting;
                waitTimer_ = kWaitTime_;
            }
        }

        if (CheckCollision(player)) {
            player->Die();
            currentState_ = State::Returning;
        }
        break;

    case State::Waiting:
        // ▼▼▼ ★★★ ここを修正 (削除) ★★★ ▼▼▼
        // プレイヤーがYゾーンから出ても (ジャンプしても) 戻らないように、
        // isPlayerInZone_ のチェックを削除します。
        /*
        if (!isPlayerInZone_ && currentState_ != State::Returning) {
            currentState_ = State::Returning;
            break;
        }
        */
        // ▲▲▲ ★★★ 修正完了 ★★★ ▲▲▲

        waitTimer_ -= kDeltaTime;
        if (waitTimer_ <= 0.0f) {
            currentState_ = State::Returning;
        }

        if (CheckCollision(player)) {
            player->Die();
            currentState_ = State::Returning;
        }
        break;

        // (Returning, Finished は変更なし)
    case State::Returning:
        // 元の位置 (画面外) に戻る
        if (side_ == AttackSide::FromLeft) {
            wallPos.x -= kSpeed_;
            if (wallPos.x <= returnX_) {
                wallPos.x = returnX_;
                currentState_ = State::Finished; // ★ Idle に戻さず Finished にする
            }
        } else { // FromRight
            wallPos.x += kSpeed_;
            if (wallPos.x >= returnX_) {
                wallPos.x = returnX_;
                currentState_ = State::Finished; // ★ Idle に戻さず Finished にする
            }
        }
        break;

    case State::Finished:
        // 何もしない
        break;
    }
}

void Trap::Draw(
    ID3D12GraphicsCommandList* commandList,
    const Matrix4x4& viewProjectionMatrix,
    D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {

    // ★★★ 待機中 (Idle) と 完了 (Finished) 以外は描画 ★★★
    if (currentState_ != State::Idle && currentState_ != State::Finished) {
        wall_->Draw(commandList, viewProjectionMatrix, lightGpuAddress, textureSrvHandle);
    }
}

bool Trap::CheckCollision(Player* player) {
    Vector3 pPos = player->GetPosition();
    float pSize = player->GetHalfSize();
    float pLeft = pPos.x - pSize;
    float pRight = pPos.x + pSize;
    float pTop = pPos.y + pSize;
    float pBottom = pPos.y - pSize;

    Vector3 wPos = wall_->transform.translate;
    float wLeft = wPos.x - wallHalfSize_;
    float wRight = wPos.x + wallHalfSize_;
    float wTop = wPos.y + wallHalfSize_;
    float wBottom = wPos.y - wallHalfSize_;

    if (pLeft > wRight || pRight < wLeft || pTop < wBottom || pBottom > wTop) {
        return false;
    }
    return true;
}