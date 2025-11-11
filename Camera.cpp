// Camera.cpp
#include "Camera.h"
#include "MathUtil.h" // MakeAffineMatrix, Inverse, MakePerspectiveFovMatrix, Multiply
// 🔽🔽🔽 imgui.h のインクルードを削除 🔽🔽🔽
// #include "externals/imgui/imgui.h" 
// 🔼🔼🔼 *************************** 🔼🔼🔼

void Camera::Initialize() {
    // 固定する座標と回転を設定
    transform_.translate = { 9.2f, 5.3f, -21.9f }; 
    // 🔽🔽🔽 X軸回転 (見下ろし角度) を追加 🔽🔽🔽
    transform_.rotate = { 0.0f, 0.0f, 0.0f }; 
    // 🔼🔼🔼 ******************************* 🔼🔼🔼
    transform_.scale = { 1.0f, 1.0f, 1.0f };

    // fovY_, aspectRatio_, nearZ_, farZ_ は初期値を使用

    UpdateMatrix(); // 初期行列を計算
}

void Camera::UpdateMatrix() {
    // カメラ行列を計算
    Matrix4x4 cameraMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    // ビュー行列（カメラ行列の逆行列）
    matView_ = Inverse(cameraMatrix);
    // プロジェクション行列
    matProjection_ = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearZ_, farZ_);
    // ビュープロジェクション行列
    matViewProjection_ = Multiply(matView_, matProjection_);
}

// 🔽🔽🔽 ImGui_Draw 関数の定義を丸ごと削除 🔽🔽🔽
/*
void Camera::ImGui_Draw() {
    ImGui::Begin("Camera");
    // ... (中身) ...
    ImGui::End();
}
*/
// 🔼🔼🔼 ********************************* 🔼🔼🔼