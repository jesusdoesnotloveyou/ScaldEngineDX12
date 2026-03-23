#include "stdafx.h"
#include "MousePad.h"

void MousePad::OnLeftPressed(const int x, const int y) noexcept
{
    bIsLeftPressed = true;
    m_mouseBuffer.push(MouseEvent(MouseEvent::LPressed, x, y));
    TrimBuffer();
}

void MousePad::OnLeftReleased(const int x, const int y) noexcept
{
    bIsLeftPressed = false;
    m_mouseBuffer.push(MouseEvent(MouseEvent::LReleased, x, y));
    TrimBuffer();
}

void MousePad::OnRightPressed(const int x, const int y) noexcept
{
    bIsRightPressed = true;
    m_mouseBuffer.push(MouseEvent(MouseEvent::RPressed, x, y));
    TrimBuffer();
}

void MousePad::OnRightReleased(const int x, const int y) noexcept
{
    bIsRightPressed = false;
    m_mouseBuffer.push(MouseEvent(MouseEvent::RReleased, x, y));
    TrimBuffer();
}

void MousePad::OnMiddlePressed(const int x, const int y) noexcept
{
    bIsMiddlePressed = true;
    m_mouseBuffer.push(MouseEvent(MouseEvent::MPressed, x, y));
    TrimBuffer();
}

void MousePad::OnMiddleReleased(const int x, const int y) noexcept
{
    bIsMiddlePressed = false;
    m_mouseBuffer.push(MouseEvent(MouseEvent::MReleased, x, y));
    TrimBuffer();
}

void MousePad::OnWheelDelta(const int x, const int y, const int delta) noexcept
{
    wheelDeltaCarry += delta;
    // generate events for every 120
    while (wheelDeltaCarry >= WHEEL_DELTA)
    {
        wheelDeltaCarry -= WHEEL_DELTA;
        OnWheelUp(x, y);
    }
    while (wheelDeltaCarry <= -WHEEL_DELTA)
    {
        wheelDeltaCarry += WHEEL_DELTA;
        OnWheelDown(x, y);
    }
    std::wstring debugMsg = L"Wheel delta carry: " + std::to_wstring(wheelDeltaCarry) + L"\n";
    OutputDebugString(debugMsg.c_str());
}

void MousePad::OnWheelUp(const int x, const int y) noexcept
{
    m_mouseBuffer.push(MouseEvent(MouseEvent::WheelUp, x, y));
    TrimBuffer();
}

void MousePad::OnWheelDown(const int x, const int y) noexcept
{
    m_mouseBuffer.push(MouseEvent(MouseEvent::WheelDown, x, y));
    TrimBuffer();
}

void MousePad::OnMouseEnter() noexcept
{
    bIsInWindow = true;
    m_mouseBuffer.push(MouseEvent(MouseEvent::Enter, this->x, this->y));
    TrimBuffer();
}

void MousePad::OnMouseLeave() noexcept
{
    bIsInWindow = false;
    m_mouseBuffer.push(MouseEvent(MouseEvent::Leave, this->x, this->y));
    TrimBuffer();
}

void MousePad::OnMouseMove(const int x, const int y) noexcept
{
    // Update mouse coordinates
    this->x = x;
    this->y = y;
    m_mouseBuffer.push(MouseEvent(MouseEvent::Move, this->x, this->y));
    TrimBuffer();
}

void MousePad::OnMouseMoveRaw(const int x, const int y) noexcept
{
    m_mouseBuffer.push(MouseEvent(MouseEvent::RawMove, x, y));
    TrimBuffer();
}

MouseEvent MousePad::ReadEvent() noexcept
{
    if (m_mouseBuffer.size() > 0u)
    {
        MouseEvent e = m_mouseBuffer.front();
        m_mouseBuffer.pop();
        return e;
    }
    return MouseEvent{};
}

void MousePad::TrimBuffer() noexcept
{
    while (m_mouseBuffer.size() > bufferSize)
    {
        m_mouseBuffer.pop();
    }
}

void MousePad::Flush() noexcept
{
    m_mouseBuffer = std::queue<MouseEvent>{};
}