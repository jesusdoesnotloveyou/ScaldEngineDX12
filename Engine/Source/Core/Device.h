#pragma once

#include "DXHelper.h"

#include <cstdint>
#include <memory>

struct ID3D12Device2;
struct IDXGIFactory4;
struct IDXGIAdapter1;

namespace Scald
{
using namespace Microsoft::WRL;

class SwapChain;
class CommandQueue;

enum class QueueType : uint8_t
{
    Direct,
    Copy,
    Compute,
    QueueTypes = 3
};

class Device final : std::enable_shared_from_this<Device>
{
    friend class GraphicsContext;
    Device(bool bUseWarpAdapter);

public:
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    void CreateDebugLayer();
    static std::unique_ptr<Device> Create(bool bUseWarpAdapter = false);
    std::unique_ptr<SwapChain> CreateSwapChain(HWND hWnd, uint32_t width, uint32_t height, DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R10G10B10A2_UNORM);
    
    void CreateCommandObjectsAndInternalFences();

    ComPtr<ID3D12Device2> GetDevice2() const { return m_device; }
    ComPtr<IDXGIFactory4> GetFactory() const { return m_factory; }
    ComPtr<IDXGIAdapter1> GetDXGIAdapter() const { return m_hardwareAdapter; }
    std::shared_ptr<CommandQueue> GetCommandQueue(D3D12_COMMAND_LIST_TYPE commandListType = D3D12_COMMAND_LIST_TYPE_DIRECT) const;

    void Flush();
    
    uint32_t GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE heapType) const;

private:
    void CreateCommandQueues();
    void CreateCommandAllocators();
    void CreateCommandLists();

private:
    void LogAdapters();
    void LogAdapterOutputs(IDXGIAdapter* adapter);
    void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format = DXGI_FORMAT_R10G10B10A2_UNORM);

    void GetHardwareAdapter(_In_ IDXGIFactory1* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter = false);
    void CheckFeatureSupport();

private:
    static inline uint32_t m_dxgiFactoryFlags = 0u;
    bool m_useWarpDevice = false;
    BOOL UMA = FALSE;

    std::unique_ptr<CommandQueue> m_directQueue;
    std::unique_ptr<CommandQueue> m_copyQueue;
    std::unique_ptr<CommandQueue> m_computeQueue;

    // Adapter info.
    ComPtr<IDXGIAdapter3> m_hardwareAdapter;
    // DXGI factory.
    ComPtr<IDXGIFactory4> m_factory = nullptr;
    // D3D12 device itself.
    ComPtr<ID3D12Device2> m_device = nullptr;
    // ComPtr<ID3D12Device4> m_device4 = nullptr; // For RT stuff

    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;

    // Could be cached.
    uint32_t m_rtvDescriptorSize;
    uint32_t m_dsvDescriptorSize;
    uint32_t m_cbvSrvUavDescriptorSize;
};
}  // namespace Scald