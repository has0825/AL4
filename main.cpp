// ========================================================================
//
// main.cpp (省略なし完全版)
//
// ========================================================================
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
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <xaudio2.h>
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
#include "Camera.h" // Camera.h をインクルード
#include "Trap.h" // Trap.h をインクルード
#include "FallingBlock.h" // 新規作成した FallingBlock.h


// ヘルパー関数 (省略なし)
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception)
{
    SYSTEMTIME time;
    GetLocalTime(&time);
    wchar_t filePath[MAX_PATH] = { 0 };
    CreateDirectory(L"./Dumps", nullptr);
    StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
        time.wYear, time.wMonth, time.wDay, time.wHour,
        time.wMinute);
    HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    DWORD processId = GetCurrentProcessId();
    DWORD threadId = GetCurrentThreadId();
    MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
    minidumpInformation.ThreadId = threadId;
    minidumpInformation.ExceptionPointers = exception;
    minidumpInformation.ClientPointers = TRUE;
    MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle,
        MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
    return EXCEPTION_EXECUTE_HANDLER;
}
void Log(std::ostream& os, const std::string& message)
{
    os << message << std::endl;
    OutputDebugStringA(message.c_str());
}
std::wstring ConvertString(const std::string& str)
{
    if (str.empty()) { return std::wstring(); }
    auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
    if (sizeNeeded == 0) { return std::wstring(); }
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
    return result;
}
std::string ConvertString(const std::wstring& str)
{
    if (str.empty()) { return std::string(); }
    auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
    if (sizeNeeded == 0) { return std::string(); }
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
    return result;
}
struct D3DResourceLeakChecker {
    ~D3DResourceLeakChecker()
    {
        Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
            debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
            debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
            debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
        }
    }
};

// ★ GameScene の enum
enum class GameScene {
    Title,      // タイトルシーン
    GamePlay,   // ゲームプレイシーン
    GameOver,   // ゲームオーバーシーン
    GameClear   // ゲームクリアシーン
};


int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    D3DResourceLeakChecker leakChecker;
    WinApp* winApp = WinApp::GetInstance();
    winApp->Initialize();
    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    dxCommon->Initialize(winApp);
    Input::GetInstance()->Initialize();
    CoInitializeEx(0, COINIT_MULTITHREADED); // "T" のみ
    SetUnhandledExceptionFilter(ExportDump);
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice;
    XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    xAudio2->CreateMasteringVoice(&masterVoice);
    ID3D12Device* device = dxCommon->GetDevice();
    GraphicsPipeline* graphicsPipeline = new GraphicsPipeline();
    graphicsPipeline->Initialize(device);
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    // --- ▼▼▼ リソースの宣言 (★ 変更： new は後回し) ▼▼▼ ---
    MapChip* mapChip = nullptr;
    Model* playerModel = nullptr;
    Player* player = nullptr;

    std::vector<Trap*> traps_;
    std::vector<FallingBlock*> fallingBlocks_;
    Model* goalModel_ = nullptr;
    // --- ▲▲▲ リソースの宣言 ▲▲▲ ---


    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

    // プレイヤーのテクスチャをロード (変更なし)
    std::string playerTexturePath = "Resources/player/player.png";
    DirectX::ScratchImage playerMipImages = LoadTexture(playerTexturePath);
    const DirectX::TexMetadata& playerMetadata = playerMipImages.GetMetadata();
    Microsoft::WRL::ComPtr<ID3D12Resource> playerTextureResource = CreateTextureResource(device, playerMetadata);
    Microsoft::WRL::ComPtr<ID3D12Resource> playerIntermediateResource = UploadTextureData(playerTextureResource.Get(), playerMipImages, device, commandList);

    // "block.png" を読み込む (変更なし)
    std::string blockTexturePath = "Resources/block/block.png";
    DirectX::ScratchImage blockMipImages = LoadTexture(blockTexturePath);
    if (blockMipImages.GetImageCount() == 0) {
        OutputDebugStringA("ERROR: Failed to load block.png!\n");
        assert(false && "Failed to load block.png");
    }
    const DirectX::TexMetadata& blockMetadata = blockMipImages.GetMetadata();
    Microsoft::WRL::ComPtr<ID3D12Resource> blockTextureResource = CreateTextureResource(device, blockMetadata);
    Microsoft::WRL::ComPtr<ID3D12Resource> blockIntermediateResource = UploadTextureData(blockTextureResource.Get(), blockMipImages, device, commandList);

    // "cube.jpg" を読み込む (変更なし)
    std::string cubeTexturePath = "Resources/cube/cube.jpg";
    DirectX::ScratchImage cubeMipImages = LoadTexture(cubeTexturePath);
    if (cubeMipImages.GetImageCount() == 0) {
        OutputDebugStringA("ERROR: Failed to load cube.jpg!\n");
        assert(false && "Failed to load cube.jpg");
    }
    const DirectX::TexMetadata& cubeMetadata = cubeMipImages.GetMetadata();
    Microsoft::WRL::ComPtr<ID3D12Resource> cubeTextureResource = CreateTextureResource(device, cubeMetadata);
    Microsoft::WRL::ComPtr<ID3D12Resource> cubeIntermediateResource = UploadTextureData(cubeTextureResource.Get(), cubeMipImages, device, commandList);


    const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // プレイヤーテクスチャ用のSRVを作成（ヒープの2番目） (変更なし)
    D3D12_SHADER_RESOURCE_VIEW_DESC playerSrvDesc{};
    playerSrvDesc.Format = playerMetadata.format;
    playerSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    playerSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    playerSrvDesc.Texture2D.MipLevels = UINT(playerMetadata.mipLevels);
    D3D12_CPU_DESCRIPTOR_HANDLE playerTextureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);
    D3D12_GPU_DESCRIPTOR_HANDLE playerTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);
    device->CreateShaderResourceView(playerTextureResource.Get(), &playerSrvDesc, playerTextureSrvHandleCPU);

    // block.png の SRV を作成 (ヒープの3番目) (変更なし)
    D3D12_SHADER_RESOURCE_VIEW_DESC blockSrvDesc{};
    blockSrvDesc.Format = blockMetadata.format;
    blockSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    blockSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    blockSrvDesc.Texture2D.MipLevels = UINT(blockMetadata.mipLevels);
    D3D12_CPU_DESCRIPTOR_HANDLE blockTextureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 3);
    D3D12_GPU_DESCRIPTOR_HANDLE blockTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 3);
    device->CreateShaderResourceView(blockTextureResource.Get(), &blockSrvDesc, blockTextureSrvHandleCPU);

    // cube.jpg の SRV を作成 (ヒープの4番目) (変更なし)
    D3D12_SHADER_RESOURCE_VIEW_DESC cubeSrvDesc{};
    cubeSrvDesc.Format = cubeMetadata.format;
    cubeSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    cubeSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    cubeSrvDesc.Texture2D.MipLevels = UINT(cubeMetadata.mipLevels);
    D3D12_CPU_DESCRIPTOR_HANDLE cubeTextureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 4);
    D3D12_GPU_DESCRIPTOR_HANDLE cubeTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 4);
    device->CreateShaderResourceView(cubeTextureResource.Get(), &cubeSrvDesc, cubeTextureSrvHandleCPU);

    // (★ mapChip->Load は GamePlay 初期化へ移動)

    // (★ トラップ生成は GamePlay 初期化へ移動)

    // (★ FallingBlock 生成は GamePlay 初期化へ移動)

    // (★ Goal 生成は GamePlay 初期化へ移動)


    // ライトの初期化 (変更なし)
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
    DirectionalLight* directionalLightData = nullptr;
    directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
    directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData->direction = Normalize({ 0.0f, -1.0f, 0.0f });
    directionalLightData->intensity = 1.0f;

    // Camera クラスを作成・初期化 (変更なし)
    Camera* camera = new Camera();
    camera->Initialize();

    // カメラのワールド座標をGPUに送るためのリソース (変更なし)
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraForGpuResource = CreateBufferResource(device, sizeof(CameraForGpu));
    CameraForGpu* cameraForGpuData = nullptr;
    cameraForGpuResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraForGpuData));

    // ImGuiの初期化 (変更なし)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsClassic();
    ImGui_ImplWin32_Init(winApp->GetHwnd());
    ImGui_ImplDX12_Init(device, dxCommon->GetBackBufferCount(), dxCommon->GetRtvDesc().Format,
        srvDescriptorHeap.Get(),
        srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());


    // --- ▼▼▼ ★ 追加 ★ ▼▼▼ ---
    GameScene currentScene = GameScene::Title; // 初期シーン
    bool isGameInitialized = false; // ゲームリソースが初期化されたか
    bool isLoadingNextMap = false; // (GamePlay 内で使う)
    // --- ▲▲▲ ★ 追加 ★ ▲▲▲ ---

    // ★ ゲームプレイリソースを解放するラムダ関数
    auto cleanupGameResources = [&]() {
        delete mapChip;
        mapChip = nullptr;
        delete player;
        player = nullptr;
        delete playerModel; // playerModel は GamePlay 初期化で new する
        playerModel = nullptr;

        for (Trap* trap : traps_) {
            delete trap;
        }
        traps_.clear();

        for (FallingBlock* block : fallingBlocks_) {
            delete block;
        }
        fallingBlocks_.clear();

        delete goalModel_;
        goalModel_ = nullptr;

        isGameInitialized = false; // 未初期化状態に戻す
        };


    // メインループ
    while (!winApp->IsEndRequested()) {
        winApp->ProcessMessage();
        Input::GetInstance()->Update();
        Input* input = Input::GetInstance(); // わかりやすくするため

        // ImGuiフレーム開始 (共通)
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // --- ▼▼▼ ★ 変更： シーンベースの更新処理 ▼▼▼ ---
        switch (currentScene) {

            // ========================================================
        case GameScene::Title:
        {
            // --- 更新 (Title) ---
            if (input->IsKeyPressed(VK_RETURN)) { // Enterキー
                currentScene = GameScene::GamePlay;
            }

            // --- 描画 (Title) ---
            ImGui::Begin("TITLE SCREEN");
            ImVec2 windowSize(300, 120); // 少し縦幅を広げる
            ImGui::SetWindowSize(windowSize);
            ImGui::SetWindowPos(ImVec2(
                (WinApp::kClientWidth - windowSize.x) * 0.5f,
                (WinApp::kClientHeight - windowSize.y) * 0.5f
            ));
            ImGui::Text("My Awesome Game"); // (ゲームタイトルは適宜変更してください)
            ImGui::Separator();
            ImGui::Text("Press ENTER to Start");
            ImGui::End();

            break; // Title シーン終わり
        }
        // ========================================================


        // ========================================================
        case GameScene::GamePlay:
        {
            // --- ★ ゲームプレイリソースの初期化 (初回のみ) ★ ---
            if (!isGameInitialized) {

                // (元々 WinMain の冒頭にあった初期化処理をここに移動)

                mapChip = new MapChip();
                playerModel = Model::Create("Resources/player", "player.obj", device);
                player = new Player();

                mapChip->Load("Resources/map.csv", device);
                player->Initialize(playerModel, mapChip);

                // --- ▼▼▼ map.csv 用のトラップを手動で生成 (省略なし) ▼▼▼ ---
                size_t mapHeight = 15; // CSVの総行数
                auto csvYToWorldY = [&](int csvY) {
                    return (static_cast<float>(mapHeight - 1) - static_cast<float>(csvY)) * MapChip::kBlockSize + (MapChip::kBlockSize / 2.0f);
                    };

                // 1. 左から来るトラップ (CSV y=4, 5, 6)
                float stopMarginNormal = MapChip::kBlockSize * 1.0f;

                Trap* trapL1 = new Trap();
                trapL1->Initialize(device, csvYToWorldY(4), Trap::AttackSide::FromLeft, stopMarginNormal);
                traps_.push_back(trapL1);

                Trap* trapL2 = new Trap();
                trapL2->Initialize(device, csvYToWorldY(5), Trap::AttackSide::FromLeft, stopMarginNormal);
                traps_.push_back(trapL2);

                Trap* trapL3 = new Trap();
                trapL3->Initialize(device, csvYToWorldY(6), Trap::AttackSide::FromLeft, stopMarginNormal);
                traps_.push_back(trapL3);

                // 2. 右から来るトラップ (通常) (CSV y=8, 9, 10)
                Trap* trapR1 = new Trap();
                trapR1->Initialize(device, csvYToWorldY(8), Trap::AttackSide::FromRight, stopMarginNormal);
                traps_.push_back(trapR1);

                Trap* trapR2 = new Trap();
                trapR2->Initialize(device, csvYToWorldY(9), Trap::AttackSide::FromRight, stopMarginNormal);
                traps_.push_back(trapR2);

                Trap* trapR3 = new Trap();
                trapR3->Initialize(device, csvYToWorldY(10), Trap::AttackSide::FromRight, stopMarginNormal);
                traps_.push_back(trapR3);

                // 3. 右から来るトラップ (Short) (CSV y=12, 13)
                float stopMarginShort = MapChip::kBlockSize * 0.2f;

                Trap* trapRS1 = new Trap();
                trapRS1->Initialize(device, csvYToWorldY(12), Trap::AttackSide::FromRight, stopMarginShort);
                traps_.push_back(trapRS1);

                Trap* trapRS2 = new Trap();
                trapRS2->Initialize(device, csvYToWorldY(13), Trap::AttackSide::FromRight, stopMarginShort);
                traps_.push_back(trapRS2);
                // --- ▲▲▲ トラップ生成完了 ▲▲▲ ---

                // 4. マップチップから 3, 4 の情報を取得して FallingBlock を生成
                const std::vector<DynamicBlockData>& dynamicBlocks = mapChip->GetDynamicBlocks();
                for (const DynamicBlockData& data : dynamicBlocks) {
                    FallingBlock* newBlock = new FallingBlock();
                    newBlock->Initialize(device, data.position, static_cast<BlockType>(data.type));
                    fallingBlocks_.push_back(newBlock);
                }

                // 5. マップチップから 5 (ゴール) の情報を取得してモデルを生成
                if (mapChip->HasGoal()) {
                    goalModel_ = Model::Create("Resources/cube", "cube.obj", device);
                    goalModel_->transform.scale = { MapChip::kBlockSize, MapChip::kBlockSize, MapChip::kBlockSize };
                    goalModel_->transform.rotate = { 0.0f, 0.0f, 0.0f };
                    goalModel_->transform.translate = mapChip->GetGoalPosition();
                }

                isLoadingNextMap = false; // マップ遷移フラグもリセット
                isGameInitialized = true; // 初期化完了
            }

            // --- 更新 (GamePlay) ---
            if (!isLoadingNextMap) {
                if (player->IsAlive()) {
                    player->Update();
                } else {
                    // ★ 死亡したら GameOver シーンへ
                    currentScene = GameScene::GameOver;
                }

                // すべてのトラップを更新
                for (Trap* trap : traps_) {
                    trap->Update(player);
                }
                // すべての落ちるブロックを更新
                for (FallingBlock* block : fallingBlocks_) {
                    block->Update(player, mapChip);
                }

                // プレイヤーがマップ外に出たかチェック
                if (player->IsExiting()) {
                    isLoadingNextMap = true; // 遷移フラグを立てる
                }

                // プレイヤーがゴールしたかチェック
                if (player->IsAlive() && mapChip->HasGoal() && mapChip->CheckGoalCollision(player->GetPosition(), player->GetHalfSize())) {
                    // ★ クリアしたら GameClear シーンへ
                    currentScene = GameScene::GameClear;
                }
            }

            player->ImGui_Draw();

            // --- マップ遷移処理 (省略なし) ---
            if (isLoadingNextMap) {

                // 1. 古いトラップをすべて削除
                for (Trap* trap : traps_) {
                    delete trap;
                }
                traps_.clear();

                // 1b. 古い落ちるブロックをすべて削除
                for (FallingBlock* block : fallingBlocks_) {
                    delete block;
                }
                fallingBlocks_.clear();

                // 1c. 古いゴールを削除
                delete goalModel_;
                goalModel_ = nullptr;


                // 2. 新しいマップをロード
                mapChip->Load("Resources/map2.csv", device);

                // 3. map2.csv 用の新しい開始位置を決定 (左下)
                size_t map2Height = 15;
                float spawnY = (static_cast<float>(map2Height - 1) - 14.0f) * MapChip::kBlockSize + (MapChip::kBlockSize / 2.0f);
                float spawnX = (static_cast<float>(0)) * MapChip::kBlockSize + (MapChip::kBlockSize / 2.0f);
                Vector3 map2StartPosition = { spawnX, spawnY, 0.0f };
                player->SetPosition(map2StartPosition);

                // 4. map2.csv 用の新しい動的オブジェクト (3, 4) を生成
                const std::vector<DynamicBlockData>& dynamicBlocks2 = mapChip->GetDynamicBlocks();
                for (const DynamicBlockData& data : dynamicBlocks2) {
                    FallingBlock* newBlock = new FallingBlock();
                    newBlock->Initialize(device, data.position, static_cast<BlockType>(data.type));
                    fallingBlocks_.push_back(newBlock);
                }

                // 5. map2.csv 用のゴール (5) を生成
                if (mapChip->HasGoal()) {
                    goalModel_ = Model::Create("Resources/cube", "cube.obj", device);
                    goalModel_->transform.scale = { MapChip::kBlockSize, MapChip::kBlockSize, MapChip::kBlockSize };
                    goalModel_->transform.rotate = { 0.0f, 0.0f, 0.0f };
                    goalModel_->transform.translate = mapChip->GetGoalPosition();
                }

                // 6. 遷移完了
                isLoadingNextMap = false;
            }

            break; // GamePlay シーン終わり
        }
        // ========================================================


        // ========================================================
        // ★★★ ここから変更 ★★★
        case GameScene::GameOver:
        {
            // --- 更新 (GameOver) ---
            if (input->IsKeyPressed(VK_RETURN)) { // Enterキーが押されたら
                cleanupGameResources();
                currentScene = GameScene::Title; // タイトルに戻る
            }

            // --- 描画 (GameOver) ---
            //ImGui::Begin("GAME OVER");
            //ImVec2 windowSize(300, 120); // タイトルと合わせる
            //ImGui::SetWindowSize(windowSize);
            //ImGui::SetWindowPos(ImVec2(
            //    (WinApp::kClientWidth - windowSize.x) * 0.5f,
            //    (WinApp::kClientHeight - windowSize.y) * 0.5f
            //));
            //ImGui::Text("You Died!");
            //ImGui::Separator();
            //ImGui::Text("Press ENTER to return to Title");
            //ImGui::End();

            break;
        }
        // ========================================================


        // ========================================================
        // ★★★ ここから変更 ★★★
        case GameScene::GameClear:
        {
            // --- 更新 (GameClear) ---
            if (input->IsKeyPressed(VK_RETURN)) { // Enterキーが押されたら
                cleanupGameResources();
                currentScene = GameScene::Title; // タイトルに戻る
            }

            //// --- 描画 (GameClear) ---
            //ImGui::Begin("GAME CLEAR");
            //ImVec2 windowSize(300, 120); // タイトルと合わせる
            //ImGui::SetWindowSize(windowSize);
            //ImGui::SetWindowPos(ImVec2(
            //    (WinApp::kClientWidth - windowSize.x) * 0.5f,
            //    (WinApp::kClientHeight - windowSize.y) * 0.5f
            //));
            //ImGui::Text("Congratulations!");
            //ImGui::Separator();
            //ImGui::Text("Press ENTER to return to Title");
            //ImGui::End();

            break;
        }
        // ========================================================

        } // --- switch (currentScene) 終わり ---


        /*ImGui::Render();*/

        const Matrix4x4& viewProjectionMatrix = camera->GetViewProjectionMatrix();
        cameraForGpuData->worldPosition = camera->GetTransform().translate;
        directionalLightData->direction = Normalize(directionalLightData->direction);


        // --- ▼▼▼ 描画処理 (共通処理) ▼▼▼ ---
        dxCommon->PreDraw();

        commandList->SetGraphicsRootSignature(graphicsPipeline->GetRootSignature());
        ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
        commandList->SetDescriptorHeaps(1, descriptorHeaps);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(4, cameraForGpuResource->GetGPUVirtualAddress());
        commandList->SetPipelineState(graphicsPipeline->GetPipelineState(kBlendModeNone));

        // --- ★ 描画処理 (シーン分岐) ★ ---
        // (GamePlay シーンでのみゲームオブジェクトを描画する)
        // ★★★ ここの if 文を変更 ★★★
        if (currentScene == GameScene::GamePlay)
        {
            // ゲームが初期化されていないと (isGameInitialized == false) 各ポインタは nullptr
            if (isGameInitialized) {
                // マップの描画
                mapChip->Draw(
                    commandList,
                    viewProjectionMatrix,
                    directionalLightResource->GetGPUVirtualAddress(),
                    blockTextureSrvHandleGPU);

                // プレイヤーの描画
                player->Draw(
                    commandList,
                    viewProjectionMatrix,
                    directionalLightResource->GetGPUVirtualAddress(),
                    playerTextureSrvHandleGPU);

                // すべてのトラップ(横)を描画
                for (Trap* trap : traps_) {
                    trap->Draw(
                        commandList,
                        viewProjectionMatrix,
                        directionalLightResource->GetGPUVirtualAddress(),
                        cubeTextureSrvHandleGPU);
                }

                // すべての落ちるブロック(3, 4)を描画
                for (FallingBlock* block : fallingBlocks_) {
                    block->Draw(
                        commandList,
                        viewProjectionMatrix,
                        directionalLightResource->GetGPUVirtualAddress(),
                        blockTextureSrvHandleGPU);
                }

                // ゴール(5)を描画
                if (goalModel_) {
                    goalModel_->Draw(
                        commandList,
                        viewProjectionMatrix,
                        directionalLightResource->GetGPUVirtualAddress(),
                        cubeTextureSrvHandleGPU);
                }
            }
        }
        // (Title, GameOver, GameClear シーンは ImGui 以外に描画するものがない)


        // ImGui の描画 (全シーン共通)
        /*ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);*/
        dxCommon->PostDraw();
        // --- ▲▲▲ 描画処理 ▲▲▲ ---

    } // --- メインループ (while) 終わり ---

    //ImGui_ImplDX12_Shutdown();
    //ImGui_ImplWin32_Shutdown();
    //ImGui::DestroyContext();

    // --- ▼▼▼ ★ 変更： 終了処理 ▼▼▼ ---

    // もしゲームプレイ中に終了したらリソースを解放する
    if (isGameInitialized) {
        cleanupGameResources(); // ★ 作成した解放関数を呼ぶ
    }

    // (元からあった解放処理)
    delete graphicsPipeline;
    delete camera;

    // (mapChip, player, playerModel, traps_, fallingBlocks_, goalModel_ は
    //  cleanupGameResources で解放済み or そもそも new されていない)

    dxCommon->Finalize();
    CoUninitialize();
    winApp->Finalize();
    return 0;
}