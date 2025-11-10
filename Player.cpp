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
    transform_.translate = { 2.0f, 9.0f, 0.0f };
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

    // ▼▼▼ ステップ1: 入力を処理して、このフレームのX方向の基本速度を決定する ▼▼▼
    float moveX = 0.0f;
    if (input->IsKeyDown('D')) { moveX = kMoveSpeed; }
    if (input->IsKeyDown('A')) { moveX = -kMoveSpeed; }

    // 基本のX速度を設定 (壁ジャンプで上書きされる可能性あり)
    velocity_.x = moveX;


    // ▼▼▼ ステップ2: 物理演算と衝突判定 (Y -> X の順) ▼▼▼

    // 壁滑り or 重力
    if (wallTouch_ != WallTouchSide::None && !onGround_ && velocity_.y < -kWallSlideSpeed) {
        velocity_.y = -kWallSlideSpeed;
    } else {
        velocity_.y -= kGravity;
    }

    onGround_ = false; // フレーム開始時にリセット
    wallTouch_ = WallTouchSide::None; // フレーム開始時にリセット

    // Y方向の移動と衝突判定
    Vector3 position = transform_.translate; // 現在位置を取得
    position.y += velocity_.y; // Y方向に移動

    float playerTop = position.y + kPlayerHalfSize;
    float playerBottom = position.y - kPlayerHalfSize;
    float playerLeft = transform_.translate.x - kPlayerHalfSize;  // Y判定では「現在のX」を使う
    float playerRight = transform_.translate.x + kPlayerHalfSize; // Y判定では「現在のX」を使う

    if (velocity_.y < 0) { // 落下中の下方向判定
        if (mapChip_->CheckCollision({ playerLeft, playerBottom, 0 }) || mapChip_->CheckCollision({ playerRight, playerBottom, 0 })) {
            position.y = floor(playerBottom / MapChip::kBlockSize) * MapChip::kBlockSize + MapChip::kBlockSize + kPlayerHalfSize;
            velocity_.y = 0;
            onGround_ = true;
        }
    } else if (velocity_.y > 0) { // 上昇中の上方向判定
        if (mapChip_->CheckCollision({ playerLeft, playerTop, 0 }) || mapChip_->CheckCollision({ playerRight, playerTop, 0 })) {
            position.y = floor(playerTop / MapChip::kBlockSize) * MapChip::kBlockSize - kPlayerHalfSize;
            velocity_.y = 0;
        }
    }
    // Y座標が確定 (position.y)

    // X方向の移動と衝突判定
    position.x += velocity_.x; // Yが確定した position に X の移動を加える

    playerLeft = position.x - kPlayerHalfSize;   // X判定では「予測X」を使う
    playerRight = position.x + kPlayerHalfSize;  // X判定では「予測X」を使う
    playerTop = position.y + kPlayerHalfSize;    // X判定では「確定Y」を使う
    playerBottom = position.y - kPlayerHalfSize; // X判定では「確定Y」を使う

    // ▼▼▼ ★★★ ここが今回の修正点 ★★★ ▼▼▼
    // Y座標のチェック位置を、プレイヤーの状態(地上/空中)によって切り替える

    // (A) 空中にいる時用のY座標 (壁抜け対策: ブロックの中を見る)
    float checkY_Bottom_ForWall = playerBottom - 0.001f;
    // (B) 地上にいる時用のY座標 (地面誤認対策: ブロックの上を見る)
    float checkY_Bottom_ForMove = playerBottom + 0.001f;

    // プレイヤーの上端 (これは共通)
    float checkY_Top = playerTop - 0.001f;

    if (velocity_.x < 0) { // 左移動
        // onGround_ の状態に応じて (A) か (B) を選択
        float checkY_Bottom = onGround_ ? checkY_Bottom_ForMove : checkY_Bottom_ForWall;

        if (mapChip_->CheckCollision({ playerLeft, checkY_Top, 0 }) || mapChip_->CheckCollision({ playerLeft, checkY_Bottom, 0 })) {
            position.x = floor(playerLeft / MapChip::kBlockSize) * MapChip::kBlockSize + MapChip::kBlockSize + kPlayerHalfSize + 0.001f;
            if (!onGround_) wallTouch_ = WallTouchSide::Left;
            velocity_.x = 0; // 壁に当たったらX速度をリセット
        }
    } else if (velocity_.x > 0) { // 右移動
        // onGround_ の状態に応じて (A) か (B) を選択
        float checkY_Bottom = onGround_ ? checkY_Bottom_ForMove : checkY_Bottom_ForWall;

        if (mapChip_->CheckCollision({ playerRight, checkY_Top, 0 }) || mapChip_->CheckCollision({ playerRight, checkY_Bottom, 0 })) {
            position.x = floor(playerRight / MapChip::kBlockSize) * MapChip::kBlockSize - kPlayerHalfSize - 0.001f;
            if (!onGround_) wallTouch_ = WallTouchSide::Right;
            velocity_.x = 0; // 壁に当たったらX速度をリセット
        }
    }

    // 最終的な座標を transform_ に反映
    transform_.translate = position;


    // ▼▼▼ ステップ3: 確定した状態を元に、ジャンプ処理を行う ▼▼▼

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
        // 壁キック (入力 moveX が必要)
        else if (wallTouch_ == WallTouchSide::Left && moveX >= 0) {
            velocity_.y = kWallJumpPowerY;
            velocity_.x = kWallJumpPowerX; // ★ここでX速度が上書きされる
            jumpBufferTimer_ = 0.0f;
            transform_.rotate.y = -M_PI / 2.0f; // 右向き
            wallTouch_ = WallTouchSide::None; // 壁接触解除
        } else if (wallTouch_ == WallTouchSide::Right && moveX <= 0) {
            velocity_.y = kWallJumpPowerY;
            velocity_.x = -kWallJumpPowerX; // ★ここでX速度が上書きされる
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
        // 最終的な速度(velocity_.x)で判断
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