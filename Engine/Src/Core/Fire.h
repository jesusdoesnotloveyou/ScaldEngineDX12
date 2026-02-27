#pragma once

#include "ParticleSystem.h"

class FireParticleSystem final : ParticleSystem
{
public:
    FireParticleSystem(ID3D11Device* device, ID3D11DeviceContext* deviceContext, int maxParticles, XMVECTOR origin, class Camera* camera);
    virtual void Update(float elapsedTime) override;
    virtual void Simulate(float elapsedTime) override;
    virtual void Render() override;
    virtual void Emit(int numParticles) override;
    virtual void InitializeSystem() override;
    virtual ~FireParticleSystem() noexcept override;

protected:
    virtual void InitializeParticle(int index) override;

private:
    VertexShader mFireVertexShader;
    PixelShader mFirePixelShader;
    ComputeShader mFireSimulateShader;
    ComputeShader mFireEmitShader;
};