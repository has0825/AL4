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
    const float kJumpPower = 0.35f;
    const float kWallSlideSpeed = 0.02f;
    const float kWallJumpPowerX = 0.3f;
    const float kWallJumpPowerY = 0.42f;
    const float kPlayerHalfSize = 0.2f;

    // ▼▼▼ 死亡時の演出処理 ▼▼▼
    if (!isAlive_) {
        velocity_.y -= kGravity;
        transform_.translate.y += velocity_.y;

        // 死亡演出：回転
        transform_.rotate.z += 0.1f;
        transform_.rotate.x += 0.05f;

        model_->transform = transform_;
        return; // 操作を受け付けない
    }

    // ▼▼▼ 生存時の通常更新 ▼▼▼
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

    if (input->IsKeyPressed('J')) {
        PlayerBullet* newBullet = new PlayerBullet();
        float bulletSpeed = 0.3f * lrDirection_;
        newBullet->Initialize(bulletModel_, transform_.translate, bulletSpeed);
        bullets_.push_back(newBullet);
    }

    bullets_.remove_if([this](PlayerBullet* bullet) {
        if (bullet->Update(mapChip_)) {
            delete bullet;
            return true;
        }
        return false;
        });

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
            if (!(playerRight > mapWidth && (transform_.translate.y > topExitY_Min || transform_.translate.y < bottomExitY_Max))) {
                position.x = floor(playerRight / MapChip::kBlockSize) * MapChip::kBlockSize - kPlayerHalfSize - 0.001f;
                if (!onGround_) wallTouch_ = WallTouchSide::Right;
                velocity_.x = 0;
            }
        }
    }

    transform_.translate = position;

    if (jumpBufferTimer_ > 0.0f) jumpBufferTimer_ -= 0.016f;
    if (input->IsKeyPressed(VK_SPACE)) jumpBufferTimer_ = 0.1f;

    if (jumpBufferTimer_ > 0.0f) {
        if (onGround_) {
            velocity_.y = kJumpPower;
            jumpBufferTimer_ = 0.0f;
            onGround_ = false;
        } else if (wallTouch_ == WallTouchSide::Left && moveX >= 0) {
            velocity_.y = kWallJumpPowerY; velocity_.x = kWallJumpPowerX;
            jumpBufferTimer_ = 0.0f; transform_.rotate.y = -(float)M_PI / 2.0f;
            wallTouch_ = WallTouchSide::None; lrDirection_ = 1.0f;
        } else if (wallTouch_ == WallTouchSide::Right && moveX <= 0) {
            velocity_.y = kWallJumpPowerY; velocity_.x = -kWallJumpPowerX;
            jumpBufferTimer_ = 0.0f; transform_.rotate.y = (float)M_PI / 2.0f;
            wallTouch_ = WallTouchSide::None; lrDirection_ = -1.0f;
        }
    }

    if (onGround_) {
        if (moveX > 0.0f) transform_.rotate.y = -(float)M_PI / 2.0f;
        else if (moveX < 0.0f) transform_.rotate.y = (float)M_PI / 2.0f;
    } else if (wallTouch_ == WallTouchSide::None) {
        if (velocity_.x > 0.01f) transform_.rotate.y = -(float)M_PI / 2.0f;
        else if (velocity_.x < -0.01f) transform_.rotate.y = (float)M_PI / 2.0f;
    }

    model_->transform = transform_;
}

void Player::Die() {
    if (!isAlive_) return;
    isAlive_ = false;
    velocity_.x = 0.0f;
    velocity_.y = 0.4f; // 跳ね上がり
}

void Player::Reset() {
    SetPosition(initialPosition_);
    isAlive_ = true;
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
    model_->transform = transform_;
    initialPosition_ = pos;
    for (PlayerBullet* bullet : bullets_) delete bullet;
    bullets_.clear();
}