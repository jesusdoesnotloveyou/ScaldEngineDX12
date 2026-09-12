#pragma once

#include "ShadowMap.h"

namespace Scald
{
    class Device;

    class CascadeShadowMap final : public ShadowMap
    {
    public:
        CascadeShadowMap(Device* device, UINT width, UINT height, UINT cascadesCount);
        CascadeShadowMap(const CascadeShadowMap& lhs) = delete;
        CascadeShadowMap& operator=(const CascadeShadowMap& lhs) = delete;

        virtual ~CascadeShadowMap() noexcept override;

        virtual void CreateViews() override;

    private:
        void CreateResource();
    };
}  // namespace Scald