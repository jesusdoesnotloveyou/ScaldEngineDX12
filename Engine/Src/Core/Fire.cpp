#include "stdafx.h"
#include "Fire.h"

FireParticleSystem::FireParticleSystem(ID3D11Device* device, ID3D11DeviceContext* deviceContext, int maxParticles, XMVECTOR origin, class Camera* camera)
	: ParticleSystem(device, deviceContext, maxParticles, origin, camera)
{

}

void FireParticleSystem::Update(float elapsedTime)
{
	Emit(16);
	Simulate(elapsedTime);
	SortParticles();
}

void FireParticleSystem::Render()
{
	ParticleSystem::Render();

	mCameraData.gView = XMMatrixTranspose(camera->GetViewMatrix());
	mCameraData.gProjection = XMMatrixTranspose(camera->GetPerspectiveProjectionMatrix());
	mCameraData.gForward = camera->GetForwardVector();
	mCameraData.gUp = camera->GetUpVector();
	mCBCamera.SetAndApplyData(mCameraData);

	mDeviceContext->CopyStructureCount(indirectArgsBuffer, 0u, mSortListBufferUAV.Get());

	mDeviceContext->IASetInputLayout(mFireVertexShader.GetInputLayout());
	mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	mDeviceContext->VSSetShaderResources(0u, 1u, mSortListBufferSRV.GetAddressOf());
	mDeviceContext->VSSetShader(mFireVertexShader.Get(), nullptr, 0u);

	mDeviceContext->GSSetShaderResources(0u, 1u, mParticleBufferSRV.GetAddressOf());
	mDeviceContext->GSSetShader(mBillboardGeometryShader.Get(), nullptr, 0u);
	mDeviceContext->GSSetConstantBuffers(0u, 1u, mCBCamera.GetAddressOf());

	mDeviceContext->PSSetShader(mFirePixelShader.Get(), nullptr, 0u);

	mDeviceContext->DrawInstancedIndirect(indirectArgsBuffer, 0u);
}

static bool firstEmit = true;
void FireParticleSystem::Emit(int numParticles)
{
	ParticleSystem::Emit(numParticles);

	ApplyChanges(mDeviceContext, injectionBuffer, injectionBufferSize, injectionParticleData);

	mParticleData.maxNumParticles = maxParticles;
	mParticleData.numEmitInThisFrame = numParticles;
	mParticleData.gEmitPos = origin;
	mParticleData.gEyePos = camera->GetPosition();

	if (firstEmit) mParticleData.numAliveParticles = 0u;
	mCBParticle.SetAndApplyData(mParticleData);

	if (!firstEmit)
	{
		mDeviceContext->CopyStructureCount(mCBParticle.Get(), 12, mSortListBufferUAV.Get());
	}

	UINT minOne = (UINT)-1;
	UINT zero = 0u;

	mDeviceContext->CSSetShader(mFireEmitShader.Get(), nullptr, 0u);
	mDeviceContext->CSSetUnorderedAccessViews(0u, 1u, mParticleBufferUAV.GetAddressOf(), nullptr);

	if (firstEmit)
	{
		mDeviceContext->CSSetUnorderedAccessViews(1u, 1u, mSortListBufferUAV.GetAddressOf(), &zero);
		mDeviceContext->CSSetUnorderedAccessViews(2u, 1u, mDeadListBufferUAV.GetAddressOf(), &maxParticles);
		firstEmit = false;
	}
	else
	{
		mDeviceContext->CSSetUnorderedAccessViews(1u, 1u, mSortListBufferUAV.GetAddressOf(), &minOne);
		mDeviceContext->CSSetUnorderedAccessViews(2u, 1u, mDeadListBufferUAV.GetAddressOf(), &minOne);
	}

	mDeviceContext->CSSetConstantBuffers(0u, 1u, mCBParticle.GetAddressOf());
	mDeviceContext->CSSetShaderResources(0u, 1u, mInjectionBufferSRV.GetAddressOf());
	mDeviceContext->Dispatch(1u, 1u, 1u);

	ID3D11UnorderedAccessView* nullUAV = nullptr;
	mDeviceContext->CSSetUnorderedAccessViews(0u, 1u, &nullUAV, nullptr);
	mDeviceContext->CSSetUnorderedAccessViews(1u, 1u, &nullUAV, nullptr);
	mDeviceContext->CSSetUnorderedAccessViews(2u, 1u, &nullUAV, nullptr);
	mParticleData.numEmitInThisFrame = 0u;
}

void FireParticleSystem::InitializeSystem()
{
	ParticleSystem::InitializeSystem();

	D3D11_INPUT_ELEMENT_DESC inputLayoutParticlesDesc[] = {
		D3D11_INPUT_ELEMENT_DESC {
			"SV_VertexID",
			0u,
			DXGI_FORMAT_R32_UINT,
			0u,
			0u,
			D3D11_INPUT_PER_VERTEX_DATA,
			0u}
	};

	ThrowIfFailed(mFireVertexShader.Init(mDevice, inputLayoutParticlesDesc, (UINT)std::size(inputLayoutParticlesDesc), L"./Shaders/particles/fire/ParticlesVS.hlsl"));
	ThrowIfFailed(mFirePixelShader.Init(mDevice, L"./Shaders/particles/fire/ParticlesPS.hlsl"));
	ThrowIfFailed(mFireSimulateShader.Init(mDevice, L"./Shaders/particles/fire/SimulateCS.hlsl"));
	ThrowIfFailed(mFireEmitShader.Init(mDevice, L"./Shaders/particles/fire/EmitCS.hlsl"));
}

FireParticleSystem::~FireParticleSystem() noexcept
{
}

void FireParticleSystem::InitializeParticle(int index)
{
	if (index < 0 || index >= injectionBufferSize) return;

	Particle p;
	p.acceleration = XMVectorSet(0.0f, 4.0f * -0.981f, 0.0f, 0.0f);
	p.initialColor = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	p.endColor = XMVectorSet(1.0f, 1.0f, 0.8f, 1.0f); //randColor();
	p.initialSize = 0.02f;
	p.endSize = 0.04f;
	p.initialWeight = 1.0f;
	p.endWeight = 1.0f;
	p.lifeTime = 0.0f;
	p.maxLifeTime = 3.0f;
	p.pos = origin;
	p.prevPos = origin;

	float rf = generateRandom(0.0f, 1.0f * XM_2PI);
	p.velocity = generateRandom(0.3f, 2.0f) * XMVectorSet(std::sin(rf), generateRandom(0.0f, 1.0f), std::cos(rf), 0.0f);

	injectionParticleData[index] = p;
}

void FireParticleSystem::Simulate(float elapsedTime)
{
	mParticleData.deltaTime = elapsedTime;
	mParticleData.maxNumParticles = maxParticles;
	mParticleData.gEyePos = camera->GetPosition();
	mCBParticle.SetAndApplyData(mParticleData);

	// numAliveParticles starts from 12th byte
	mDeviceContext->CopyStructureCount(mCBParticle.Get(), 12u, mSortListBufferUAV.Get());

	UINT minOne = (UINT)-1;
	UINT zero = 0;

	mDeviceContext->CSSetUnorderedAccessViews(0u, 1u, mParticleBufferUAV.GetAddressOf(), nullptr);
	mDeviceContext->CSSetUnorderedAccessViews(1u, 1u, mSortListBufferUAV.GetAddressOf(), &minOne);
	mDeviceContext->CSSetUnorderedAccessViews(2u, 1u, mDeadListBufferUAV.GetAddressOf(), &minOne);

	mDeviceContext->CSSetShader(mFireSimulateShader.Get(), nullptr, 0u);
	mDeviceContext->CSSetConstantBuffers(0u, 1u, mCBParticle.GetAddressOf());
	mDeviceContext->Dispatch(32u, 32u, 1u);

	ID3D11UnorderedAccessView* nullUAV = nullptr;
	mDeviceContext->CSSetUnorderedAccessViews(0u, 1u, &nullUAV, nullptr);
	mDeviceContext->CSSetUnorderedAccessViews(1u, 1u, &nullUAV, nullptr);
	mDeviceContext->CSSetUnorderedAccessViews(2u, 1u, &nullUAV, nullptr);
}