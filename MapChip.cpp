#include "MapChip.h"
#include "DirectXCommon.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <Windows.h>
#include <string>
#include <cmath> 

const float MapChip::kBlockSize = 0.7f;

MapChip::~MapChip() {
    for (Model* model : models_) {
        delete model;
    }
    models_.clear();
}

void MapChip::Initialize() {
}

void MapChip::Load(const std::string& filePath, ID3D12Device* device) {
    for (Model* model : models_) {
        delete model;
    }
    models_.clear();
    data_.clear();
    dynamicBlocks_.clear();
    hasGoal_ = false;

    std::ifstream file(filePath);
    assert(file.is_open() && "FAIL: map file could not be opened.");

    std::string line;
    while (std::getline(file, line)) {
        std::vector<int> row;
        std::string cell;
        std::stringstream ss(line);
        while (std::getline(ss, cell, ',')) {
            row.push_back(std::stoi(cell));
        }
        data_.push_back(row);
    }
    file.close();

    size_t rowCount = data_.size();

    for (size_t y = 0; y < rowCount; ++y) {
        for (size_t x = 0; x < data_[y].size(); ++x) {
            int type = data_[y][x];

            float worldX = x * kBlockSize + kBlockSize / 2.0f;
            float worldY = (static_cast<float>(rowCount - 1) - static_cast<float>(y)) * kBlockSize + kBlockSize / 2.0f;
            Vector3 pos = { worldX, worldY, 0.0f };

            if (type == 1) { // 壁
                Model* block = Model::Create("Resources/block", "block.obj", device);
                block->transform.translate = pos;
                block->transform.scale = { kBlockSize, kBlockSize, kBlockSize };
                models_.push_back(block);
            } else if (type == 2) { // スタート
                startPosition_ = pos;
                data_[y][x] = 0;
            } else if (type == 5) { // ゴール
                goalPos_ = pos;
                hasGoal_ = true;
                data_[y][x] = 0;
            } else if (type >= 3) { // 動的ブロック (3,4,6,7,8,9,10)
                DynamicBlockData d;
                d.position = pos;
                d.type = type;
                dynamicBlocks_.push_back(d);
                data_[y][x] = 0;
            }
        }
    }
}

void MapChip::Draw(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjectionMatrix, D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {
    for (Model* model : models_) {
        model->Draw(commandList, viewProjectionMatrix, lightGpuAddress, textureSrvHandle);
    }
}

bool MapChip::CheckCollision(const Vector3& worldPos) {
    int x = static_cast<int>(floor(worldPos.x / kBlockSize));
    int y = static_cast<int>(floor(worldPos.y / kBlockSize));
    int mapY = (static_cast<int>(data_.size()) - 1) - y;

    if (mapY < 0 || mapY >= data_.size()) return false;
    if (x < 0 || x >= data_[mapY].size()) return false;

    if (data_[mapY][x] == 1) {
        return true;
    }
    return false;
}

bool MapChip::CheckGoalCollision(const Vector3& playerPos, float playerHalfSize) const {
    if (!hasGoal_) return false;

    float pLeft = playerPos.x - playerHalfSize;
    float pRight = playerPos.x + playerHalfSize;
    float pTop = playerPos.y + playerHalfSize;
    float pBottom = playerPos.y - playerHalfSize;

    float halfSize = kBlockSize / 2.0f;
    float gLeft = goalPos_.x - halfSize;
    float gRight = goalPos_.x + halfSize;
    float gTop = goalPos_.y + halfSize;
    float gBottom = goalPos_.y - halfSize;

    if (pLeft > gRight || pRight < gLeft || pTop < gBottom || pBottom > gTop) {
        return false;
    }
    return true;
}

void MapChip::GetGridCoordinates(const Vector3& worldPos, int& outX, int& outMapY) const {
    if (data_.empty()) { outX = -1; outMapY = -1; return; }
    outX = static_cast<int>(floor(worldPos.x / kBlockSize));
    int y = static_cast<int>(floor(worldPos.y / kBlockSize));
    outMapY = (static_cast<int>(data_.size() - 1)) - y;

    if (outMapY < 0 || outMapY >= data_.size() || outX < 0 || outX >= data_[outMapY].size()) {
        outX = -1; outMapY = -1;
    }
}

void MapChip::SetGridCell(int x, int mapY, int value) {
    if (mapY >= 0 && mapY < data_.size()) {
        if (x >= 0 && x < data_[mapY].size()) {
            data_[mapY][x] = value;
        }
    }
}

// ★追加実装: 指定タイプのブロック位置を検索
bool MapChip::FindBlock(int type, int& outGridX, int& outMapY) const {
    // dynamicBlocks_ に格納されている情報から探す
    for (const auto& db : dynamicBlocks_) {
        if (db.type == type) {
            GetGridCoordinates(db.position, outGridX, outMapY);
            return true;
        }
    }
    return false;
}

// ★追加実装: グリッド座標をワールド座標へ変換
Vector3 MapChip::GetWorldPosFromGrid(int gridX, int gridMapY) const {
    size_t rowCount = data_.size();
    float worldX = gridX * kBlockSize + kBlockSize / 2.0f;
    // 配列インデックス(gridMapY) から ワールドYへの変換
    // worldY = (rowCount - 1 - index) * size + size/2
    float worldY = (static_cast<float>(rowCount - 1) - static_cast<float>(gridMapY)) * kBlockSize + kBlockSize / 2.0f;
    return { worldX, worldY, 0.0f };
}