#include "Player.h"
#include "MathUtil.h"
#include <cmath>
#include <string>
#include <Windows.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void Player::Initialize(Model* model, MapChip* mapChip, ID3D12Device* device) {
    model_ = model;
    mapChip_ = mapChip;
    transform_.scale = { 0.4f, 0.4f, 0.4f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };

    initialPosition_ = { 2.0f, 9.0f, 0.0f };
    SetPosition(initialPosition_);
    isAlive_ = true;

    // 弾のモデル生成
    bulletModel_ = Model::Create("Resources/cube", "cube.obj", device);
}

void Player::Update() {
    Input* input = Input::GetInstance();

    // --- パラメータ調整 ---
    const float kMoveSpeed = 0.1f;
    const float kGravity = 0.025f;
    const float kJumpPower = 0.35f;       // 通常ジャンプ力
    const float kWallSlideSpeed = 0.05f;  // 壁ずり落ち速度
    const float kWallJumpPowerX = 0.2f;   // 壁キックの横飛ばし力
    const float kWallJumpPowerY = 0.42f;  // 壁キックの上昇力
    const float kPlayerHalfSize = 0.2f;

    // ローリング用パラメータ
    const float kRollSpeed = 0.25f;       // ローリング速度
    const float kRollDuration = 0.4f;     // ローリング時間(秒)
    const float kRollCooldownTime = 0.6f; // クールタイム(秒)

    // ▼▼▼ 死亡時の演出処理 ▼▼▼
    if (!isAlive_) {
        velocity_.y -= kGravity;
        transform_.translate.y += velocity_.y;
        transform_.rotate.z += 0.1f;
        transform_.rotate.x += 0.05f;
        model_->transform = transform_;
        return;
    }

    // --- タイマー更新 ---
    if (rollCooldown_ > 0.0f) rollCooldown_ -= 1.0f / 60.0f;
    if (wallJumpLockTimer_ > 0.0f) wallJumpLockTimer_ -= 1.0f / 60.0f;

    // ▼▼▼ ローリング開始処理 (Shiftキー) ▼▼▼
    if (input->IsKeyPressed(VK_SHIFT) && !isRolling_ && rollCooldown_ <= 0.0f) {
        isRolling_ = true;
        rollTimer_ = kRollDuration;
        rollCooldown_ = kRollCooldownTime;

        // 向きに合わせて初速を与える
        velocity_.x = lrDirection_ * kRollSpeed;
        velocity_.y = 0.0f; // 重力無視で直進させたい場合は0にする（今回は少し浮かすか、地面を転がるか）
    }

    // ▼▼▼ ローリング中の更新 ▼▼▼
    if (isRolling_) {
        rollTimer_ -= 1.0f / 60.0f;

        // ローリング中は速度固定
        velocity_.x = lrDirection_ * kRollSpeed;

        // 見た目の回転 (進行方向に回転)
        transform_.rotate.z += (lrDirection_ * 0.5f);

        // 終了判定
        if (rollTimer_ <= 0.0f) {
            isRolling_ = false;
            transform_.rotate.z = 0.0f; // 回転リセット
        }

        // ローリング中は重力を適用するか？（ここでは適用するが少し弱くする例）
        velocity_.y -= kGravity * 0.5f;

    }
    // ▼▼▼ 通常時の移動・ジャンプ処理 ▼▼▼
    else {
        // 壁ジャンプ直後は入力を受け付けない (慣性を働かせるため)
        if (wallJumpLockTimer_ <= 0.0f) {
            velocity_.x = 0.0f;
            float moveX = 0.0f;

            if (input->IsKeyDown('D')) {
                moveX = kMoveSpeed;
                lrDirection_ = 1.0f;
            }
            if (input->IsKeyDown('A')) {
                moveX = -kMoveSpeed;
                lrDirection_ = -1.0f;
            }
            velocity_.x = moveX;
        } else {
            // ロック中は空気抵抗のみ (少し減速)
            velocity_.x *= 0.98f;
        }

        // 射撃 (ローリング中は撃てない)
        if (input->IsKeyPressed('J')) {
            PlayerBullet* newBullet = new PlayerBullet();
            float bulletSpeed = 0.3f * lrDirection_;
            newBullet->Initialize(bulletModel_, transform_.translate, bulletSpeed);
            bullets_.push_back(newBullet);
        }

        // 重力と壁ずり落ち
        if (wallTouch_ != WallTouchSide::None && !onGround_ && velocity_.y < -kWallSlideSpeed) {
            // 壁に張り付いて落ちる
            velocity_.y = -kWallSlideSpeed;
            // 壁キック演出のために少し回転させてもいいかも
        } else {
            velocity_.y -= kGravity;
        }
    }

    // --- 弾の更新 ---
    bullets_.remove_if([this](PlayerBullet* bullet) {
        if (bullet->Update(mapChip_)) {
            delete bullet;
            return true;
        }
        return false;
        });

    // --- 衝突判定前リセット ---
    onGround_ = false;
    WallTouchSide prevWallTouch = wallTouch_;
    wallTouch_ = WallTouchSide::None;

    // ==========================================
    // 物理挙動とコリジョン (Y軸)
    // ==========================================
    Vector3 position = transform_.translate;
    position.y += velocity_.y;

    float playerTop = position.y + kPlayerHalfSize;
    float playerBottom = position.y - kPlayerHalfSize;
    float playerLeft = transform_.translate.x - kPlayerHalfSize;
    float playerRight = transform_.translate.x + kPlayerHalfSize;

    // 床・天井判定
    if (velocity_.y < 0) {
        if (mapChip_->CheckCollision({ playerLeft, playerBottom, 0 }) || mapChip_->CheckCollision({ playerRight, playerBottom, 0 })) {
            position.y = floor(playerBottom / MapChip::kBlockSize) * MapChip::kBlockSize + MapChip::kBlockSize + kPlayerHalfSize;
            velocity_.y = 0;
            onGround_ = true;
            // 着地したら壁ジャンプロック解除
            wallJumpLockTimer_ = 0.0f;
        }
    } else if (velocity_.y > 0) {
        if (mapChip_->CheckCollision({ playerLeft, playerTop, 0 }) || mapChip_->CheckCollision({ playerRight, playerTop, 0 })) {
            position.y = floor(playerTop / MapChip::kBlockSize) * MapChip::kBlockSize - kPlayerHalfSize;
            velocity_.y = 0;
        }
    }

    // ==========================================
    // 物理挙動とコリジョン (X軸)
    // ==========================================
    position.x += velocity_.x;
    playerLeft = position.x - kPlayerHalfSize;
    playerRight = position.x + kPlayerHalfSize;
    playerTop = position.y + kPlayerHalfSize;
    playerBottom = position.y - kPlayerHalfSize;

    // 少し内側で判定しないと、壁ずり落ち中に地面判定が暴れることがあるため微調整
    float checkY_Top = playerTop - 0.05f;
    float checkY_Bottom = playerBottom + 0.05f;

    if (velocity_.x < 0) { // 左移動
        if (mapChip_->CheckCollision({ playerLeft, checkY_Top, 0 }) || mapChip_->CheckCollision({ playerLeft, checkY_Bottom, 0 })) {
            position.x = floor(playerLeft / MapChip::kBlockSize) * MapChip::kBlockSize + MapChip::kBlockSize + kPlayerHalfSize + 0.001f;
            if (!onGround_) wallTouch_ = WallTouchSide::Left;
            velocity_.x = 0;
            // ローリング中に壁にぶつかったら止まる
            if (isRolling_) isRolling_ = false;
        }
    } else if (velocity_.x > 0) { // 右移動
        // マップ端判定含む
        size_t colCount = 20;
        if (mapChip_->GetColCount() > 0) colCount = mapChip_->GetColCount();
        float mapWidth = static_cast<float>(colCount) * MapChip::kBlockSize;
        float topExitY_Min = 7.7f;
        float bottomExitY_Max = 0.7f;

        bool mapChipHit = mapChip_->CheckCollision({ playerRight, checkY_Top, 0 }) || mapChip_->CheckCollision({ playerRight, checkY_Bottom, 0 });
        bool mapEdgeHit = (playerRight > mapWidth && (transform_.translate.y > topExitY_Min || transform_.translate.y < bottomExitY_Max));

        if (mapChipHit || (!mapEdgeHit && playerRight > mapWidth)) {
            // 衝突した場合
            if (mapChipHit || playerRight > mapWidth) {
                // exit条件を満たしていないのに画面外に出ようとした、または壁に当たった
                if (mapChipHit) {
                    position.x = floor(playerRight / MapChip::kBlockSize) * MapChip::kBlockSize - kPlayerHalfSize - 0.001f;
                    if (!onGround_) wallTouch_ = WallTouchSide::Right;
                    velocity_.x = 0;
                    if (isRolling_) isRolling_ = false;
                }
            }
        }
    }

    transform_.translate = position;

    // ▼▼▼ ジャンプ処理 (先行入力あり) ▼▼▼
    if (jumpBufferTimer_ > 0.0f) jumpBufferTimer_ -= 0.016f;
    if (input->IsKeyPressed(VK_SPACE) && !isRolling_) jumpBufferTimer_ = 0.1f;

    if (jumpBufferTimer_ > 0.0f) {
        if (onGround_) {
            // 通常ジャンプ
            velocity_.y = kJumpPower;
            jumpBufferTimer_ = 0.0f;
            onGround_ = false;
        } else if (wallTouch_ == WallTouchSide::Left) {
            // 左壁キック (右上に飛ぶ)
            velocity_.y = kWallJumpPowerY;
            velocity_.x = kWallJumpPowerX;
            jumpBufferTimer_ = 0.0f;
            wallTouch_ = WallTouchSide::None;
            lrDirection_ = 1.0f;

            // ★壁ジャンプ直後は入力をロックして飛距離を稼ぐ
            wallJumpLockTimer_ = 0.3f;
        } else if (wallTouch_ == WallTouchSide::Right) {
            // 右壁キック (左上に飛ぶ)
            velocity_.y = kWallJumpPowerY;
            velocity_.x = -kWallJumpPowerX;
            jumpBufferTimer_ = 0.0f;
            wallTouch_ = WallTouchSide::None;
            lrDirection_ = -1.0f;

            // ★壁ジャンプ直後は入力をロック
            wallJumpLockTimer_ = 0.3f;
        }
    }

    // --- 見た目の向き反映 (ローリング中は回転制御しているので上書きしない) ---
    if (!isRolling_) {
        if (onGround_) {
            if (lrDirection_ > 0.0f) transform_.rotate.y = -(float)M_PI / 2.0f;
            else transform_.rotate.y = (float)M_PI / 2.0f;
        } else {
            // 空中や壁キック中
            if (velocity_.x > 0.01f) transform_.rotate.y = -(float)M_PI / 2.0f;
            else if (velocity_.x < -0.01f) transform_.rotate.y = (float)M_PI / 2.0f;
        }
        // Z回転を戻す
        transform_.rotate.z = 0.0f;
    }

    model_->transform = transform_;
}

void Player::Die() {
    // 無敵中（ローリング中）なら死なない
    if (isRolling_) return;

    if (!isAlive_) return;
    isAlive_ = false;
    velocity_.x = 0.0f;
    velocity_.y = 0.4f; // 跳ね上がり
}

void Player::Reset() {
    SetPosition(initialPosition_);
    isAlive_ = true;
    isRolling_ = false;
    wallJumpLockTimer_ = 0.0f;
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    for (PlayerBullet* bullet : bullets_) delete bullet;
    bullets_.clear();
}

void Player::Draw(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjectionMatrix, D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {
    model_->Draw(commandList, viewProjectionMatrix, lightGpuAddress, textureSrvHandle);
    for (PlayerBullet* bullet : bullets_) bullet->Draw(commandList, viewProjectionMatrix, lightGpuAddress, textureSrvHandle);
}

void Player::ImGui_Draw() {
    ImGui::Begin("Player");
    ImGui::Text("isAlive: %s", isAlive_ ? "TRUE" : "FALSE");
    ImGui::Text("Rolling: %s", isRolling_ ? "YES" : "NO");
    ImGui::Text("Wall: %d", (int)wallTouch_);
    ImGui::Text("Pos: %.2f, %.2f", transform_.translate.x, transform_.translate.y);
    ImGui::End();
}

bool Player::IsExiting() const {
    if (!mapChip_) return false;
    float mapWidth = static_cast<float>(mapChip_->GetColCount()) * MapChip::kBlockSize;
    if (transform_.translate.x > mapWidth) {
        if (transform_.translate.y > 7.7f || transform_.translate.y < 0.7f) return true;
    }
    return false;
}

void Player::SetPosition(const Vector3& pos) {
    transform_.translate = pos;
    velocity_ = { 0.0f, 0.0f, 0.0f };
    onGround_ = false;
    wallTouch_ = WallTouchSide::None;
    jumpBufferTimer_ = 0.0f;
    wallJumpLockTimer_ = 0.0f;
    isRolling_ = false;
    model_->transform = transform_;
    initialPosition_ = pos;
    for (PlayerBullet* bullet : bullets_) delete bullet;
    bullets_.clear();
}

void Player::UpdateClearAnimation() {
    // 勝利の回転（Y軸回転）
    transform_.rotate.y += 0.2f;

    // ゆっくり上昇
    transform_.translate.y += 0.02f;

    // 少し手前に傾ける（楽しげに見えるように）
    transform_.rotate.z = 0.1f * std::sin(transform_.translate.y * 5.0f);

    // モデル行列更新
    model_->transform = transform_;
}