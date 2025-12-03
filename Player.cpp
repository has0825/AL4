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

    // 弾のモデル生成 (Cubeを再利用)
    bulletModel_ = Model::Create("Resources/cube", "cube.obj", device);
}

void Player::Update() {
    Input* input = Input::GetInstance();

    const float kMoveSpeed = 0.1f;
    const float kGravity = 0.025f;
    const float kJumpPower = 0.45f;
    const float kWallSlideSpeed = 0.02f;
    const float kWallJumpPowerX = 0.3f;
    const float kWallJumpPowerY = 0.42f;
    const float kPlayerHalfSize = 0.2f;

    // ▼▼▼ 移動と向き ▼▼▼
    velocity_.x = 0.0f;
    float moveX = 0.0f;

    if (input->IsKeyDown('D')) {
        moveX = kMoveSpeed;
        lrDirection_ = 1.0f; // 右
    }
    if (input->IsKeyDown('A')) {
        moveX = -kMoveSpeed;
        lrDirection_ = -1.0f; // 左
    }
    velocity_.x = moveX;


    // ▼▼▼ 弾の発射 (Jキー) ▼▼▼
    if (input->IsKeyPressed('J')) {
        PlayerBullet* newBullet = new PlayerBullet();

        // 速度: 向きに合わせて設定 (0.3)
        float bulletSpeed = 0.3f * lrDirection_;

        // 初期化
        newBullet->Initialize(bulletModel_, transform_.translate, bulletSpeed);

        // リストに追加 (これで前の弾も消えない)
        bullets_.push_back(newBullet);
    }

    // ▼▼▼ 弾の更新と削除 ▼▼▼
    bullets_.remove_if([this](PlayerBullet* bullet) {
        // 寿命か壁衝突で true が返ってきたら削除
        if (bullet->Update(mapChip_)) {
            delete bullet; // メモリ解放
            return true;   // リストから外す
        }
        return false;
        });


    // ▼▼▼ プレイヤーの物理演算 ▼▼▼

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

    position.x += velocity_.x;

    playerLeft = position.x - kPlayerHalfSize;
    playerRight = position.x + kPlayerHalfSize;
    playerTop = position.y + kPlayerHalfSize;
    playerBottom = position.y - kPlayerHalfSize;

    float checkY_Bottom_ForWall = playerBottom - 0.001f;
    float checkY_Bottom_ForMove = playerBottom + 0.001f;
    float checkY_Top = playerTop - 0.001f;

    if (velocity_.x < 0) {
        float checkY_Bottom = onGround_ ? checkY_Bottom_ForMove : checkY_Bottom_ForWall;
        if (mapChip_->CheckCollision({ playerLeft, checkY_Top, 0 }) || mapChip_->CheckCollision({ playerLeft, checkY_Bottom, 0 })) {
            position.x = floor(playerLeft / MapChip::kBlockSize) * MapChip::kBlockSize + MapChip::kBlockSize + kPlayerHalfSize + 0.001f;
            if (!onGround_) wallTouch_ = WallTouchSide::Left;
            velocity_.x = 0;
        }
    } else if (velocity_.x > 0) {
        float checkY_Bottom = onGround_ ? checkY_Bottom_ForMove : checkY_Bottom_ForWall;
        bool collision = mapChip_->CheckCollision({ playerRight, checkY_Top, 0 }) || mapChip_->CheckCollision({ playerRight, checkY_Bottom, 0 });

        if (collision) {
            size_t colCount = 20;
            if (mapChip_->GetColCount() > 0) colCount = mapChip_->GetColCount();
            float mapWidth = static_cast<float>(colCount) * MapChip::kBlockSize;
            float topExitY_Min = 7.7f;
            float bottomExitY_Max = 0.7f;
            bool isOutOfMap = (playerRight > mapWidth);
            bool isAtTopExit = (transform_.translate.y > topExitY_Min);
            bool isAtBottomExit = (transform_.translate.y < bottomExitY_Max);

            if (isOutOfMap && (isAtTopExit || isAtBottomExit)) {
                // 出口
            } else {
                position.x = floor(playerRight / MapChip::kBlockSize) * MapChip::kBlockSize - kPlayerHalfSize - 0.001f;
                if (!onGround_) wallTouch_ = WallTouchSide::Right;
                velocity_.x = 0;
            }
        }
    }

    transform_.translate = position;

    // --- ジャンプ ---
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
            onGround_ = false;
        } else if (wallTouch_ == WallTouchSide::Left && moveX >= 0) {
            velocity_.y = kWallJumpPowerY;
            velocity_.x = kWallJumpPowerX;
            jumpBufferTimer_ = 0.0f;
            transform_.rotate.y = -(float)M_PI / 2.0f;
            wallTouch_ = WallTouchSide::None;
            lrDirection_ = 1.0f;
        } else if (wallTouch_ == WallTouchSide::Right && moveX <= 0) {
            velocity_.y = kWallJumpPowerY;
            velocity_.x = -kWallJumpPowerX;
            jumpBufferTimer_ = 0.0f;
            transform_.rotate.y = (float)M_PI / 2.0f;
            wallTouch_ = WallTouchSide::None;
            lrDirection_ = -1.0f;
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

    // プレイヤー本体の描画
    model_->Draw(commandList, viewProjectionMatrix, lightGpuAddress, textureSrvHandle);

    // ★ 弾の描画
    for (PlayerBullet* bullet : bullets_) {
        bullet->Draw(commandList, viewProjectionMatrix, lightGpuAddress, textureSrvHandle);
    }
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
    ImGui::Text("Bullets: %d", (int)bullets_.size()); // 弾数の確認
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

    // リセット時に弾を消す
    for (PlayerBullet* bullet : bullets_) {
        delete bullet;
    }
    bullets_.clear();
}

bool Player::IsExiting() const {
    if (!mapChip_) { return false; }
    size_t colCount = 20;
    if (mapChip_->GetColCount() > 0) colCount = mapChip_->GetColCount();
    float mapWidth = static_cast<float>(colCount) * MapChip::kBlockSize;
    const Vector3& pos = transform_.translate;
    if (pos.x > mapWidth) {
        float topExitY_Min = 7.7f;
        float bottomExitY_Max = 0.7f;
        if (pos.y > topExitY_Min) return true;
        if (pos.y < bottomExitY_Max) return true;
    }
    return false;
}

void Player::SetPosition(const Vector3& pos) {
    transform_.translate = pos;
    velocity_ = { 0.0f, 0.0f, 0.0f };
    onGround_ = false;
    wallTouch_ = WallTouchSide::None;
    jumpBufferTimer_ = 0.0f;
    model_->transform = transform_;
    initialPosition_ = pos;

    // 位置リセット時に弾を消す
    for (PlayerBullet* bullet : bullets_) {
        delete bullet;
    }
    bullets_.clear();
}