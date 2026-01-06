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

// --- ヘルパー関数群 ---
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

void Log(std::ostream& os, const std::string& message) {
    os << message << std::endl;
    OutputDebugStringA(message.c_str());
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

// --- セーブ・ロード機能 ---
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

// --- シーン定義 ---
enum class GameScene {
    Title,      // タイトル画面
    GamePlay,   // ゲームプレイ中
    GameOver,   // ゲームオーバー
    GameClear   // ゲームクリア
};

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

    // スカイドーム用モデルポインタ
    Model* skydomeModel = nullptr;

    // 中間リソース保持リスト
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResources;

    // --- 共通リソースの読み込み ---
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
    const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // =========================================================================
    // ★ 安全なテクスチャ読み込み用ラムダ関数
    // =========================================================================
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

        // 中間リソースをリストに追加して保持
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

    // スカイドーム用テクスチャ (Index: 8)
    Microsoft::WRL::ComPtr<ID3D12Resource> skydomeTextureResource = LoadAndCreateTextureSRV("Resources/skydome/sky_sphere.png", 8);
    D3D12_GPU_DESCRIPTOR_HANDLE skydomeTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 8);


    // =========================================================================
    // シーン用モデル生成
    // =========================================================================

    // Title Model
    titleModel = Model::Create("Resources/Title", "Title.obj", device);
    if (titleModel) {
        titleModel->transform.translate = { 9.2f, 11.0f, 30.0f };
        titleModel->transform.scale = { 1.0f, 1.0f, 1.0f };
    }

    // GameOver Model
    gameOverModel = Model::Create("Resources/GameOver", "GameOver.obj", device);
    if (gameOverModel) {
        gameOverModel->transform.translate = { 9.2f, 11.0f, 30.0f };
        gameOverModel->transform.scale = { 1.0f, 1.0f, 1.0f };
    }

    // GameClear Model
    gameClearModel = Model::Create("Resources/Clear", "Clear.obj", device);
    if (gameClearModel) {
        gameClearModel->transform.translate = { 9.2f, 10.0f, 40.0f };
        gameClearModel->transform.scale = { 1.0f, 1.0f, 1.0f };
    }

    // スカイドームモデル生成
    skydomeModel = Model::Create("Resources/skydome", "skydome.obj", device);
    if (skydomeModel) {
        // ★変更: 背景全体を覆うようにさらに大きくする (5000倍)
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

    // ImGui 初期化削除

    // --- シーン管理用変数 ---
    GameScene currentScene = GameScene::Title;
    bool isGameInitialized = false;
    bool isLoadingNextMap = false;

    // ★ 現在のマップ名とリスポーン地点（セーブデータ代わり）
    // ゲーム起動中だけ保持される
    std::string currentMapFilePath = "Resources/map.csv";
    Vector3 currentRespawnPos = { 0.0f, 0.0f, 0.0f }; // 0,0,0 ならデフォルト位置

    // 次のマップ移動用
    std::string nextMapFilePath = "";
    Vector3 nextRespawnPos = { 0.0f, 0.0f, 0.0f };

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

        // ImGui NewFrame 削除

        // --- シーン処理 (ロジック更新のみ) ---
        switch (currentScene) {
        case GameScene::Title:
            // ★ Spaceキーで遷移
            if (input->IsKeyPressed(VK_SPACE)) {
                // スタート時は必ず map.csv から（クリア後のリセットなども考慮）
                if (currentMapFilePath.empty()) {
                    currentMapFilePath = "Resources/map.csv";
                    currentRespawnPos = { 0.0f, 0.0f, 0.0f };
                }
                currentScene = GameScene::GamePlay;
            }
            break;

        case GameScene::GamePlay:
            if (!isGameInitialized) {
                // ... (初期化処理) ...
                mapChip = new MapChip();
                playerModel = Model::Create("Resources/player", "player.obj", device);
                player = new Player();

                // ★ 記憶しているマップを読み込む
                mapChip->Load(currentMapFilePath, device);

                player->Initialize(playerModel, mapChip, device);

                // ★ リスポーン地点が設定されていればそこに移動（Map2, Map3のリスタート用）
                if (currentRespawnPos.x != 0.0f || currentRespawnPos.y != 0.0f) {
                    player->SetPosition(currentRespawnPos);
                }

                // map.csv の場合のみ手動トラップ配置 (元のコード仕様)
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
                if (mapChip->HasGoal()) {
                    goalModel_ = Model::Create("Resources/cube", "cube.obj", device);
                    if (goalModel_) {
                        goalModel_->transform.scale = { MapChip::kBlockSize, MapChip::kBlockSize, MapChip::kBlockSize };
                        goalModel_->transform.rotate = { 0.0f, 0.0f, 0.0f };
                        goalModel_->transform.translate = mapChip->GetGoalPosition();
                    }
                }
                isLoadingNextMap = false;
                isGameInitialized = true;
            }

            if (!isLoadingNextMap) {
                if (player->IsAlive()) {
                    player->Update();

                    // ★追加: 落下判定 (Y座標が-10.0f以下ならゲームオーバー)
                    if (player->GetPosition().y < -10.0f) {
                        currentScene = GameScene::GameOver;
                    }
                } else {
                    currentScene = GameScene::GameOver;
                }

                for (Trap* trap : traps_) trap->Update(player);
                for (FallingBlock* block : fallingBlocks_) block->Update(player, mapChip);

                // --- マップ移動・クリア判定 ---

                // 1. map.csv -> map2.csv (IsExitingで判定)
                if (currentMapFilePath == "Resources/map.csv") {
                    if (player->IsExiting()) {
                        isLoadingNextMap = true;
                        nextMapFilePath = "Resources/map2.csv";
                        // map2 初期位置 (元のコード通り)
                        size_t map2Height = 15;
                        float spawnY = (static_cast<float>(map2Height - 1) - 14.0f) * MapChip::kBlockSize + (MapChip::kBlockSize / 2.0f);
                        float spawnX = (MapChip::kBlockSize / 2.0f);
                        nextRespawnPos = { spawnX, spawnY, 0.0f };
                    }
                }
                // 2. ★ map2.csv -> map3.csv (左上到達判定)
                else if (currentMapFilePath == "Resources/map2.csv") {
                    Vector3 pPos = player->GetPosition();
                    // map2の高さ (MapChipから取得)
                    float mapTopY = static_cast<float>(mapChip->GetRowCount()) * MapChip::kBlockSize;

                    // 左上エリア判定 (X < 2ブロック分, Y > 上から2ブロック分)
                    if (pPos.x < MapChip::kBlockSize * 2.0f && pPos.y > mapTopY - (MapChip::kBlockSize * 2.0f)) {
                        isLoadingNextMap = true;
                        nextMapFilePath = "Resources/map3.csv";

                        // ★ map3 初期位置: 3.5f (ご指定通り2.5fからさらに右へ+1)
                        float spawnX = MapChip::kBlockSize * 3.5f;
                        float spawnY = MapChip::kBlockSize * 1.5f; // 下から2段目
                        nextRespawnPos = { spawnX, spawnY, 0.0f };
                    }
                }
                // 3. ★ map3.csv -> ゲームクリア (ゴール判定)
                else if (currentMapFilePath == "Resources/map3.csv") {
                    if (player->IsAlive() && mapChip->HasGoal() && mapChip->CheckGoalCollision(player->GetPosition(), player->GetHalfSize())) {
                        currentScene = GameScene::GameClear;
                        // セーブデータをリセット
                        currentMapFilePath = "Resources/map.csv";
                        currentRespawnPos = { 0.0f, 0.0f, 0.0f };
                    }
                }
            }
            // player->ImGui_Draw(); // 削除

            if (isLoadingNextMap) {
                // リソース解放
                for (Trap* trap : traps_) delete trap; traps_.clear();
                for (FallingBlock* block : fallingBlocks_) delete block; fallingBlocks_.clear();
                delete goalModel_; goalModel_ = nullptr;

                // ★ セーブデータを更新 (インゲームセーブ)
                currentMapFilePath = nextMapFilePath;
                currentRespawnPos = nextRespawnPos;

                // 新しいマップをロード
                mapChip->Load(currentMapFilePath, device);

                // プレイヤー位置更新
                player->SetPosition(currentRespawnPos);

                // オブジェクト再配置
                const auto& dynamicBlocks2 = mapChip->GetDynamicBlocks();
                for (const auto& data : dynamicBlocks2) {
                    FallingBlock* newBlock = new FallingBlock();
                    newBlock->Initialize(device, data.position, static_cast<BlockType>(data.type));
                    fallingBlocks_.push_back(newBlock);
                }
                if (mapChip->HasGoal()) {
                    goalModel_ = Model::Create("Resources/cube", "cube.obj", device);
                    if (goalModel_) {
                        goalModel_->transform.scale = { MapChip::kBlockSize, MapChip::kBlockSize, MapChip::kBlockSize };
                        goalModel_->transform.rotate = { 0.0f, 0.0f, 0.0f };
                        goalModel_->transform.translate = mapChip->GetGoalPosition();
                    }
                }

                isLoadingNextMap = false;
                // isGameInitialized は true のまま (リソース再利用)
            }
            break;

        case GameScene::GameOver:
            // ★ Spaceキーでリスタート
            if (input->IsKeyPressed(VK_SPACE)) {
                cleanupGameResources(); // リソースを全て解放して isGameInitialized = false にする
                currentScene = GameScene::GamePlay; // GamePlayに戻る

                // ここで GamePlay に戻ると、isGameInitialized == false なので初期化処理が走る。
                // その際、currentMapFilePath と currentRespawnPos は
                // 直前の状態（Map3ならMap3の初期位置）を保持しているので、そこから再開される。
            }
            break;

        case GameScene::GameClear:
            // ★ Spaceキーでタイトルへ
            if (input->IsKeyPressed(VK_SPACE)) {
                cleanupGameResources();
                currentScene = GameScene::Title;
                // セーブデータは GamePlay ループ内のクリア判定時にリセット済み
            }
            break;
        }

        // ImGui Render 削除

        // --- 描画開始 ---
        // 1. 画面クリア
        dxCommon->PreDraw();

        // 2. 共通描画設定 (ルートシグネチャ、パイプライン)
        commandList->SetGraphicsRootSignature(graphicsPipeline->GetRootSignature());
        commandList->SetPipelineState(graphicsPipeline->GetPipelineState(kBlendModeNone));
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // 3. 定数バッファ設定
        const Matrix4x4& viewProjectionMatrix = camera->GetViewProjectionMatrix();
        cameraForGpuData->worldPosition = camera->GetTransform().translate;
        directionalLightData->direction = Normalize(directionalLightData->direction);
        commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(4, cameraForGpuResource->GetGPUVirtualAddress());

        // 4. ディスクリプタヒープの設定
        ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
        commandList->SetDescriptorHeaps(1, descriptorHeaps);

        // 5. ゲームオブジェクトの描画

        // ★変更: スカイドーム描画 (全シーン共通で、一番最初に描画して背景にする)
        if (skydomeModel && skydomeTextureResource) {
            skydomeModel->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), skydomeTextureSrvHandleGPU);
        }

        if (currentScene == GameScene::Title) {
            // タイトルモデルの描画
            if (titleModel && titleTextureResource) {
                titleModel->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), titleTextureSrvHandleGPU);
            }
        } else if (currentScene == GameScene::GameOver) {
            // ゲームオーバーモデルの描画
            if (gameOverModel && gameOverTextureResource) {
                gameOverModel->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), gameOverTextureSrvHandleGPU);
            }
        } else if (currentScene == GameScene::GameClear) {
            // ゲームクリアモデルの描画
            if (gameClearModel && gameClearTextureResource) {
                gameClearModel->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), gameClearTextureSrvHandleGPU);
            }
        } else if (currentScene == GameScene::GamePlay && isGameInitialized) {
            // スカイドームは上で描画済みなのでここでは不要
            if (blockTextureResource) mapChip->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), blockTextureSrvHandleGPU);
            if (playerTextureResource) player->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), playerTextureSrvHandleGPU);
            if (cubeTextureResource) {
                for (Trap* trap : traps_) trap->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), cubeTextureSrvHandleGPU);
                if (goalModel_) goalModel_->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), cubeTextureSrvHandleGPU);
            }
            if (blockTextureResource) {
                for (FallingBlock* block : fallingBlocks_) block->Draw(commandList, viewProjectionMatrix, directionalLightResource->GetGPUVirtualAddress(), blockTextureSrvHandleGPU);
            }
        }

        // 6. ImGuiの描画 削除

        // 7. 画面フリップ
        dxCommon->PostDraw();

    } // End Loop

    if (isGameInitialized) cleanupGameResources();

    // 中間リソースはComPtrのデストラクタで自動解放されます
    // シーン用モデルの削除
    delete titleModel; delete gameOverModel; delete gameClearModel;
    delete skydomeModel; // スカイドーム解放
    delete graphicsPipeline; delete camera;

    dxCommon->Finalize();
    CoUninitialize();
    winApp->Finalize();

    return 0;
}