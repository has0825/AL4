#pragma once
#include "Model.h"
#include "Input.h"
#include "externals/imgui/imgui.h"
#include "MapChip.h"
#include "PlayerBullet.h" 
#include <list>

class Player {
public:
    void Initialize(Model* model, MapChip* mapChip, ID3D12Device* device);

    void Update();
    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const Matrix4x4& viewProjectionMatrix,
        D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle);
    void ImGui_Draw();

    void Die();
    void Reset();

    // 生存確認
    bool IsAlive() const { return isAlive_; }

    // 無敵確認 (ローリング中など)
    bool IsInvincible() const { return isRolling_; }

    const Vector3& GetPosition() const { return transform_.translate; }
    float GetHalfSize() const { return 0.2f; }
    bool IsOnGround() const { return onGround_; }

    bool IsExiting() const;
    void SetPosition(const Vector3& pos);
    void UpdateClearAnimation();

private:
    enum class WallTouchSide {
        None,
        Left,
        Right
    };

    Model* model_ = nullptr;
    MapChip* mapChip_ = nullptr;
    Transform transform_{};
    Vector3 velocity_{};

    bool onGround_ = false;
    WallTouchSide wallTouch_ = WallTouchSide::None;

    // ジャンプ・壁ジャンプ関連
    float jumpBufferTimer_ = 0.0f;
    float wallJumpLockTimer_ = 0.0f; // 壁キック後の操作不能時間

    // ローリング関連
    bool isRolling_ = false;       // ローリング中か
    float rollTimer_ = 0.0f;       // ローリングの残り時間
    float rollCooldown_ = 0.0f;    // 次にローリングできるまでの時間

    bool isAlive_ = true;
    Vector3 initialPosition_{};

    // --- 弾関連 ---
    Model* bulletModel_ = nullptr;
    std::list<PlayerBullet*> bullets_;
    float lrDirection_ = 1.0f; // 1.0:右, -1.0:左

};