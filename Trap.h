#pragma once
#include "Model.h"
#include "Player.h" // Playerの情報を参照するため
#include "MapChip.h"  // kBlockSize を参照するため

class Trap {
public:
    // 攻撃方向 (FromLeft/FromRight)
    enum class AttackSide {
        FromLeft,
        FromRight
    };

    ~Trap(); // デストラクタでモデルを解放

    // 初期化 (作動Y座標, 攻撃方向, 停止マージン)
    void Initialize(ID3D12Device* device, float triggerY, AttackSide side, float stopMargin);

    // 更新
    void Update(Player* player);

    // 描画
    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const Matrix4x4& viewProjectionMatrix,
        D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle); // main から cube.jpg のハンドルを受け取る

    // リセット
    void Reset();

private:
    // AABB (軸並行境界ボックス) での当たり判定
    bool CheckCollision(Player* player);

    // トラップの状態
    enum class State {
        Idle,       // 待機中 (プレイヤーがYゾーンに入るのを待つ)
        Attacking,  // 攻撃中 (プレイヤーに向かって移動)
        Waiting,    // 停止中 (2秒待機)
        Returning,  // 帰還中 (元の位置に戻る)
        Finished    // ★★★ 完了 (二度と動かない) ★★★
    };
    State currentState_ = State::Idle;

    // このトラップの攻撃方向 (Initializeで設定)
    AttackSide side_ = AttackSide::FromLeft;

    // 落下してくる壁モデル (1つ)
    Model* wall_ = nullptr;

    // 壁の当たり判定サイズ (MapChip::kBlockSize と同じ)
    float wallHalfSize_ = 0.0f;

    // --- 動作パラメータ ---
    const float kSpeed_ = 0.2f;      // 壁の移動速度
    const float kWaitTime_ = 2.0f;    // 停止時間 (2秒)
    float waitTimer_ = 0.0f;          // 停止用タイマー

    // --- 座標管理 ---
    float startX_ = 0.0f;   // 攻撃開始時のX座標 (画面外)
    float targetX_ = 0.0f;  // 停止目標のX座標
    float returnX_ = 0.0f;  // 戻るX座標 (startX_ と同じ)
    float trapY_ = 0.0f;    // ★ このトラップが作動するY座標 (Initializeで設定)
    float mapWidth_ = 0.0f;
    float stopMargin_ = 0.0f;// ★ 停止マージン (Initializeで設定)
    float offscreenMargin_ = 0.0f;

    // --- トリガー管理 ---
    bool isPlayerInZone_ = false; // プレイヤーがトラップのY座標範囲にいるか
};