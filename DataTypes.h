#pragma once
#include "MathTypes.h"
#include <string>
#include <vector>

// 頂点データ
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

// マテリアル
struct Material {
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
};

// 座標変換行列
struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
	// ★修正点: カメラ座標をここから削除
};

// ★修正点: カメラ専用の構造体を追加
struct CameraForGpu {
	Vector3 worldPosition;
};

// 平行光源
struct DirectionalLight {
	Vector4 color;
	Vector3 direction;
	float intensity;
};

// モデルのマテリアル情報
struct MaterialData {
	std::string textureFilePath;
};

// モデルデータ
struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};