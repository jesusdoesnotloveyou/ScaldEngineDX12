#pragma once

#include "DXHelper.h"
#include "Utility.h"

#include <memory>

namespace Scald
{
class Device;
class SwapChain;

class GraphicsContext final : NonCopyable
{
    friend class D3D12Sample;

private:
    GraphicsContext() = default;
    ~GraphicsContext() = default;

public:
    std::unique_ptr<Device> m_device;
    std::unique_ptr<SwapChain> m_swapChain;
    // queues
};
}  // namespace Scald