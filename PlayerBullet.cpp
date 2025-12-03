#include "PlayerBullet.h"
#include <cmath> // floorなど

void PlayerBullet::Initialize(Model* model, const Vector3& position, float velocityX) {
    model_ = model;
    transform_.translate = position;
    transform_.scale = { 0.2f, 0.2f, 0.2f }; // 弾のサイズ
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    velocityX_ = velocityX;
    lifeTimer_ = kLifeTime;
}

bool PlayerBullet::Update(MapChip* mapChip) {
    // 1. 移動
    transform_.translate.x += velocityX_;

    // 2. 時間経過で消滅
    lifeTimer_ -= 1.0f / 60.0f;
    if (lifeTimer_ <= 0.0f) {
        return true; // 消滅
    }

    // 3. 壁との当たり判定
    if (mapChip->CheckCollision(transform_.translate)) {
        return true; // 消滅
    }

    // ★★★ 重要：ここで model_->transform を更新してはいけません！ ★★★
    // ここで更新すると、共有しているモデルの位置が「最後の弾の位置」で上書きされ、
    // 全ての弾が同じ場所に描画されてしまいます。

    return false; // 生存
}

void PlayerBullet::Draw(
    ID3D12GraphicsCommandList* commandList,
    const Matrix4x4& viewProjectionMatrix,
    D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {

    // ★★★ 修正点：描画する「直前」に、この弾の位置をモデルにセットする ★★★
    model_->transform = transform_;

    // 描画コマンドの発行
    model_->Draw(commandList, viewProjectionMatrix, lightGpuAddress, textureSrvHandle);
}