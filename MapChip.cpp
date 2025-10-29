#include "MapChip.h"
#include "DirectXCommon.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <Windows.h>
#include <string>

// マップチップ1つのサイズ
// 🔽🔽🔽 **ここを 0.8f から 0.6f に変更** 🔽🔽🔽
const float MapChip::kBlockSize = 0.7f;
// 🔼🔼🔼 ********************************** 🔼🔼🔼

// デストラクタの実装
MapChip::~MapChip() {
    // 確保したモデルをすべて解放する
    for (Model* model : models_) {
        delete model;
    }
    models_.clear();
}

void MapChip::Initialize() {
    // 処理を Load に移動したため、ここは空
}

void MapChip::Load(const std::string& filePath, ID3D12Device* device) {
    // 確保済みのモデルを解放
    for (Model* model : models_) {
        delete model;
    }
    models_.clear();
    data_.clear();

    // map.csv ファイルを開く
    std::ifstream file(filePath);
    assert(file.is_open() && "FAIL: map file could not be opened.");

    // map.csv から data_ に読み込み
    std::string line;
    while (std::getline(file, line)) {
        std::vector<int> row;
        std::string cell;
        std::stringstream ss(line);
        while (std::getline(ss, cell, ',')) {
            if (cell.empty()) {
                row.push_back(0);
            } else {
                row.push_back(std::stoi(cell));
            }
        }
        data_.push_back(row);
    }
    file.close();

    // 読み込んだデータに基づいてモデルを生成・配置する
    Transform transform;

    // スケールは kBlockSize をそのまま使う (自動的に 0.6f になる)
    transform.scale = { kBlockSize, kBlockSize, kBlockSize };

    transform.rotate = { 0, 0, 0 };

    for (size_t y = 0; y < data_.size(); ++y) {
        for (size_t x = 0; x < data_[y].size(); ++x) {

            if (data_[y][x] == 1) { // 1ならブロックのモデルを生成

                Model* model = Model::Create("Resources/block", "block.obj", device);

                // 座標を設定 (kBlockSize が 0.6f になっているので自動的に調整される)
                float worldY = (static_cast<float>(data_.size() - 1) - static_cast<float>(y)) * kBlockSize;
                float worldX = static_cast<float>(x) * kBlockSize;
                transform.translate = { worldX + kBlockSize / 2.0f, worldY + kBlockSize / 2.0f, 0.0f };

                model->transform = transform;
                models_.push_back(model);
            }
        }
    }
}

void MapChip::Draw(
    ID3D12GraphicsCommandList* commandList,
    const Matrix4x4& viewProjectionMatrix,
    D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {

    // すべてのモデルを描画する
    for (Model* model : models_) {
        model->Draw(commandList, viewProjectionMatrix, lightGpuAddress, textureSrvHandle);
    }
}

bool MapChip::CheckCollision(const Vector3& worldPos) {
    // (変更なし。kBlockSize が 0.6f になっているので自動的に当たり判定も縮小される)
    if (data_.empty()) {
        return false;
    }
    int x = static_cast<int>(floor(worldPos.x / kBlockSize));
    int y = static_cast<int>(floor(worldPos.y / kBlockSize));
    int mapY = (static_cast<int>(data_.size() - 1)) - y;

    if (x < 0 || x >= static_cast<int>(data_[0].size()) || mapY < 0 || mapY >= static_cast<int>(data_.size())) {
        return true;
    }
    return data_[mapY][x] == 1;
}