#pragma once
#include "Model.h"
#include "Input.h"
#include "externals/imgui/imgui.h"
#include "MapChip.h"
#include "PlayerBullet.h" 
#include <list> // ★★★ これが必要です

class Player {
public:
    // device引数を追加
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
    bool IsAlive() const { return isAlive_; }
    const Vector3& GetPosition() const { return transform_.translate; }
    float GetHalfSize() const { return 0.2f; }
    bool IsOnGround() const { return onGround_; }

    bool IsExiting() const;
    void SetPosition(const Vector3& pos);

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
    float jumpBufferTimer_ = 0.0f;

    bool isAlive_ = true;
    Vector3 initialPosition_{};

    // --- 弾関連 ---
    Model* bulletModel_ = nullptr;          // 弾の共通モデル
    std::list<PlayerBullet*> bullets_;      // 弾のリスト (ポインタ変数は使わずリストのみ)
    float lrDirection_ = 1.0f;              // 向き (1.0:右, -1.0:左)
};