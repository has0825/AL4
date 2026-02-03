#include "Trap.h"
#include <cmath> // std::abs
#include <cassert> // assert

Trap::~Trap() {
    delete wall_;
}

void Trap::Initialize(ID3D12Device* device, float triggerY, AttackSide side, float stopMargin) {
    wall_ = Model::Create("Resources/cube", "cube.obj", device);
    wallHalfSize_ = MapChip::kBlockSize / 2.0f;
    wall_->transform.scale = { MapChip::kBlockSize, MapChip::kBlockSize, MapChip::kBlockSize };
    trapY_ = triggerY;
    side_ = side;
    stopMargin_ = stopMargin;
    mapWidth_ = 20.0f * MapChip::kBlockSize;
    offscreenMargin_ = MapChip::kBlockSize * 3.0f;
    Reset();
}

void Trap::Reset() {
    currentState_ = State::Idle;
    waitTimer_ = 0.0f;
    isPlayerInZone_ = false;
    if (side_ == AttackSide::FromLeft) {
        wall_->transform.translate = { -offscreenMargin_, trapY_, 0.0f };
    } else {
        wall_->transform.translate = { mapWidth_ + offscreenMargin_, trapY_, 0.0f };
    }
}

void Trap::Update(Player* player) {
    if (currentState_ == State::Finished) { return; }

    const Vector3& playerPos = player->GetPosition();
    Vector3& wallPos = wall_->transform.translate;
    const float kDeltaTime = 1.0f / 60.0f;

    bool wasInZone = isPlayerInZone_;
    // ゾーン判定はプレイヤーが生きている時だけ行う
    if (player->IsAlive()) {
        isPlayerInZone_ = (std::abs(playerPos.y - trapY_) < MapChip::kBlockSize * 0.5f);
    }

    switch (currentState_) {
    case State::Idle:
    {
        if (player->IsAlive() && isPlayerInZone_ && !wasInZone) {
            currentState_ = State::Attacking;
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
        if (side_ == AttackSide::FromLeft) {
            wallPos.x += kSpeed_;
            if (wallPos.x >= targetX_) {
                wallPos.x = targetX_;
                currentState_ = State::Waiting;
                waitTimer_ = kWaitTime_;
            }
        } else {
            wallPos.x -= kSpeed_;
            if (wallPos.x <= targetX_) {
                wallPos.x = targetX_;
                currentState_ = State::Waiting;
                waitTimer_ = kWaitTime_;
            }
        }
        // ★修正点：当たり判定があってもステートを変えず、死亡させるだけにする
        if (player->IsAlive() && CheckCollision(player)) {
            player->Die();
            // currentState_ = State::Returning; // ←ここをコメントアウト/削除
        }
        break;

    case State::Waiting:
        waitTimer_ -= kDeltaTime;
        if (waitTimer_ <= 0.0f) { currentState_ = State::Returning; }

        // ★修正点：待機中でも死亡させるだけにする（即座に帰らせない）
        if (player->IsAlive() && CheckCollision(player)) {
            player->Die();
            // currentState_ = State::Returning; // ←ここをコメントアウト/削除
        }
        break;

    case State::Returning:
        if (side_ == AttackSide::FromLeft) {
            wallPos.x -= kSpeed_;
            if (wallPos.x <= returnX_) {
                wallPos.x = returnX_;
                currentState_ = State::Finished;
            }
        } else {
            wallPos.x += kSpeed_;
            if (wallPos.x >= returnX_) {
                wallPos.x = returnX_;
                currentState_ = State::Finished;
            }
        }
        break;

    case State::Finished:
        break;
    }
}

void Trap::Draw(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjectionMatrix, D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {
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
    if (pLeft > wRight || pRight < wLeft || pTop < wBottom || pBottom > wTop) { return false; }
    return true;
}