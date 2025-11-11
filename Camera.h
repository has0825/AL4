// Camera.h
#pragma once
#include "MathTypes.h" // Vector3, Matrix4x4
#include "DataTypes.h" // Transform

class Camera {
public:
    void Initialize();
    void UpdateMatrix();

    // 行列を取得
    const Matrix4x4& GetViewMatrix() const { return matView_; }
    const Matrix4x4& GetProjectionMatrix() const { return matProjection_; }
    const Matrix4x4& GetViewProjectionMatrix() const { return matViewProjection_; }

    // Transformを取得 (GPU転送用)
    Transform& GetTransform() { return transform_; }
    const Transform& GetTransform() const { return transform_; }

    // 🔽🔽🔽 ImGui_Draw の宣言を削除 🔽🔽🔽
    // void ImGui_Draw();
    // 🔼🔼🔼 ************************ 🔼🔼🔼

private:
    Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} };
    float fovY_ = 0.45f;
    float aspectRatio_ = 16.0f / 9.0f;
    float nearZ_ = 0.1f;
    float farZ_ = 100.0f;

    Matrix4x4 matView_{};
    Matrix4x4 matProjection_{};
    Matrix4x4 matViewProjection_{};
};