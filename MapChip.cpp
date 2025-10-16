#include "MapChip.h"
#include "DirectXCommon.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <Windows.h>
#include <string>

// マップチップ1つのサイズ
const float MapChip::kBlockSize = 1.0f;

void MapChip::Initialize() {
    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
    // ブロック用に "resources/block/block.obj" を読み込む
    model_ = Model::Create("resources/block", "block.obj", device);
}

void MapChip::Load(const std::string& filePath) {
    data_.clear();
    std::ifstream file(filePath);
    assert(file.is_open() && "FAIL: map file could not be opened.");

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
}

void MapChip::Draw(
    ID3D12GraphicsCommandList* commandList,
    const Matrix4x4& viewProjectionMatrix,
    D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress,
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {

    if (data_.empty()) {
        return;
    }

    Transform transform;
    transform.scale = { kBlockSize / 2.0f, kBlockSize / 2.0f, kBlockSize / 2.0f };
    transform.rotate = { 0, 0, 0 };

    for (size_t y = 0; y < data_.size(); ++y) {
        for (size_t x = 0; x < data_[y].size(); ++x) {
            if (data_[y][x] == 1) { // 1ならブロックを描画
                float worldY = (static_cast<float>(data_.size() - 1) - static_cast<float>(y)) * kBlockSize;
                float worldX = static_cast<float>(x) * kBlockSize;
                transform.translate = { worldX + kBlockSize / 2.0f, worldY + kBlockSize / 2.0f, 0.0f };
                model_->transform = transform;
                model_->Draw(commandList, viewProjectionMatrix, lightGpuAddress, textureSrvHandle);
            }
        }
    }
}

bool MapChip::CheckCollision(const Vector3& worldPos) {
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