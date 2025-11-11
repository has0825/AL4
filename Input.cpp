#include "Input.h"
#include <cassert>

Input* Input::GetInstance() {
    static Input instance;
    return &instance;
}

void Input::Initialize() {
    // 全キーの入力状態を取得する
    HRESULT hr = GetKeyboardState(keys_);
    assert(SUCCEEDED(hr));
    memcpy(prevKeys_, keys_, sizeof(keys_));
}

void Input::Update() {
    // キーボード
    memcpy(prevKeys_, keys_, sizeof(keys_));
    GetKeyboardState(keys_);

    // ゲームパッド
    prevJoyState_ = joyState_;
    DWORD result = XInputGetState(0, &joyState_);
    if (result == ERROR_SUCCESS) {
        isJoyValid_ = true;
    } else {
        isJoyValid_ = false;
    }
}

bool Input::IsKeyPressed(uint8_t key) const {
    // ★修正点: 0x80 とのANDで、現在押されている & 前回押されていないかを見る
    return (keys_[key] & 0x80) && !(prevKeys_[key] & 0x80);
}

bool Input::IsKeyDown(uint8_t key) const {
    // ★修正点: 0x80 とのANDで、キーが押されているかを見る
    return (keys_[key] & 0x80);
}

bool Input::IsKeyReleased(uint8_t key) const {
    // ★修正点: 0x80 とのANDで、現在押されていない & 前回押されていたかを見る
    return !(keys_[key] & 0x80) && (prevKeys_[key] & 0x80);
}

bool Input::GetJoyState(XINPUT_STATE& out) const {
    if (isJoyValid_) {
        out = joyState_;
        return true;
    }
    return false;
}

bool Input::GetJoyState(int stickNo, XINPUT_STATE& out) const {
    if (isJoyValid_) {
        DWORD result = XInputGetState(stickNo, &out);
        if (result == ERROR_SUCCESS) {
            return true;
        }
    }
    return false;
}