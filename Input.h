#pragma once
#include <Windows.h>
#include <XInput.h>
#include <cstdint>

#pragma comment(lib, "xinput.lib")

class Input {
public:
    // シングルトンインスタンスの取得
    static Input* GetInstance();

    // 初期化
    void Initialize();
    // 更新
    void Update();

    // キーボード関連
    bool IsKeyPressed(uint8_t key) const;
    bool IsKeyDown(uint8_t key) const;
    bool IsKeyReleased(uint8_t key) const;

    // ゲームパッド関連
    bool GetJoyState(XINPUT_STATE& out) const;
    bool GetJoyState(int stickNo, XINPUT_STATE& out) const; // 番号指定版

private:
    Input() = default;
    ~Input() = default;
    Input(const Input&) = delete;
    const Input& operator=(const Input&) = delete;

private:
    BYTE keys_[256] = {};
    BYTE prevKeys_[256] = {};

    XINPUT_STATE joyState_{};
    XINPUT_STATE prevJoyState_{};
    bool isJoyValid_ = false;
};