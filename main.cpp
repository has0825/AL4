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


int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    D3DResourceLeakChecker leakChecker;
    WinApp* winApp = WinApp::GetInstance();
    winApp->Initialize();
    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    dxCommon->Initialize(winApp);
    Input::GetInstance()->Initialize();
    CoInitializeEx(0, COINIT_MULTITHREADED);
    SetUnhandledExceptionFilter(ExportDump);
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice;
    XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    xAudio2->CreateMasteringVoice(&masterVoice);
    ID3D12Device* device = dxCommon->GetDevice();
    GraphicsPipeline* graphicsPipeline = new GraphicsPipeline();
    graphicsPipeline->Initialize(device);
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    MapChip* mapChip = new MapChip();
    Model* playerModel = Model::Create("Resources/player", "player.obj", device);
    Player* player = new Player();
    player->Initialize(playerModel, mapChip);

    // トラップを複数管理するベクター
    std::vector<Trap*> traps_;

    // 落ちるブロック(3, 4)を複数管理するベクター
    std::vector<FallingBlock*> fallingBlocks_;

    // ゴール(5)のモデル
    Model* goalModel_ = nullptr;


    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

    // プレイヤーのテクスチャをロード
    std::string playerTexturePath = "Resources/player/player.png";
    DirectX::ScratchImage playerMipImages = LoadTexture(playerTexturePath);
    const DirectX::TexMetadata& playerMetadata = playerMipImages.GetMetadata();
    Microsoft::WRL::ComPtr<ID3D12Resource> playerTextureResource = CreateTextureResource(device, playerMetadata);
    Microsoft::WRL::ComPtr<ID3D12Resource> playerIntermediateResource = UploadTextureData(playerTextureResource.Get(), playerMipImages, device, commandList);

    // "block.png" を読み込む
    std::string blockTexturePath = "Resources/block/block.png";
    DirectX::ScratchImage blockMipImages = LoadTexture(blockTexturePath);
    if (blockMipImages.GetImageCount() == 0) {
        OutputDebugStringA("ERROR: Failed to load block.png!\n");
        assert(false && "Failed to load block.png");
    }
    const DirectX::TexMetadata& blockMetadata = blockMipImages.GetMetadata();
    Microsoft::WRL::ComPtr<ID3D12Resource> blockTextureResource = CreateTextureResource(device, blockMetadata);
    Microsoft::WRL::ComPtr<ID3D12Resource> blockIntermediateResource = UploadTextureData(blockTextureResource.Get(), blockMipImages, device, commandList);

    // "cube.jpg" を読み込む
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

    // プレイヤーテクスチャ用のSRVを作成（ヒープの2番目）
    D3D12_SHADER_RESOURCE_VIEW_DESC playerSrvDesc{};
    playerSrvDesc.Format = playerMetadata.format;
    playerSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    playerSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    playerSrvDesc.Texture2D.MipLevels = UINT(playerMetadata.mipLevels);
    D3D12_CPU_DESCRIPTOR_HANDLE playerTextureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);
    D3D12_GPU_DESCRIPTOR_HANDLE playerTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);
    device->CreateShaderResourceView(playerTextureResource.Get(), &playerSrvDesc, playerTextureSrvHandleCPU);

    // block.png の SRV を作成 (ヒープの3番目)
    D3D12_SHADER_RESOURCE_VIEW_DESC blockSrvDesc{};
    blockSrvDesc.Format = blockMetadata.format;
    blockSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    blockSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    blockSrvDesc.Texture2D.MipLevels = UINT(blockMetadata.mipLevels);
    D3D12_CPU_DESCRIPTOR_HANDLE blockTextureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 3);
    D3D12_GPU_DESCRIPTOR_HANDLE blockTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 3);
    device->CreateShaderResourceView(blockTextureResource.Get(), &blockSrvDesc, blockTextureSrvHandleCPU);

    // cube.jpg の SRV を作成 (ヒープの4番目)
    D3D12_SHADER_RESOURCE_VIEW_DESC cubeSrvDesc{};
    cubeSrvDesc.Format = cubeMetadata.format;
    cubeSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    cubeSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    cubeSrvDesc.Texture2D.MipLevels = UINT(cubeMetadata.mipLevels);
    D3D12_CPU_DESCRIPTOR_HANDLE cubeTextureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 4);
    D3D12_GPU_DESCRIPTOR_HANDLE cubeTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 4);
    device->CreateShaderResourceView(cubeTextureResource.Get(), &cubeSrvDesc, cubeTextureSrvHandleCPU);


    // Load() を device 付きでここで呼び出す
    mapChip->Load("Resources/map.csv", device);

    // --- ▼▼▼ map.csv 用のトラップを手動で生成 ▼▼▼ ---

    // CSVのY座標をワールドのY座標に変換するラムダ式
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


    // ライトの初期化
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
    DirectionalLight* directionalLightData = nullptr;
    directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
    directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData->direction = Normalize({ 0.0f, -1.0f, 0.0f });
    directionalLightData->intensity = 1.0f;

    // Camera クラスを作成・初期化
    Camera* camera = new Camera();
    camera->Initialize(); // Initialize 内で固定位置・角度が設定される

    // カメラのワールド座標をGPUに送るためのリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraForGpuResource = CreateBufferResource(device, sizeof(CameraForGpu));
    CameraForGpu* cameraForGpuData = nullptr;
    cameraForGpuResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraForGpuData));

    // ImGuiの初期化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsClassic();
    ImGui_ImplWin32_Init(winApp->GetHwnd());
    ImGui_ImplDX12_Init(device, dxCommon->GetBackBufferCount(), dxCommon->GetRtvDesc().Format,
        srvDescriptorHeap.Get(),
        srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

    bool isLoadingNextMap = false;
    bool isGameClear = false; // ゲームクリアフラグ


    // メインループ
    while (!winApp->IsEndRequested()) {
        winApp->ProcessMessage();
        Input::GetInstance()->Update();

        // 遷移中、ゲームクリア中は更新しない
        if (!isLoadingNextMap && !isGameClear) {
            if (player->IsAlive()) {
                player->Update();
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
            if (player->IsAlive() && mapChip->CheckGoalCollision(player->GetPosition(), player->GetHalfSize())) {
                isGameClear = true;
            }
        }


        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        player->ImGui_Draw();

        // 死亡時のリセット処理 (デバッグ用)
        if (!player->IsAlive()) {
            ImGui::Begin("GAME OVER");
            ImGui::Text("You Died!");
            if (ImGui::Button("Reset Game")) {
                player->Reset();
                // すべてのトラップをリセット
                for (Trap* trap : traps_) {
                    trap->Reset();
                }
                // すべての落ちるブロックをリセット
                for (FallingBlock* block : fallingBlocks_) {
                    // ▼▼▼ ★★★ 修正 ★★★ ▼▼▼
                    block->Reset(mapChip); // mapChip を渡す
                    // ▲▲▲ ★★★ 修正 ★★★ ▲▲▲
                }
            }
            ImGui::End();
        }

        // ゲームクリア時の表示
        if (isGameClear) {
            ImGui::Begin("GAME CLEAR");
            ImGui::Text("Congratulations!");
            ImGui::End();
        }


        ImGui::Render();

        const Matrix4x4& viewProjectionMatrix = camera->GetViewProjectionMatrix();
        cameraForGpuData->worldPosition = camera->GetTransform().translate;
        directionalLightData->direction = Normalize(directionalLightData->direction);


        // --- ▼▼▼ マップ遷移処理 ▼▼▼ ---
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
        // --- ▲▲▲ 修正完了 ▲▲▲ ---


        dxCommon->PreDraw();

        commandList->SetGraphicsRootSignature(graphicsPipeline->GetRootSignature());
        ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
        commandList->SetDescriptorHeaps(1, descriptorHeaps);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(4, cameraForGpuResource->GetGPUVirtualAddress());
        commandList->SetPipelineState(graphicsPipeline->GetPipelineState(kBlendModeNone));

        // マップの描画
        // ★ MapChip::Draw は静的ブロック(1)のみ描画 -> block.png を使う
        mapChip->Draw(
            commandList,
            viewProjectionMatrix,
            directionalLightResource->GetGPUVirtualAddress(),
            blockTextureSrvHandleGPU); // ★ cube -> block

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
                cubeTextureSrvHandleGPU); // ★ cube.jpg のテクスチャハンドルを渡す
        }

        // すべての落ちるブロック(3, 4)を描画
        for (FallingBlock* block : fallingBlocks_) {
            // ★ FallingBlock (3, 4) は block.obj, block.png を使う
            block->Draw(
                commandList,
                viewProjectionMatrix,
                directionalLightResource->GetGPUVirtualAddress(),
                blockTextureSrvHandleGPU); // ★ cube -> block
        }

        // ゴール(5)を描画
        if (goalModel_) {
            goalModel_->Draw(
                commandList,
                viewProjectionMatrix,
                directionalLightResource->GetGPUVirtualAddress(),
                cubeTextureSrvHandleGPU); // ★ cube.jpg
        }


        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
        dxCommon->PostDraw();
    }

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    delete mapChip;
    delete player;
    delete playerModel;
    delete graphicsPipeline;
    delete camera;

    // すべてのトラップを解放
    for (Trap* trap : traps_) {
        delete trap;
    }
    traps_.clear();

    // すべての落ちるブロックを解放
    for (FallingBlock* block : fallingBlocks_) {
        delete block;
    }
    fallingBlocks_.clear();

    // ゴールモデルを解放
    delete goalModel_;

    dxCommon->Finalize();
    CoUninitialize();
    winApp->Finalize();
    return 0;
}