// Copyright (C) 2019 Charles Seizilles de Mazancourt
// This file is part of the "Nazara Engine - Platform module"
// For conditions of distribution and use, see copyright notice in Config.hpp

#include <Nazara/Platform/X11/MenuImpl.hpp>
#include <Nazara/Platform/Config.hpp>
#include <Nazara/Platform/Debug.hpp>

namespace Nz
{
	MenuImpl::MenuImpl(Menu& menu) :
	m_menu(menu)
	{
	}

	MenuImpl::~MenuImpl()
	{
	}

	MenuButton& MenuImpl::AppendButton(String name)
	{
		static MenuButton btn(std::move(name), m_menu, nullptr, 0);

		return btn;
	}

	void MenuImpl::AppendSeparator()
	{
	}

	SubMenu& MenuImpl::AppendSubMenu(String name)
	{
		static SubMenu subMenu(std::move(name), m_menu, nullptr, 0);

		return subMenu;
	}

	MenuHandle MenuImpl::GetHandle() const
	{
		return nullptr;
	}

	bool MenuImpl::IsAssigned() const
	{
		return false;
	}

	void MenuImpl::PressButtonAt(size_t index)
	{
	}

	void MenuImpl::SetAssigned(bool assigned)
	{
	}

	MenuButtonImpl::MenuButtonImpl(String name, Menu & menu, MenuHandle parentMenu, uint32_t index)
	{
	}

	const String& MenuButtonImpl::GetName() const
	{
		static String str = "";
		return str;
	}

	bool MenuButtonImpl::IsChecked() const
	{
		return false;
	}

	bool MenuButtonImpl::IsEnabled() const
	{
		return false;
	}

	void MenuButtonImpl::SetChecked(bool checked)
	{
	}

	void MenuButtonImpl::SetEnabled(bool enabled)
	{
	}

	void MenuButtonImpl::SetName(String name)
	{
	}

	SubMenuImpl::SubMenuImpl(String name, Menu& menu, MenuHandle parentMenu, uint32_t index) :
	m_menu(menu)
	{
	}

	MenuButton& SubMenuImpl::AppendButton(String name)
	{
		static MenuButton btn(std::move(name), m_menu, nullptr, 0);

		return btn;
	}

	void SubMenuImpl::AppendSeparator()
	{
	}

	SubMenu& SubMenuImpl::AppendSubMenu(String name)
	{
		static SubMenu subMenu(std::move(name), m_menu, nullptr, 0);

		return subMenu;
	}

	MenuHandle SubMenuImpl::GetHandle() const
	{
		return nullptr;
	}

	const String& SubMenuImpl::GetName() const
	{
		static String str = "";
		return str;
	}

	void SubMenuImpl::PressButtonAt(size_t index)
	{
	}
}
