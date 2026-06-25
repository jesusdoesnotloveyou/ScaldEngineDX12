#pragma once

namespace Scald
{
    // See boost implementation: https://www.boost.org/doc/libs/1_41_0/boost/noncopyable.hpp
    class NonCopyable
    {
    protected:
        NonCopyable() { }
        ~NonCopyable() { }

    private:
        NonCopyable(const NonCopyable& lhs) = delete;
        NonCopyable& operator=(const NonCopyable& lhs) = delete;

        // NonMovable
        NonCopyable(NonCopyable&& rhs) = delete;
        NonCopyable& operator=(NonCopyable&& rhs) = delete;
    };
}

// Platform-specific break macro
// __nop() is a no-operation intrinsic that can be used to prevent the compiler from optimizing away the debug break instruction.
#define PLATFORM_BREAK() (__nop(), __debugbreak());