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

    // Load() を device 付きでここで呼び出す
    mapChip->Load("Resources/map.csv", device);

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

    // メインループ
    while (!winApp->IsEndRequested()) {
        winApp->ProcessMessage();
        Input::GetInstance()->Update();
        player->Update();

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        player->ImGui_Draw();

        // camera->ImGui_Draw() の呼び出しを削除済み

        ImGui::Render();

        // camera->UpdateMatrix() の呼び出しを削除済み

        // 行列は Camera から取得するだけ
        const Matrix4x4& viewProjectionMatrix = camera->GetViewProjectionMatrix();

        // GPUに送るカメラ座標を更新
        cameraForGpuData->worldPosition = camera->GetTransform().translate;

        directionalLightData->direction = Normalize(directionalLightData->direction);
        dxCommon->PreDraw();

        commandList->SetGraphicsRootSignature(graphicsPipeline->GetRootSignature());
        ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
        commandList->SetDescriptorHeaps(1, descriptorHeaps);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(4, cameraForGpuResource->GetGPUVirtualAddress()); // カメラ座標CBVはそのまま
        commandList->SetPipelineState(graphicsPipeline->GetPipelineState(kBlendModeNone));

        // マップの描画 (取得した viewProjectionMatrix を使う)
        mapChip->Draw(
            commandList,
            viewProjectionMatrix, // ◀ Cameraから取得した行列
            directionalLightResource->GetGPUVirtualAddress(),
            blockTextureSrvHandleGPU);

        // プレイヤーの描画 (取得した viewProjectionMatrix を使う)
        player->Draw(
            commandList,
            viewProjectionMatrix, // ◀ Cameraから取得した行列
            directionalLightResource->GetGPUVirtualAddress(),
            playerTextureSrvHandleGPU);

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
    delete camera; // Camera を delete する
    dxCommon->Finalize();
    CoUninitialize();
    winApp->Finalize();
    return 0;
}