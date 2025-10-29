#pragma once

#include "SComponent.h"

namespace Scald
{
	class SObject;

	class Transform : public SComponent
	{
		using Super = SComponent;
	public:
		Transform(std::shared_ptr<SObject> owner, XMVECTOR pos, XMVECTOR rot, XMVECTOR scale);
		virtual ~Transform() noexcept override;

		FORCEINLINE XMVECTOR GetScale()
		{
			return XMLoadFloat3(&m_scale);
		}

		FORCEINLINE XMVECTOR GetEulerRot()
		{
			return XMLoadFloat3(&m_euler);
		}

		FORCEINLINE XMVECTOR GetOrientation()
		{
			return XMLoadFloat4(&m_orient);
		}
		
		FORCEINLINE XMVECTOR GetTranslation()
		{
			return XMLoadFloat3(&m_translation);
		}

	public:
		virtual void OnUpdate() override;
		virtual void OnAttach() override;
		virtual void OnDestroy() override;


	private:
		XMFLOAT3 m_scale;
		XMFLOAT3 m_euler;
		XMFLOAT4 m_orient;
		XMFLOAT3 m_translation;
	};
}