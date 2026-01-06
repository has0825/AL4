#include "MapChip.h"
#include "DirectXCommon.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <Windows.h>
#include <string>

// マップチップ1つのサイズ
const float MapChip::kBlockSize = 0.7f;

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

    dynamicBlocks_.clear();
    hasGoal_ = false;

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
                int cellValue = std::stoi(cell);
                row.push_back(cellValue);
            }
        }
        data_.push_back(row);
    }
    file.close();

    // 読み込んだデータに基づいてモデルを生成・配置する
    Transform transform;
    transform.scale = { kBlockSize, kBlockSize, kBlockSize };
    transform.rotate = { 0, 0, 0 };

    for (size_t y = 0; y < data_.size(); ++y) {
        for (size_t x = 0; x < data_[y].size(); ++x) {

            int cellValue = data_[y][x];

            // (x, y) からワールド座標を計算
            float worldY = (static_cast<float>(data_.size() - 1) - static_cast<float>(y)) * kBlockSize;
            float worldX = static_cast<float>(x) * kBlockSize;
            Vector3 pos = { worldX + kBlockSize / 2.0f, worldY + kBlockSize / 2.0f, 0.0f };

            if (cellValue == 1) {
                // 1なら静的ブロックのモデルを生成
                Model* model = Model::Create("Resources/block", "block.obj", device);
                transform.translate = pos;
                model->transform = transform;
                models_.push_back(model);
            } else if (cellValue == 3 || cellValue == 4 || cellValue == 6 || cellValue == 7 || cellValue == 8) {
                // 3, 4, 6, 7, 8 は動的ブロックとして登録
                dynamicBlocks_.push_back({ pos, cellValue });
            } else if (cellValue == 5) {
                // 5 (ゴール) の位置を記録
                goalPos_ = pos;
                hasGoal_ = true;
            }
        }
    }
}

void MapChip::Draw(
    ID3D12GraphicsCommandList* commandList,
    const Matrix4x4& viewProjectionMatrix,
    D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {

    // 静的なブロック (1) を描画
    for (Model* model : models_) {
        model->Draw(commandList, viewProjectionMatrix, lightGpuAddress, textureSrvHandle);
    }
}

bool MapChip::CheckCollision(const Vector3& worldPos) {
    if (data_.empty()) {
        return false;
    }
    int x = static_cast<int>(floor(worldPos.x / kBlockSize));
    int y = static_cast<int>(floor(worldPos.y / kBlockSize));
    int mapY = (static_cast<int>(data_.size() - 1)) - y;

    // 1. マップの「左端」か「上端」に出た場合、壁とみなす (true)
    if (x < 0 || mapY < 0) {
        return true;
    }

    // 2. マップの「右端」か「下端」に出た場合、空間とみなす (false)
    if (x >= static_cast<int>(data_[0].size()) || mapY >= static_cast<int>(data_.size())) {
        return false;
    }

    // 3. マップの内側の場合、data_ を参照する
    return data_[mapY][x] == 1;
}

bool MapChip::CheckGoalCollision(const Vector3& playerPos, float playerHalfSize) const {
    // ゴールがマップになければ
    if (!hasGoal_) {
        return false;
    }

    // プレイヤーのAABB
    float pLeft = playerPos.x - playerHalfSize;
    float pRight = playerPos.x + playerHalfSize;
    float pTop = playerPos.y + playerHalfSize;
    float pBottom = playerPos.y - playerHalfSize;

    // ゴールのAABB
    float halfSize = kBlockSize / 2.0f;
    float gLeft = goalPos_.x - halfSize;
    float gRight = goalPos_.x + halfSize;
    float gTop = goalPos_.y + halfSize;
    float gBottom = goalPos_.y - halfSize;

    // AABB 衝突判定
    if (pLeft > gRight || pRight < gLeft || pTop < gBottom || pBottom > gTop) {
        return false; // 衝突していない
    }

    return true; // 衝突している
}

void MapChip::GetGridCoordinates(const Vector3& worldPos, int& outX, int& outMapY) const {
    if (data_.empty()) {
        outX = -1;
        outMapY = -1;
        return;
    }
    outX = static_cast<int>(floor(worldPos.x / kBlockSize));
    int y = static_cast<int>(floor(worldPos.y / kBlockSize));
    outMapY = (static_cast<int>(data_.size() - 1)) - y;

    // 境界チェック
    if (outX < 0 || outX >= static_cast<int>(data_[0].size()) || outMapY < 0 || outMapY >= static_cast<int>(data_.size())) {
        outX = -1;
        outMapY = -1;
    }
}

void MapChip::SetGridCell(int x, int mapY, int value) {
    if (data_.empty()) { return; }

    // 境界チェック
    if (x < 0 || x >= static_cast<int>(data_[0].size()) || mapY < 0 || mapY >= static_cast<int>(data_.size())) {
        return;
    }
    // 値を上書き
    data_[mapY][x] = value;
}