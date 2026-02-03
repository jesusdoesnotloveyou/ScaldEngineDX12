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

		FORCEINLINE XMVECTOR GetScale() const
		{
			return XMLoadFloat3(&m_scale);
		}

		void SetScale(const XMVECTOR& scale)
		{
			XMStoreFloat3(&m_scale, scale);
			m_bIsDirty = true;
		}

		void SetScale(const XMFLOAT3& scale)
		{
			m_scale = scale;
			m_bIsDirty = true;
		}

		FORCEINLINE XMVECTOR GetEulerRot() const
		{
			return XMLoadFloat3(&m_euler);
		}

		FORCEINLINE XMVECTOR GetOrientation() const
		{
			return XMLoadFloat4(&m_orient);
		}
		
		FORCEINLINE XMVECTOR GetTranslation() const
		{
			return XMLoadFloat3(&m_translation);
		}

		void SetTranslation(const XMVECTOR& translation)
		{
			XMStoreFloat3(&m_translation, translation);
			m_bIsDirty = true;
		}

		void SetTranslation(const XMFLOAT3& translation)
		{
			m_translation = translation;
			m_bIsDirty = true;
		}

		const XMMATRIX& GetWorld() const
		{
			return m_world;
		}


	public:
		virtual void OnUpdate() override;
		virtual void OnAttach() override;
		virtual void OnDestroy() override;


	private:
		XMMATRIX m_world;

		XMFLOAT4 m_orient;
		XMFLOAT3 m_euler;
		XMFLOAT3 m_translation;
		XMFLOAT3 m_scale;

		bool m_bIsDirty = true;
	};
}