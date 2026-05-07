#pragma once

#include <queue>

struct MousePoint
{
	int x;
	int y;
};

struct MouseEvent
{
	enum EventType
	{
		RPressed,
		RReleased,
		LPressed,
		LReleased,
		MPressed,
		MReleased,
		WheelUp,
		WheelDown,
		Enter,
		Move,
		Leave,
		RawMove,
		Invalid
	};

	MouseEvent() : Type(Invalid), x(0), y(0) {}
	MouseEvent(const EventType type, const int x, const int y) : Type(type), x(x), y(y) {}

	EventType GetType() const { return Type; }
	bool IsValid() const { return Type != Invalid; }
	MousePoint GetPos() const { return MousePoint{ x, y }; }
	int GetPosX() const { return x; }
	int GetPosY() const { return y; }

private:
	EventType Type;
	int x;
	int y;
};

class MousePad
{
public:
	MousePad() = default;
	MousePad(const MousePad&) = delete;

public:
	void OnLeftPressed(const int x, const int y) noexcept;
	void OnLeftReleased(const int x, const int y) noexcept;
	void OnRightPressed(const int x, const int y) noexcept;
	void OnRightReleased(const int x, const int y) noexcept;
	void OnMiddlePressed(const int x, const int y) noexcept;
	void OnMiddleReleased(const int x, const int y) noexcept;
	void OnWheelDelta(const int x, const int y, const int delta) noexcept;
private:
	void OnWheelUp(const int x, const int y) noexcept;
	void OnWheelDown(const int x, const int y) noexcept;
public:
	void OnMouseEnter() noexcept;
	void OnMouseLeave() noexcept;
	
	void OnMouseMove(const int x, const int y) noexcept;
	void OnMouseMoveRaw(const int x, const int y) noexcept;

	FORCEINLINE MousePoint GetPos() const noexcept
	{
		return MousePoint{ x, y };
	}

	FORCEINLINE int GetPosX() const noexcept { return x; }
	FORCEINLINE int GetPosY() const noexcept { return y; }

	FORCEINLINE bool IsInWindow() const noexcept { return bIsInWindow; }
	FORCEINLINE bool IsLeftPressed() const noexcept { return bIsLeftPressed; }
	FORCEINLINE bool IsMiddlePressed() const noexcept { return bIsMiddlePressed; }
	FORCEINLINE bool IsRightPressed() const noexcept { return bIsRightPressed; }

	MouseEvent ReadEvent() noexcept;
	void TrimBuffer() noexcept;
	void Flush() noexcept;

	FORCEINLINE bool IsEventBufferEmpty() const { return m_mouseBuffer.empty(); }

private:
	static constexpr unsigned int bufferSize = 16u;
	
	int x = 0;
	int y = 0;
	int deltaX = 0;
	int deltaY = 0;
	
	bool bIsLeftPressed = false;
	bool bIsRightPressed = false;
	bool bIsMiddlePressed = false;
	bool bIsInWindow = false;

	int wheelDeltaCarry = 0;
	
	std::queue<MouseEvent> m_mouseBuffer;
};