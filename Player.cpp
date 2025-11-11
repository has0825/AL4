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

    // リセット用に初期位置を記憶
    initialPosition_ = { 2.0f, 9.0f, 0.0f };

    // SetPosition を呼んで初期化
    SetPosition(initialPosition_);
    isAlive_ = true;
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
    const float kPlayerHalfSize = 0.2f;

    // ▼▼▼ ステップ1: 入力を処理して、このフレームのX方向の基本速度を決定する ▼▼▼
    float moveX = 0.0f;
    if (input->IsKeyDown('D')) { moveX = kMoveSpeed; }
    if (input->IsKeyDown('A')) { moveX = -kMoveSpeed; }

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
    Vector3 position = transform_.translate;
    position.y += velocity_.y;

    float playerTop = position.y + kPlayerHalfSize;
    float playerBottom = position.y - kPlayerHalfSize;
    float playerLeft = transform_.translate.x - kPlayerHalfSize;
    float playerRight = transform_.translate.x + kPlayerHalfSize;

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

    // X方向の移動と衝突判定
    position.x += velocity_.x;

    playerLeft = position.x - kPlayerHalfSize;
    playerRight = position.x + kPlayerHalfSize;
    playerTop = position.y + kPlayerHalfSize;
    playerBottom = position.y - kPlayerHalfSize;

    // (A) 空中にいる時用のY座標 (壁抜け対策: ブロックの中を見る)
    float checkY_Bottom_ForWall = playerBottom - 0.001f;
    // (B) 地上にいる時用のY座標 (地面誤認対策: ブロックの上を見る)
    float checkY_Bottom_ForMove = playerBottom + 0.001f;

    float checkY_Top = playerTop - 0.001f;

    if (velocity_.x < 0) { // 左移動
        float checkY_Bottom = onGround_ ? checkY_Bottom_ForMove : checkY_Bottom_ForWall;

        if (mapChip_->CheckCollision({ playerLeft, checkY_Top, 0 }) || mapChip_->CheckCollision({ playerLeft, checkY_Bottom, 0 })) {
            position.x = floor(playerLeft / MapChip::kBlockSize) * MapChip::kBlockSize + MapChip::kBlockSize + kPlayerHalfSize + 0.001f;
            if (!onGround_) wallTouch_ = WallTouchSide::Left;
            velocity_.x = 0;
        }
    } else if (velocity_.x > 0) { // 右移動
        float checkY_Bottom = onGround_ ? checkY_Bottom_ForMove : checkY_Bottom_ForWall;

        // まず当たり判定をチェック
        bool collision = mapChip_->CheckCollision({ playerRight, checkY_Top, 0 }) || mapChip_->CheckCollision({ playerRight, checkY_Bottom, 0 });

        if (collision) {
            // 当たった場合

            // マップの右端かどうかの判定準備
            size_t colCount = 20; // デフォルト
            if (mapChip_->GetColCount() > 0) colCount = mapChip_->GetColCount();
            float mapWidth = static_cast<float>(colCount) * MapChip::kBlockSize;

            // 出口となるY座標の定義 (map.csv の15行マップに基づく)
            float topExitY_Min = 7.7f;      // マップ上部 (CSV Y=1,2,3 あたり) の下限
            float bottomExitY_Max = 0.7f;   // マップ下部 (CSV Y=14) の上限

            bool isOutOfMap = (playerRight > mapWidth); // 画面外か？
            bool isAtTopExit = (transform_.translate.y > topExitY_Min);    // 上の出口か？
            bool isAtBottomExit = (transform_.translate.y < bottomExitY_Max);  // 下の出口か？

            // 「画面外」かつ「出口Y座標」の場合、衝突を *無視* する
            if (isOutOfMap && (isAtTopExit || isAtBottomExit)) {
                // 何もしない (そのまま画面外へ進む)
            } else {
                // それ以外の「本当の壁」か「画面外だがYが違う」場合
                position.x = floor(playerRight / MapChip::kBlockSize) * MapChip::kBlockSize - kPlayerHalfSize - 0.001f;
                if (!onGround_) wallTouch_ = WallTouchSide::Right;
                velocity_.x = 0;
            }
        }
    }

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
            onGround_ = false;
        } else if (wallTouch_ == WallTouchSide::Left && moveX >= 0) {
            velocity_.y = kWallJumpPowerY;
            velocity_.x = kWallJumpPowerX;
            jumpBufferTimer_ = 0.0f;
            transform_.rotate.y = -(float)M_PI / 2.0f;
            wallTouch_ = WallTouchSide::None;
        } else if (wallTouch_ == WallTouchSide::Right && moveX <= 0) {
            velocity_.y = kWallJumpPowerY;
            velocity_.x = -kWallJumpPowerX;
            jumpBufferTimer_ = 0.0f;
            transform_.rotate.y = (float)M_PI / 2.0f;
            wallTouch_ = WallTouchSide::None;
        }
    }

    // 向きの更新
    if (onGround_) {
        if (moveX > 0.0f) {
            transform_.rotate.y = -(float)M_PI / 2.0f;
        } else if (moveX < 0.0f) {
            transform_.rotate.y = (float)M_PI / 2.0f;
        }
    } else if (wallTouch_ == WallTouchSide::None) {
        if (velocity_.x > 0.01f) {
            transform_.rotate.y = -(float)M_PI / 2.0f;
        } else if (velocity_.x < -0.01f) {
            transform_.rotate.y = (float)M_PI / 2.0f;
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
    ImGui::Text("Velocity: %.3f, %.3f", velocity_.x, velocity_.y);
    ImGui::Text("OnGround: %s", onGround_ ? "true" : "false");
    const char* wallText = "None";
    if (wallTouch_ == WallTouchSide::Left) wallText = "Left";
    if (wallTouch_ == WallTouchSide::Right) wallText = "Right";
    ImGui::Text("WallTouch: %s", wallText);
    ImGui::Text("JumpBuffer: %.3f", jumpBufferTimer_);

    ImGui::Text("isAlive: %s", isAlive_ ? "TRUE" : "FALSE");

    ImGui::End();
}

void Player::Die() {
    isAlive_ = false;
    velocity_ = { 0.0f, 0.0f, 0.0f };
}

void Player::Reset() {
    SetPosition(initialPosition_);
    isAlive_ = true;
}

// --- ▼▼▼ マップ遷移のために追加/修正 ▼▼▼ ---

bool Player::IsExiting() const {
    if (!mapChip_) { return false; }

    // CSVの列数 (20列と仮定)
    size_t colCount = 20;
    if (mapChip_->GetColCount() > 0) {
        colCount = mapChip_->GetColCount();
    }

    // マップの右端のワールド座標
    float mapWidth = static_cast<float>(colCount) * MapChip::kBlockSize;

    // プレイヤーの中心座標
    const Vector3& pos = transform_.translate;

    // 1. 画面右端より外に出たか？
    if (pos.x > mapWidth) {
        // 出口となるY座標の定義 (map.csv の15行マップに基づく)
        float topExitY_Min = 7.7f;      // マップ上部 (CSV Y=1,2,3 あたり) の下限
        float bottomExitY_Max = 0.7f;   // マップ下部 (CSV Y=14) の上限

        // 2. 上の出口Y座標にいるか？
        if (pos.y > topExitY_Min) {
            return true;
        }
        // 3. 下の出口Y座標にいるか？
        if (pos.y < bottomExitY_Max) {
            return true;
        }
    }

    return false;
}

void Player::SetPosition(const Vector3& pos) {
    transform_.translate = pos;
    velocity_ = { 0.0f, 0.0f, 0.0f };
    onGround_ = false; // 空中からスタート
    wallTouch_ = WallTouchSide::None;
    jumpBufferTimer_ = 0.0f;
    model_->transform = transform_; // モデルにも反映

    // ▼▼▼ ★★★ ここが修正点 ★★★ ▼▼▼
    // 渡された位置を、新しいリスポーン地点として記憶する
    initialPosition_ = pos;
    // ▲▲▲ ★★★ 修正完了 ★★★ ▲▲▲
}
// --- ▲▲▲ 追加完了 ▲▲▲ ---