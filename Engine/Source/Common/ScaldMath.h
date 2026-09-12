#pragma once

#include <DirectXMath.h>
#include <cstdlib>

namespace Scald
{
    using namespace DirectX;

    template <typename T>
    static T Min(const T& a, const T& b)
    {
        return a < b ? a : b;
    }

    template <typename T>
    static T Max(const T& a, const T& b)
    {
        return a < b ? b : a;
    }

    template <typename T>
    static T Clamp(const T& value, const T& _min, const T& _max)
    {
        return Min(Max(value, _min), _max);
    }

    template <typename T>
    static XMVECTOR SphericalToCarthesian(const T radius, const T theta, const T phi)
    {
        return XMVectorSet(radius * sinf(phi) * cosf(theta), radius * cosf(phi), radius * sinf(phi) * sinf(theta), 1.0f);
    }

    static inline XMMATRIX Inverse4x4(const XMMATRIX& mat)
    {
        auto det = XMMatrixDeterminant(mat);
        return XMMatrixInverse(&det, mat);
    }

    // Returns random float in [0, 1).
    static float RandF()
    {
        return (float)(rand()) / (float)RAND_MAX;
    }

    // Returns random float in [a, b).
    static float RandF(float a, float b)
    {
        return a + RandF() * (b - a);
    }

    static inline const XMVECTOR ForwardVector = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    static inline const XMVECTOR RightVector = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    static inline const XMVECTOR UpVector = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    static inline const XMVECTOR ZeroVector = XMVectorZero();
    static inline const XMVECTOR One = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
    }  // namespace Scald