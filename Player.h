#pragma once
#include "Model.h"
#include "Input.h"
#include "externals/imgui/imgui.h"

class Player {
public:
    void Initialize(Model* model);
    void Update();
    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const Matrix4x4& viewProjectionMatrix,
        D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle);
    void ImGui_Draw();

private:
    Model* model_ = nullptr;
    Transform transform_{};
    Vector3 velocity_{};
    int jumpCount_ = 0;
};