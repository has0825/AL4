#include "Player.h"
#include "MathUtil.h"
#include <cmath>
#include <string>
#include <Windows.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void Player::Initialize(Model* model, MapChip* mapChip) {
    model_ = model;
    mapChip_ = mapChip;
    transform_.scale = { 0.4f, 0.4f, 0.4f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 3.0f, 5.0f, 0.0f };
    velocity_ = { 0.0f, 0.0f, 0.0f };
    onGround_ = false;
    wallTouch_ = WallTouchSide::None;
    jumpBufferTimer_ = 0.0f;
}

void Player::Update() {
    Input* input = Input::GetInstance();

    // 🔽🔽🔽 1. ジャンプ性能を「キック」らしく調整 🔽🔽🔽
    // --- 物理挙動で使う定数 ---
    const float kMoveSpeed = 0.1f;
    const float kGravity = 0.025f; // 少し重くしてキレを出す
    const float kJumpPower = 0.45f;  // 地上ジャンプも少し強く
    const float kWallSlideSpeed = 0.02f;
    const float kWallJumpPowerX = 0.3f;   // 横方向のキック力を強く
    const float kWallJumpPowerY = 0.42f;  // 縦方向のキック力も強く
    const float kPlayerHalfSize = 0.4f;
    // 🔼🔼🔼 ここまで 🔼🔼🔼

    // ▼▼▼ ステップ1: 物理演算と衝突判定を行い、現在の状態を確定させる ▼▼▼

    if (wallTouch_ != WallTouchSide::None && !onGround_ && velocity_.y < -kWallSlideSpeed) {
        velocity_.y = -kWallSlideSpeed;
    } else {
        velocity_.y -= kGravity;
    }

    onGround_ = false;
    wallTouch_ = WallTouchSide::None;

    Vector3 position = transform_.translate;
    position.y += velocity_.y;

    float playerTop = position.y + kPlayerHalfSize;
    float playerBottom = position.y - kPlayerHalfSize;
    float playerLeft = transform_.translate.x - kPlayerHalfSize;
    float playerRight = transform_.translate.x + kPlayerHalfSize;

    if (velocity_.y < 0) {
        if (mapChip_->CheckCollision({ playerLeft, playerBottom, 0 }) || mapChip_->CheckCollision({ playerRight, playerBottom, 0 })) {
            position.y = floor(playerBottom / MapChip::kBlockSize) * MapChip::kBlockSize + MapChip::kBlockSize + kPlayerHalfSize;
            velocity_.y = 0;
            onGround_ = true;
        }
    } else if (velocity_.y > 0) {
        if (mapChip_->CheckCollision({ playerLeft, playerTop, 0 }) || mapChip_->CheckCollision({ playerRight, playerTop, 0 })) {
            position.y = floor(playerTop / MapChip::kBlockSize) * MapChip::kBlockSize - kPlayerHalfSize;
            velocity_.y = 0;
        }
    }
    transform_.translate.y = position.y;

    position = transform_.translate;
    position.x += velocity_.x;

    playerLeft = position.x - kPlayerHalfSize;
    playerRight = position.x + kPlayerHalfSize;
    playerTop = position.y + kPlayerHalfSize;
    playerBottom = position.y - kPlayerHalfSize;

    if (velocity_.x < 0) {
        if (mapChip_->CheckCollision({ playerLeft, playerTop, 0 }) || mapChip_->CheckCollision({ playerLeft, playerBottom, 0 })) {
            position.x = floor(playerLeft / MapChip::kBlockSize) * MapChip::kBlockSize + MapChip::kBlockSize + kPlayerHalfSize;
            if (!onGround_) wallTouch_ = WallTouchSide::Left;
        }
    } else if (velocity_.x > 0) {
        if (mapChip_->CheckCollision({ playerRight, playerTop, 0 }) || mapChip_->CheckCollision({ playerRight, playerBottom, 0 })) {
            position.x = floor(playerRight / MapChip::kBlockSize) * MapChip::kBlockSize - kPlayerHalfSize;
            if (!onGround_) wallTouch_ = WallTouchSide::Right;
        }
    }
    transform_.translate.x = position.x;

    // ▼▼▼ ステップ2: 確定した状態を元に、キー入力を処理して次のフレームの速度を決める ▼▼▼

    float moveX = 0.0f;
    if (input->IsKeyDown('D')) { moveX = kMoveSpeed; }
    if (input->IsKeyDown('A')) { moveX = -kMoveSpeed; }
    velocity_.x = moveX;

    if (jumpBufferTimer_ > 0.0f) {
        jumpBufferTimer_ -= 0.016f;
    }
    if (input->IsKeyPressed(VK_SPACE)) {
        jumpBufferTimer_ = 0.1f;
    }

    if (jumpBufferTimer_ > 0.0f) {
        if (onGround_) {
            velocity_.y = kJumpPower;
            jumpBufferTimer_ = 0.0f;
        }
        // 🔽🔽🔽 2. 壁キックの入力条件を改善 🔽🔽🔽
        // 壁に張り付いている かつ 壁方向への入力がない時だけ壁キック
        else if (wallTouch_ == WallTouchSide::Left && moveX >= 0) {
            velocity_.y = kWallJumpPowerY;
            velocity_.x = kWallJumpPowerX;
            jumpBufferTimer_ = 0.0f;
            // 🔽🔽🔽 3. プレイヤーの向きを変更 🔽🔽🔽
            transform_.rotate.y = -M_PI / 2.0f; // 右を向く
        } else if (wallTouch_ == WallTouchSide::Right && moveX <= 0) {
            velocity_.y = kWallJumpPowerY;
            velocity_.x = -kWallJumpPowerX;
            jumpBufferTimer_ = 0.0f;
            // 🔽🔽🔽 3. プレイヤーの向きを変更 🔽🔽🔽
            transform_.rotate.y = M_PI / 2.0f; // 左を向く
        }
    }

    // 🔽🔽🔽 3. 地上での向き変更も追加 🔽🔽🔽
    if (onGround_) {
        if (moveX > 0) {
            transform_.rotate.y = -M_PI / 2.0f; // 右を向く
        } else if (moveX < 0) {
            transform_.rotate.y = M_PI / 2.0f; // 左を向く
        }
    }

    model_->transform = transform_;
}


void Player::Draw(
    ID3D12GraphicsCommandList* commandList,
    const Matrix4x4& viewProjectionMatrix,
    D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {
    model_->Draw(commandList, viewProjectionMatrix, lightGpuAddress, textureSrvHandle);
}

void Player::ImGui_Draw() {
    ImGui::Begin("Player");
    ImGui::SliderFloat3("Scale", &transform_.scale.x, 0.1f, 5.0f);
    ImGui::SliderAngle("RotateX", &transform_.rotate.x, -180.0f, 180.0f);
    ImGui::SliderAngle("RotateY", &transform_.rotate.y, -180.0f, 180.0f);
    ImGui::SliderAngle("RotateZ", &transform_.rotate.z, -180.0f, 180.0f);
    ImGui::SliderFloat3("Translate", &transform_.translate.x, -10.0f, 20.0f);
    ImGui::Text("Velocity: %.2f, %.2f", velocity_.x, velocity_.y);
    ImGui::Text("OnGround: %s", onGround_ ? "true" : "false");
    const char* wallText = "None";
    if (wallTouch_ == WallTouchSide::Left) wallText = "Left";
    if (wallTouch_ == WallTouchSide::Right) wallText = "Right";
    ImGui::Text("WallTouch: %s", wallText);
    ImGui::Text("JumpBuffer: %.2f", jumpBufferTimer_);
    ImGui::End();
}