#pragma once
#include "Model.h"
#include "MapChip.h"

class PlayerBullet {
public:
    // 初期化 (モデル、初期座標、移動速度X)
    void Initialize(Model* model, const Vector3& position, float velocityX);

    // 更新 (寿命が尽きたり壁に当たったら true を返す)
    bool Update(MapChip* mapChip);

    // 描画
    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const Matrix4x4& viewProjectionMatrix,
        D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle);

private:
    Model* model_ = nullptr;
    Transform transform_{};
    float velocityX_ = 0.0f;
    float lifeTimer_ = 0.0f; // 生存時間

    // 定数: 寿命は2秒 (60FPS想定)
    const float kLifeTime = 2.0f;
};