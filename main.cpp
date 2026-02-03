#define _USE_MATH_DEFINES
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <wrl.h>
#include <Windows.h>
#include <objbase.h>
#include <d3d12.h>
#include <dbghelp.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <strsafe.h>
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include <xaudio2.h>

// ライブラリリンク
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

#include "WinApp.h"
#include "DirectXCommon.h"
#include "GraphicsPipeline.h"
#include "D3D12Util.h"
#include "Model.h"
#include "MathUtil.h"
#include "DataTypes.h"
#include "Input.h"
#include "Player.h"
#include "MapChip.h"
#include "Camera.h"
#include "Trap.h"
#include "FallingBlock.h"

// =========================================================================
// ▼ ヘルパー関数群
// =========================================================================

void Log(std::ostream& os, const std::string& message) {
    os << message << std::endl;
    OutputDebugStringA(message.c_str());
}

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
    SYSTEMTIME time;
    GetLocalTime(&time);
    wchar_t filePath[MAX_PATH] = { 0 };
    CreateDirectory(L"./Dumps", nullptr);
    StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
    HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
    minidumpInformation.ThreadId = GetCurrentThreadId();
    minidumpInformation.ExceptionPointers = exception;
    minidumpInformation.ClientPointers = TRUE;
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFileHandle,
        MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
    return EXCEPTION_EXECUTE_HANDLER;
}

std::wstring ConvertString(const std::string& str) {
    if (str.empty()) { return std::wstring(); }
    auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
    if (sizeNeeded == 0) { return std::wstring(); }
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
    return result;
}

std::string ConvertString(const std::wstring& str) {
    if (str.empty()) { return std::string(); }
    auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
    if (sizeNeeded == 0) { return std::string(); }
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
    return result;
}

struct D3DResourceLeakChecker {
    ~D3DResourceLeakChecker() {
        Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
            debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
            debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
            debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
        }
    }
};

struct SoundData {
    WAVEFORMATEX wfex;
    std::vector<BYTE> pBuffer;
    unsigned int bufferSize;
};

SoundData LoadWave(const std::string& filename) {
    SoundData soundData = {};
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        Log(std::cout, "[ERROR] Sound file not found: " + filename);
        return soundData;
    }

    struct RiffHeader {
        char chunkId[4]; // "RIFF"
        uint32_t chunkSize;
        char format[4];  // "WAVE"
    } riffHeader;

    file.read(reinterpret_cast<char*>(&riffHeader), sizeof(RiffHeader));
    if (strncmp(riffHeader.chunkId, "RIFF", 4) != 0 || strncmp(riffHeader.format, "WAVE", 4) != 0) {
        Log(std::cout, "[ERROR] Invalid WAV file: " + filename);
        return soundData;
    }

    struct ChunkHeader {
        char id[4];
        uint32_t size;
    } chunkHeader;

    while (file.read(reinterpret_cast<char*>(&chunkHeader), sizeof(ChunkHeader))) {
        if (strncmp(chunkHeader.id, "fmt ", 4) == 0) {
            file.read(reinterpret_cast<char*>(&soundData.wfex), sizeof(WAVEFORMATEX) < chunkHeader.size ? sizeof(WAVEFORMATEX) : chunkHeader.size);
            if (chunkHeader.size > sizeof(WAVEFORMATEX)) {
                file.seekg(chunkHeader.size - sizeof(WAVEFORMATEX), std::ios::cur);
            }
        } else if (strncmp(chunkHeader.id, "data", 4) == 0) {
            soundData.pBuffer.resize(chunkHeader.size);
            file.read(reinterpret_cast<char*>(soundData.pBuffer.data()), chunkHeader.size);
            soundData.bufferSize = chunkHeader.size;
            break;
        } else {
            file.seekg(chunkHeader.size, std::ios::cur);
        }
    }

    Log(std::cout, "[OK] Sound Loaded: " + filename);
    return soundData;
}

const std::string kSaveFilePath = "save_data.txt";

void SaveProgress(const std::string& mapName, const Vector3& respawnPos) {
    std::ofstream ofs(kSaveFilePath);
    if (ofs) {
        ofs << mapName << std::endl;
        ofs << respawnPos.x << " " << respawnPos.y << " " << respawnPos.z << std::endl;
    }
}

bool LoadProgress(std::string& outMapName, Vector3& outRespawnPos) {
    std::ifstream ifs(kSaveFilePath);
    if (ifs) {
        if (std::getline(ifs, outMapName)) {
            ifs >> outRespawnPos.x >> outRespawnPos.y >> outRespawnPos.z;
            return true;
        }
    }
    return false;
}

void DeleteSave() {
    if (std::filesystem::exists(kSaveFilePath)) {
        std::filesystem::remove(kSaveFilePath);
    }
}

enum class GameScene {
    Title,
    GamePlay,
    GameOver,
    GameClear
};

// =========================================================================
// ▼ メイン関数
// =========================================================================

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    D3DResourceLeakChecker leakChecker;
    WinApp* winApp = WinApp::GetInstance();
    winApp->Initialize();
    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    dxCommon->Initialize(winApp);
    Input::GetInstance()->Initialize();
    CoInitializeEx(0, COINIT_MULTITHREADED);
    SetUnhandledExceptionFilter(ExportDump);

    // Audio
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice;
    XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    xAudio2->CreateMasteringVoice(&masterVoice);

    SoundData bgmData = LoadWave("Resources/music.wav");
    IXAudio2SourceVoice* bgmSourceVoice = nullptr;

    if (bgmData.bufferSize > 0) {
        HRESULT hr = xAudio2->CreateSourceVoice(&bgmSourceVoice, &bgmData.wfex);
        if (SUCCEEDED(hr)) {
            XAUDIO2_BUFFER buf = {};
            buf.pAudioData = bgmData.pBuffer.data();
            buf.AudioBytes = bgmData.bufferSize;
            buf.Flags = XAUDIO2_END_OF_STREAM;
            buf.LoopCount = XAUDIO2_LOOP_INFINITE;
            bgmSourceVoice->SubmitSourceBuffer(&buf);
            bgmSourceVoice->Start();
        }
    }

    ID3D12Device* device = dxCommon->GetDevice();
    GraphicsPipeline* graphicsPipeline = new GraphicsPipeline();
    graphicsPipeline->Initialize(device);
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    // --- ゲームプレイ用リソースポインタ ---
    MapChip* mapChip = nullptr;
    Model* playerModel = nullptr;
    Player* player = nullptr;
    Model* goalModel_ = nullptr;
    std::vector<Trap*> traps_;
    std::vector<FallingBlock*> fallingBlocks_;

    // --- シーン用モデルポインタ ---
    Model* titleModel = nullptr;
    Model* gameOverModel = nullptr;
    Model* gameClearModel = nullptr;

    Model* skydomeModel = nullptr;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResources;

    // --- 共通リソースの読み込み ---
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
    const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    auto LoadAndCreateTextureSRV = [&](const std::string& path, uint32_t heapIndex) -> Microsoft::WRL::ComPtr<ID3D12Resource> {
        DirectX::ScratchImage mipImages = LoadTexture(path);
        const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

        if (metadata.width == 0) {
            Log(std::cout, "[ERROR] Texture Load Failed (File Not Found or Invalid): " + path);
            return nullptr;
        }

        Microsoft::WRL::ComPtr<ID3D12Resource> resource = CreateTextureResource(device, metadata);
        if (!resource) {
            Log(std::cout, "[ERROR] Failed to CreateTextureResource for: " + path);
            return nullptr;
        }

        Microsoft::WRL::ComPtr<ID3D12Resource> intermediate = UploadTextureData(resource.Get(), mipImages, device, commandList);
        if (intermediate) {
            intermediateResources.push_back(intermediate);
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = metadata.format;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

        D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, heapIndex);
        device->CreateShaderResourceView(resource.Get(), &srvDesc, handleCPU);

        Log(std::cout, "[OK] Texture Loaded: " + path);
        return resource;
        };

    // =========================================================================
    // テクスチャ読み込み
    // =========================================================================
    Microsoft::WRL::ComPtr<ID3D12Resource> playerTextureResource = LoadAndCreateTextureSRV("Resources/player/player.png", 2);
    D3D12_GPU_DESCRIPTOR_HANDLE playerTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);

    Microsoft::WRL::ComPtr<ID3D12Resource> blockTextureResource = LoadAndCreateTextureSRV("Resources/block/block.png", 3);
    D3D12_GPU_DESCRIPTOR_HANDLE blockTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 3);

    Microsoft::WRL::ComPtr<ID3D12Resource> cubeTextureResource = LoadAndCreateTextureSRV("Resources/cube/cube.jpg", 4);
    D3D12_GPU_DESCRIPTOR_HANDLE cubeTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 4);

    Microsoft::WRL::ComPtr<ID3D12Resource> titleTextureResource = LoadAndCreateTextureSRV("Resources/Title/Title.png", 5);
    D3D12_GPU_DESCRIPTOR_HANDLE titleTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 5);

    Microsoft::WRL::ComPtr<ID3D12Resource> gameOverTextureResource = LoadAndCreateTextureSRV("Resources/GameOver/GameOver.png", 6);
    D3D12_GPU_DESCRIPTOR_HANDLE gameOverTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 6);

    Microsoft::WRL::ComPtr<ID3D12Resource> gameClearTextureResource = LoadAndCreateTextureSRV("Resources/Clear/Clear.png", 7);
    D3D12_GPU_DESCRIPTOR_HANDLE gameClearTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 7);

    Microsoft::WRL::ComPtr<ID3D12Resource> skydomeTextureResource = LoadAndCreateTextureSRV("Resources/skydome/sky_sphere.png", 8);
    D3D12_GPU_DESCRIPTOR_HANDLE skydomeTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 8);

    Microsoft::WRL::ComPtr<ID3D12Resource> trapTextureResource = LoadAndCreateTextureSRV("Resources/Trap/Trap.png", 9);
    D3D12_GPU_DESCRIPTOR_HANDLE trapTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 9);

    Microsoft::WRL::ComPtr<ID3D12Resource> flagTextureResource = LoadAndCreateTextureSRV("Resources/flag.png", 10);
    D3D12_GPU_DESCRIPTOR_HANDLE flagTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 10);


    // =========================================================================
    // シーン用モデル生成
    // =========================================================================

    titleModel = Model::Create("Resources/Title", "Title.obj", device);
    if (titleModel) {
        titleModel->transform.translate = { 9.2f, 11.0f, 30.0f };
        titleModel->transform.scale = { 1.0f, 1.0f, 1.0f };
    }

    gameOverModel = Model::Create("Resources/GameOver", "GameOver.obj", device);
    if (gameOverModel) {
        gameOverModel->transform.translate = { 9.2f, 11.0f, 30.0f };
        gameOverModel->transform.scale = { 1.0f, 1.0f, 1.0f };
    }

    gameClearModel = Model::Create("Resources/Clear", "Clear.obj", device);
    if (gameClearModel) {
        gameClearModel->transform.translate = { 9.2f, 10.0f, 40.0f };
        gameClearModel->transform.scale = { 1.0f, 1.0f, 1.0f };
    }

    skydomeModel = Model::Create("Resources/skydome", "skydome.obj", device);
    if (skydomeModel) {
        skydomeModel->transform.scale = { 5000.0f, 5000.0f, 5000.0f };
        skydomeModel->transform.translate = { 0.0f, 0.0f, 0.0f };
        skydomeModel->transform.rotate = { 0.0f, 0.0f, 0.0f };
    }


    // --- ライト・カメラ (常駐) ---
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
    DirectionalLight* directionalLightData = nullptr;
    directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
    directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData->direction = Normalize({ 0.0f, -1.0f, 0.0f });
    directionalLightData->intensity = 1.0f;

    Camera* camera = new Camera();
    camera->Initialize();

    Microsoft::WRL::ComPtr<ID3D12Resource> cameraForGpuResource = CreateBufferResource(device, sizeof(CameraForGpu));
    CameraForGpu* cameraForGpuData = nullptr;
    cameraForGpuResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraForGpuData));

    // --- シーン管理用変数 ---
    GameScene currentScene = GameScene::Title;
    bool isGameInitialized = false;
    bool isLoadingNextMap = false;

    std::string currentMapFilePath = "Resources/map.csv";
    Vector3 currentRespawnPos = { 0.0f, 0.0f, 0.0f };
    std::string nextMapFilePath = "";
    Vector3 nextRespawnPos = { 0.0f, 0.0f, 0.0f };

    // ★ Map3用変数
    float map3Timer = 0.0f;
    bool map3EventTriggered = false;
    Vector3 block6Pos = { 9999.0f, 0.0f, 0.0f }; // 6番ブロックの位置

    // --- ゲームリソース解放用ラムダ ---
    auto cleanupGameResources = [&]() {
        delete mapChip; mapChip = nullptr;
        delete player; player = nullptr;
        delete playerModel; playerModel = nullptr;
        delete goalModel_; goalModel_ = nullptr;
        for (Trap* trap : traps_) delete trap; traps_.clear();
        for (FallingBlock* block : fallingBlocks_) delete block; fallingBlocks_.clear();
        isGameInitialized = false;
        };

    // ========== メインループ ==========
    while (!winApp->IsEndRequested()) {
        winApp->ProcessMessage();
        Input::GetInstance()->Update();
        Input* input = Input::GetInstance();

        // --- シーン処理 (ロジック更新のみ) ---
        switch (currentScene) {
        case GameScene::Title:
            if (input->IsKeyPressed(VK_SPACE)) {
                if (currentMapFilePath.empty()) {
                    currentMapFilePath = "Resources/map.csv";
                    currentRespawnPos = { 0.0f, 0.0f, 0.0f };
                }
                currentScene = GameScene::GamePlay;
            }
            break;

        case GameScene::GamePlay:
            if (!isGameInitialized) {
                mapChip = new MapChip();
                playerModel = Model::Create("Resources/player", "player.obj", device);
                player = new Player();

                mapChip->Load(currentMapFilePath, device);
                player->Initialize(playerModel, mapChip, device);

                if (currentRespawnPos.x != 0.0f || currentRespawnPos.y != 0.0f) {
                    player->SetPosition(currentRespawnPos);
                }

                if (currentMapFilePath == "Resources/map.csv") {
                    size_t mapHeight = 15;
                    auto csvYToWorldY = [&](int csvY) { return (static_cast<float>(mapHeight - 1) - static_cast<float>(csvY)) * MapChip::kBlockSize + (MapChip::kBlockSize / 2.0f); };
                    float stopMarginNormal = MapChip::kBlockSize * 1.0f;
                    float stopMarginShort = MapChip::kBlockSize * 0.2f;

                    traps_.push_back(new Trap()); traps_.back()->Initialize(device, csvYToWorldY(4), Trap::AttackSide::FromLeft, stopMarginNormal);
                    traps_.push_back(new Trap()); traps_.back()->Initialize(device, csvYToWorldY(5), Trap::AttackSide::FromLeft, stopMarginNormal);
                    traps_.push_back(new Trap()); traps_.back()->Initialize(device, csvYToWorldY(6), Trap::AttackSide::FromLeft, stopMarginNormal);
                    traps_.push_back(new Trap()); traps_.back()->Initialize(device, csvYToWorldY(8), Trap::AttackSide::FromRight, stopMarginNormal);
                    traps_.push_back(new Trap()); traps_.back()->Initialize(device, csvYToWorldY(9), Trap::AttackSide::FromRight, stopMarginNormal);
                    traps_.push_back(new Trap()); traps_.back()->Initialize(device, csvYToWorldY(10), Trap::AttackSide::FromRight, stopMarginNormal);
                    traps_.push_back(new Trap()); traps_.back()->Initialize(device, csvYToWorldY(12), Trap::AttackSide::FromRight, stopMarginShort);
                    traps_.push_back(new Trap()); traps_.back()->Initialize(device, csvYToWorldY(13), Trap::AttackSide::FromRight, stopMarginShort);
                }

                const auto& dynamicBlocks = mapChip->GetDynamicBlocks();
                for (const auto& data : dynamicBlocks) {
                    FallingBlock* newBlock = new FallingBlock();
                    newBlock->Initialize(device, data.position, static_cast<BlockType>(data.type));
                    fallingBlocks_.push_back(newBlock);
                }

                // ★ Map3の初期設定
                if (currentMapFilePath == "Resources/map3.csv") {
                    map3Timer = 0.0f;
                    map3EventTriggered = false;
                    // 6番(RiseOnTop)ブロックの場所を探す
                    int gx, gy;
                    if (mapChip->FindBlock(6, gx, gy)) {
                        block6Pos = mapChip->GetWorldPosFromGrid(gx, gy);
                    } else {
                        block6Pos = { 9999.0f, 0.0f, 0.0f };
                    }
                }

                if (mapChip->HasGoal()) {
                    goalModel_ = Model::Create("Resources", "flag.obj", device);
                    if (goalModel_) {
                        goalModel_->transform.scale = { MapChip::kBlockSize, MapChip::kBlockSize, MapChip::kBlockSize };
                        goalModel_->transform.rotate = { 0.0f, 0.0f, 0.0f };
                        Vector3 goalPos = mapChip->GetGoalPosition();
                        goalPos.y -= MapChip::kBlockSize * 0.5f;
                        goalModel_->transform.translate = goalPos;
                    }
                }
                isLoadingNextMap = false;
                isGameInitialized = true;
            }

            if (!isLoadingNextMap && isGameInitialized) {
                // 1. プレイヤーの更新
                player->Update();

                // 2. 奈落（画面外）の死亡判定
                if (player->IsAlive() && player->GetPosition().y < -5.0f) {
                    player->Die();
                }

                // 3. ギミック・エネミーの更新 (死亡中も動かす)
                for (Trap* trap : traps_) trap->Update(player);
                for (FallingBlock* block : fallingBlocks_) block->Update(player, mapChip);

                // ★ Map3専用ギミック：6を飛び越える or 20秒経過で壁生成＆ゴール移動
                if (currentMapFilePath == "Resources/map3.csv" && !map3EventTriggered) {
                    map3Timer += 1.0f / 60.0f;

                    bool timerCondition = (map3Timer >= 20.0f);
                    // プレイヤーのX座標がブロック6より右に行った(飛び越えた)
                    bool jumpCondition = (block6Pos.x < 9000.0f && player->GetPosition().x > block6Pos.x + MapChip::kBlockSize);

                    if (timerCondition || jumpCondition) {
                        map3EventTriggered = true;

                        // 1. ゴールを反対側(左側)に移動
                        float goalX = MapChip::kBlockSize * 1.5f;
                        float goalY = MapChip::kBlockSize * 1.5f; // 床高さに応じて調整
                        Vector3 newGoalPos = { goalX, goalY, 0.0f };
                        mapChip->SetGoalPosition(newGoalPos);
                        if (goalModel_) {
                            goalModel_->transform.translate = newGoalPos;
                            goalModel_->transform.translate.y -= MapChip::kBlockSize * 0.5f;
                        }

                        // 2. 真ん中に9(StaticHazard)を縦に並べる
                        // マップ幅20なら真ん中は x=10
                        int midX = 10;
                        size_t rows = mapChip->GetRowCount();
                        for (int y = 0; y < rows; ++y) {
                            // 天井と床を残して壁を作る(あるいは全埋め)
                            // 9番のFallingBlockを生成
                            Vector3 spawnPos = mapChip->GetWorldPosFromGrid(midX, y);
                            FallingBlock* newWall = new FallingBlock();
                            newWall->Initialize(device, spawnPos, BlockType::StaticHazard);
                            fallingBlocks_.push_back(newWall);
                        }
                    }
                }

                // 4. 死亡後のリトライ受付
                if (!player->IsAlive()) {
                    if (input->IsKeyPressed(VK_SPACE)) {
                        cleanupGameResources();
                        goto end_of_update;
                    }
                }

                // 5. マップ移動・クリア判定 (生存中のみ)
                if (player->IsAlive()) {
                    if (currentMapFilePath == "Resources/map.csv") {
                        if (player->IsExiting()) {
                            isLoadingNextMap = true;
                            nextMapFilePath = "Resources/map2.csv";
                            size_t map2Height = 15;
                            float spawnY = (static_cast<float>(map2Height - 1) - 14.0f) * MapChip::kBlockSize + (MapChip::kBlockSize / 2.0f);
                            float spawnX = (MapChip::kBlockSize / 2.0f);
                            nextRespawnPos = { spawnX, spawnY, 0.0f };
                        }
                    } else if (currentMapFilePath == "Resources/map2.csv") {
                        Vector3 pPos = player->GetPosition();
                        float mapTopY = static_cast<float>(mapChip->GetRowCount()) * MapChip::kBlockSize;
                        if (pPos.x < MapChip::kBlockSize * 2.0f && pPos.y > mapTopY - (MapChip::kBlockSize * 2.0f)) {
                            isLoadingNextMap = true;
                            nextMapFilePath = "Resources/map3.csv";
                            float spawnX = MapChip::kBlockSize * 3.5f;
                            float spawnY = MapChip::kBlockSize * 1.5f;
                            nextRespawnPos = { spawnX, spawnY, 0.0f };
                        }
                    } else if (currentMapFilePath == "Resources/map3.csv") {
                        if (mapChip->HasGoal() && mapChip->CheckGoalCollision(player->GetPosition(), player->GetHalfSize())) {
                            currentScene = GameScene::GameClear;
                            currentMapFilePath = "Resources/map.csv";
                            currentRespawnPos = { 0.0f, 0.0f, 0.0f };
                        }
                    }
                }
            }

            if (isLoadingNextMap) {
                for (Trap* trap : traps_) delete trap; traps_.clear();
                for (FallingBlock* block : fallingBlocks_) delete block; fallingBlocks_.clear();
                delete goalModel_; goalModel_ = nullptr;

                currentMapFilePath = nextMapFilePath;
                currentRespawnPos = nextRespawnPos;
                mapChip->Load(currentMapFilePath, device);
                player->SetPosition(currentRespawnPos);

                const auto& dynamicBlocks2 = mapChip->GetDynamicBlocks();
                for (const auto& data : dynamicBlocks2) {
                    FallingBlock* newBlock = new FallingBlock();
                    newBlock->Initialize(device, data.position, static_cast<BlockType>(data.type));
                    fallingBlocks_.push_back(newBlock);
                }
                if (mapChip->HasGoal()) {
                    goalModel_ = Model::Create("Resources", "flag.obj", device);
                    if (goalModel_) {
                        goalModel_->transform.scale = { MapChip::kBlockSize, MapChip::kBlockSize, MapChip::kBlockSize };
                        goalModel_->transform.rotate = { 0.0f, 0.0f, 0.0f };
                        Vector3 goalPos = mapChip->GetGoalPosition();
                        goalPos.y -= MapChip::kBlockSize * 0.5f;
                        goalModel_->transform.translate = goalPos;
                    }
                }
                isLoadingNextMap = false;
            }
            break;

        case GameScene::GameOver:
            if (input->IsKeyPressed(VK_SPACE)) {
                cleanupGameResources();
                currentScene = GameScene::GamePlay;
            }
            break;

        case GameScene::GameClear:
            if (input->IsKeyPressed(VK_SPACE)) {
                cleanupGameResources();
                currentScene = GameScene::Title;
            }
            break;
        }

    end_of_update:

        // --- 描画開始 ---
        dxCommon->PreDraw();

        commandList->SetGraphicsRootSignature(graphicsPipeline->GetRootSignature());
        commandList->SetPipelineState(graphicsPipeline->GetPipelineState(kBlendModeNone));
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        const Matrix4x4& viewProjectionMatrix = camera->GetViewProjectionMatrix();
        cameraForGpuData->worldPosition = camera->GetTransform().translate;
        directionalLightData->direction = Normalize(directionalLightData->direction);
        commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(4, cameraForGpuResource->GetGPUVirtualAddress());

        ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
        commandList->SetDescriptorHeaps(1, descriptorHeaps);

        // --- 描画コマンド発行 ---

        if (skydomeModel && skydomeTextureResource) {
            skydomeModel->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), skydomeTextureSrvHandleGPU);
        }

        if (currentScene == GameScene::Title) {
            if (titleModel && titleTextureResource) {
                titleModel->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), titleTextureSrvHandleGPU);
            }
        } else if (currentScene == GameScene::GameClear) {
            if (gameClearModel && gameClearTextureResource) {
                gameClearModel->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), gameClearTextureSrvHandleGPU);
            }
        } else if (currentScene == GameScene::GamePlay && isGameInitialized && player != nullptr) {
            // 背景を描画
            if (blockTextureResource) mapChip->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), blockTextureSrvHandleGPU);
            if (playerTextureResource) player->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), playerTextureSrvHandleGPU);
            if (cubeTextureResource) {
                for (Trap* trap : traps_) trap->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), cubeTextureSrvHandleGPU);
                if (goalModel_) goalModel_->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), flagTextureSrvHandleGPU);
            }
            if (trapTextureResource) {
                for (FallingBlock* block : fallingBlocks_) {
                    block->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), trapTextureSrvHandleGPU);
                }
            }

            // ★ 死亡演出：GameOverを最前面に描画
            if (!player->IsAlive()) {
                dxCommon->ClearDepthBuffer();
                if (gameOverModel && gameOverTextureResource) {
                    gameOverModel->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), gameOverTextureSrvHandleGPU);
                }
            }
        }

        dxCommon->PostDraw();
    }

    if (bgmSourceVoice) {
        bgmSourceVoice->DestroyVoice();
    }

    if (isGameInitialized) cleanupGameResources();

    delete titleModel; delete gameOverModel; delete gameClearModel;
    delete skydomeModel;
    delete graphicsPipeline; delete camera;

    dxCommon->Finalize();
    CoUninitialize();
    winApp->Finalize();

    return 0;
}