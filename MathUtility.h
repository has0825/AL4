#pragma once
#define _USE_MATH_DEFINES
#include <cmath>

// ベクトル
struct Vector2 {
	float x, y;
};
struct Vector3 {
	float x, y, z;
};
struct Vector4 {
	float x, y, z, w;
};

// 行列
struct Matrix3x3 {
	float m[3][3];
};
struct Matrix4x4 {
	float m[4][4];
};

// 数学関連の関数をまとめたユーティリティクラス
class MathUtility {
public:
	// 単位行列の作成
	static Matrix4x4 MakeIdentity4x4();
	// 拡大縮小行列
	static Matrix4x4 MakeScaleMatrix(const Vector3& s);
	// X軸回転行列
	static Matrix4x4 MakeRotateXMatrix(float radian);
	// Y軸回転行列
	static Matrix4x4 MakeRotateYMatrix(float radian);
	// Z軸回転行列
	static Matrix4x4 MakeRotateZMatrix(float radian);
	// 平行移動行列
	static Matrix4x4 MakeTranslateMatrix(const Vector3& tlanslate);
	// 行列の積
	static Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);
	// アフィン変換行列
	static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
	// 4x4 逆行列
	static Matrix4x4 Inverse(Matrix4x4 m);
	// 透視投影行列
	static Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);
	// 正射影行列
	static Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);
	// ビューポート変換行列
	static Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);
	// ベクトルの正規化
	static Vector3 Normalize(const Vector3& v);
};