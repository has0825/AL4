#pragma once
#include "Model.h"
#include "Input.h"
#include "externals/imgui/imgui.h"
#include "MapChip.h"

class Player {
public:
    void Initialize(Model* model, MapChip* mapChip);
    void Update();
    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const Matrix4x4& viewProjectionMatrix,
        D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle);
    void ImGui_Draw();

private:
    // 壁に触れている方向を管理
    enum class WallTouchSide {
        None,
        Left,
        Right
    };

    Model* model_ = nullptr;
    MapChip* mapChip_ = nullptr;
    Transform transform_{};
    Vector3 velocity_{};

    // 状態管理変数
    bool onGround_ = false;
    WallTouchSide wallTouch_ = WallTouchSide::None;
    float jumpBufferTimer_ = 0.0f; // ジャンプの先行入力タイマー
};