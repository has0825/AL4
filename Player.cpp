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
    transform_.scale = { 0.4f, 0.4f, 0.4f }; // Note: Scale is visual, collision uses kPlayerHalfSize
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 3.0f, 5.0f, 0.0f };
    velocity_ = { 0.0f, 0.0f, 0.0f };
    onGround_ = false;
    wallTouch_ = WallTouchSide::None;
    jumpBufferTimer_ = 0.0f;
}

void Player::Update() {
    Input* input = Input::GetInstance();

    // --- 物理挙動で使う定数 ---
    const float kMoveSpeed = 0.1f;
    const float kGravity = 0.025f;
    const float kJumpPower = 0.45f;
    const float kWallSlideSpeed = 0.02f;
    const float kWallJumpPowerX = 0.3f;
    const float kWallJumpPowerY = 0.42f;
    // 🔽🔽🔽 **kPlayerHalfSize を 0.4f に戻す** 🔽🔽🔽
    const float kPlayerHalfSize = 0.2f; // 0.20f から 0.4f に戻す
    // 🔼🔼🔼 ************************************ 🔼🔼🔼

    // ▼▼▼ ステップ1: 物理演算と衝突判定を行い、現在の状態を確定させる ▼▼▼

    // 壁滑り or 重力
    if (wallTouch_ != WallTouchSide::None && !onGround_ && velocity_.y < -kWallSlideSpeed) {
        velocity_.y = -kWallSlideSpeed;
    } else {
        velocity_.y -= kGravity;
    }

    onGround_ = false; // フレーム開始時にリセット
    wallTouch_ = WallTouchSide::None; // フレーム開始時にリセット

    // Y方向の移動と衝突判定
    Vector3 position = transform_.translate;
    position.y += velocity_.y;

    float playerTop = position.y + kPlayerHalfSize;
    float playerBottom = position.y - kPlayerHalfSize;
    float playerLeft = transform_.translate.x - kPlayerHalfSize;
    float playerRight = transform_.translate.x + kPlayerHalfSize;

    if (velocity_.y < 0) { // 落下中の下方向判定
        if (mapChip_->CheckCollision({ playerLeft, playerBottom, 0 }) || mapChip_->CheckCollision({ playerRight, playerBottom, 0 })) {
            // 🔽🔽🔽 **- 0.001f の補正を削除** 🔽🔽🔽
            position.y = floor(playerBottom / MapChip::kBlockSize) * MapChip::kBlockSize + MapChip::kBlockSize + kPlayerHalfSize;
            // 🔼🔼🔼 ************************** 🔼🔼🔼
            velocity_.y = 0;
            onGround_ = true;
        }
    } else if (velocity_.y > 0) { // 上昇中の上方向判定
        if (mapChip_->CheckCollision({ playerLeft, playerTop, 0 }) || mapChip_->CheckCollision({ playerRight, playerTop, 0 })) {
            position.y = floor(playerTop / MapChip::kBlockSize) * MapChip::kBlockSize - kPlayerHalfSize;
            velocity_.y = 0;
        }
    }
    transform_.translate.y = position.y; // Y座標を確定

    // X方向の移動と衝突判定
    position = transform_.translate; // Y座標が確定した現在位置
    position.x += velocity_.x;       // X方向に移動した後の予測位置

    playerLeft = position.x - kPlayerHalfSize;   // 予測X位置での左端
    playerRight = position.x + kPlayerHalfSize;  // 予測X位置での右端
    playerTop = position.y + kPlayerHalfSize;    // 確定Y位置での上端
    playerBottom = position.y - kPlayerHalfSize; // 確定Y位置での下端

    if (velocity_.x < 0) { // 左移動
        if (mapChip_->CheckCollision({ playerLeft, playerTop, 0 }) || mapChip_->CheckCollision({ playerLeft, playerBottom, 0 })) {
            // 衝突したら壁の外側に位置を補正 (kPlayerHalfSize=0.4f で動いていたロジック)
            position.x = floor(playerLeft / MapChip::kBlockSize) * MapChip::kBlockSize + MapChip::kBlockSize + kPlayerHalfSize;
            if (!onGround_) wallTouch_ = WallTouchSide::Left;
            // 速度リセットは不要
        }
    } else if (velocity_.x > 0) { // 右移動
        if (mapChip_->CheckCollision({ playerRight, playerTop, 0 }) || mapChip_->CheckCollision({ playerRight, playerBottom, 0 })) {
            // 衝突したら壁の外側に位置を補正 (kPlayerHalfSize=0.4f で動いていたロジック)
            position.x = floor(playerRight / MapChip::kBlockSize) * MapChip::kBlockSize - kPlayerHalfSize;
            if (!onGround_) wallTouch_ = WallTouchSide::Right;
            // 速度リセットは不要
        }
    }
    transform_.translate.x = position.x; // X座標を確定


    // ▼▼▼ ステップ2: 確定した状態を元に、キー入力を処理して次のフレームの速度を決める ▼▼▼

    float moveX = 0.0f;
    if (input->IsKeyDown('D')) { moveX = kMoveSpeed; }
    if (input->IsKeyDown('A')) { moveX = -kMoveSpeed; }

    // 🔽🔽🔽 **速度更新をシンプルに (動いていたコードのロジック)** 🔽🔽🔽
    velocity_.x = moveX;
    // 🔼🔼🔼 ************************************************ 🔼🔼🔼

    // ジャンプの先行入力処理
    if (jumpBufferTimer_ > 0.0f) {
        jumpBufferTimer_ -= 0.016f;
    }
    if (input->IsKeyPressed(VK_SPACE)) {
        jumpBufferTimer_ = 0.1f;
    }

    // ジャンプ実行判定
    if (jumpBufferTimer_ > 0.0f) {
        if (onGround_) { // 地上ジャンプ
            velocity_.y = kJumpPower;
            jumpBufferTimer_ = 0.0f;
            onGround_ = false; // ジャンプしたら接地解除
        }
        // 壁キック (動いていたコードのロジック)
        else if (wallTouch_ == WallTouchSide::Left && moveX >= 0) {
            velocity_.y = kWallJumpPowerY;
            velocity_.x = kWallJumpPowerX; // 右へキック
            jumpBufferTimer_ = 0.0f;
            transform_.rotate.y = -M_PI / 2.0f; // 右向き
            wallTouch_ = WallTouchSide::None; // 壁接触解除
        } else if (wallTouch_ == WallTouchSide::Right && moveX <= 0) {
            velocity_.y = kWallJumpPowerY;
            velocity_.x = -kWallJumpPowerX; // 左へキック
            jumpBufferTimer_ = 0.0f;
            transform_.rotate.y = M_PI / 2.0f; // 左向き
            wallTouch_ = WallTouchSide::None; // 壁接触解除
        }
    }

    // 向きの更新 (動いていたコードのロジック)
    if (onGround_) { // 地上
        if (moveX > 0) {
            transform_.rotate.y = -M_PI / 2.0f;
        } else if (moveX < 0) {
            transform_.rotate.y = M_PI / 2.0f;
        }
    }
    // 空中での向き変更を追加 (壁接触時以外)
    else if (wallTouch_ == WallTouchSide::None) {
        if (velocity_.x > 0.01f) {
            transform_.rotate.y = -M_PI / 2.0f; // 右向き
        } else if (velocity_.x < -0.01f) {
            transform_.rotate.y = M_PI / 2.0f; // 左向き
        }
    }

    model_->transform = transform_; // モデルに最終的なTransformを反映
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
    ImGui::Text("Velocity: %.3f, %.3f", velocity_.x, velocity_.y);
    ImGui::Text("OnGround: %s", onGround_ ? "true" : "false");
    const char* wallText = "None";
    if (wallTouch_ == WallTouchSide::Left) wallText = "Left";
    if (wallTouch_ == WallTouchSide::Right) wallText = "Right";
    ImGui::Text("WallTouch: %s", wallText);
    ImGui::Text("JumpBuffer: %.3f", jumpBufferTimer_);
    ImGui::End();
}