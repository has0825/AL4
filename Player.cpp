#include "Player.h"
#include "MathUtil.h"
#include <cmath> 

void Player::Initialize(Model* model) {
    model_ = model;
    transform_.scale = { 1.0f, 1.0f, 1.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, 0.0f, 0.0f };
    velocity_ = { 0.0f, 0.0f, 0.0f };
    jumpCount_ = 0;
}

void Player::Update() {
    Input* input = Input::GetInstance();

    const float moveSpeed = 0.2f;
    Vector3 move = { 0, 0, 0 };

    // --- キーボード入力による水平移動 ---
    if (input->IsKeyDown('D')) {
        move.x += moveSpeed;
    }
    if (input->IsKeyDown('A')) {
        move.x -= moveSpeed;
    }
    if (input->IsKeyDown('W')) {
        move.z += moveSpeed;
    }
    if (input->IsKeyDown('S')) {
        move.z -= moveSpeed;
    }

    // --- ゲームパッド入力による水平移動 ---
    XINPUT_STATE joyState;
    if (input->GetJoyState(joyState)) {
        const float deadZone = 0.7f;
        float stickX = (float)joyState.Gamepad.sThumbLX / SHRT_MAX;
        float stickY = (float)joyState.Gamepad.sThumbLY / SHRT_MAX;

        if (fabs(stickX) > deadZone) {
            move.x += stickX * moveSpeed;
        }
        if (fabs(stickY) > deadZone) {
            move.z += stickY * moveSpeed;
        }
    }

    // --- プレイヤーの回転処理 ---
    if (move.x != 0.0f || move.z != 0.0f) {
        // ★★★ 修正箇所 ★★★
        // move.x の符号を反転させることで、左右の向きを正しくする
        transform_.rotate.y = atan2(-move.x, move.z);
    }

    // --- ジャンプ処理 ---
    const int MAX_JUMP_COUNT = 2;

    if (input->IsKeyPressed(VK_SPACE)) {
        if (jumpCount_ < MAX_JUMP_COUNT) {
            const float jumpFirstSpeed = 1.0f;
            velocity_.y = jumpFirstSpeed;
            jumpCount_++;
        }
    }

    const float gravity = -0.05f;
    velocity_.y += gravity;

    transform_.translate.y += velocity_.y;

    if (transform_.translate.y <= 0.0f) {
        transform_.translate.y = 0.0f;
        velocity_.y = 0.0f;
        jumpCount_ = 0;
    }

    // 座標を更新
    transform_.translate.x += move.x;
    transform_.translate.z += move.z;

    // モデルのTransformにプレイヤーのTransformをコピー
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
    ImGui::SliderFloat3("Translate", &transform_.translate.x, -10.0f, 10.0f);
    ImGui::End();
}