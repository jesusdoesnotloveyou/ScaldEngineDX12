#pragma once

#include <queue>

struct KeyboardEvent
{
    enum EventType
    {
        Pressed,
        Released,
        Invalid
    };

    KeyboardEvent()
        : Type(Invalid),
          KeyCode(0u)
    {
    }
    KeyboardEvent(EventType type, unsigned char key)
        : Type(type),
          KeyCode(key)
    {
    }

    bool IsPressed() const { return Type == Pressed; }
    bool IsReleased() const { return Type == Released; }
    bool IsValid() const { return Type != Invalid; }

    EventType Type;
    unsigned char KeyCode;
};

class KeyboardDevice
{
public:
};