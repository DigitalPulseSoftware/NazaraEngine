// Copyright (C) 2017 Samy Bensaid
// This file is part of the "Nazara Development Kit"
// For conditions of distribution and use, see copyright notice in Prerequesites.hpp

namespace Ndk
{

	inline void CheckboxWidget::EnableAdaptativeMargin(bool enable)
	{
		m_adaptativeMargin = enable;
		Layout();
	}

	inline void CheckboxWidget::EnableCheckbox(bool enable)
	{
		m_checkboxEnabled = enable;
		UpdateCheckboxSprite();
	}

	inline void CheckboxWidget::EnableTristate(bool enable)
	{
		m_tristateEnabled = enable;
	}


	inline bool CheckboxWidget::IsCheckboxEnabled() const
	{
		return m_checkboxEnabled;
	}

	inline bool CheckboxWidget::IsMarginAdaptative() const
	{
		return m_adaptativeMargin;
	}

	inline bool CheckboxWidget::IsTristateEnabled() const
	{
		return m_tristateEnabled;
	}


	inline const Nz::Vector2f& CheckboxWidget::GetCheckboxSize() const
	{
		return m_checkboxSprite->GetSize();
	}

	inline Nz::Vector2f CheckboxWidget::GetCheckboxBorderSize() const
	{
		return GetCheckboxSize() / s_borderScale;
	}

	inline CheckboxState CheckboxWidget::GetState() const
	{
		return m_state;
	}

	inline float CheckboxWidget::GetTextMargin() const
	{
		return m_textMargin;
	}


	inline void CheckboxWidget::SetCheckboxSize(const Nz::Vector2f& size)
	{
		m_checkboxSprite->SetSize(size);
		m_checkboxContentSprite->SetSize(GetCheckboxSize() - GetCheckboxBorderSize() * 3.f);
		Layout();
	}

	inline void CheckboxWidget::SetTextMargin(float margin)
	{
		m_textMargin = margin;
		Layout();
	}


	inline void CheckboxWidget::UpdateText(const Nz::AbstractTextDrawer& drawer)
	{
		m_textSprite->Update(drawer);
		Layout();
	}

	inline bool CheckboxWidget::ContainsCheckbox(int x, int y) const
	{
		Nz::Vector2f checkboxSize = GetCheckboxSize();
		Nz::Vector3f pos = m_checkboxEntity->GetComponent<NodeComponent>().GetPosition(Nz::CoordSys_Local);

		return x > pos.x && x < pos.x + checkboxSize.x &&
		       y > pos.y && y < pos.y + checkboxSize.y;
	}
}