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

    // --- ▼▼▼ トラップ実装のために追加 ▼▼▼ ---
    void Die();
    void Reset();
    bool IsAlive() const { return isAlive_; }
    const Vector3& GetPosition() const { return transform_.translate; }
    float GetHalfSize() const { return 0.2f; }
    bool IsOnGround() const { return onGround_; }
    // --- ▲▲▲ 追加ここまで ▲▲▲ ---

    // --- ▼▼▼ ★★★ マップ遷移のために追加 ★★★ ▼▼▼ ---

    // マップの出口（右下）から外に出たか判定
    bool IsExiting() const;

    // プレイヤーの位置を強制的に設定（マップ遷移用）
    void SetPosition(const Vector3& pos);

    // --- ▲▲▲ ★★★ 追加ここまで ★★★ ▲▲▲ ---

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
    float jumpBufferTimer_ = 0.0f;

    // --- ▼▼▼ トラップ実装のために追加 ▼▼▼ ---
    bool isAlive_ = true;
    Vector3 initialPosition_{}; // リセット用に初期位置を保存
    // --- ▲▲▲ 追加ここまで ▲▲▲ ---
};