// Copyright (C) 2019 Charles Seizilles de Mazancourt
// This file is part of the "Nazara Engine - Platform module"
// For conditions of distribution and use, see copyright notice in Config.hpp

#include <Nazara/Platform/Menu.hpp>

#if defined(NAZARA_PLATFORM_WINDOWS)
#include <Nazara/Platform/Win32/MenuImpl.hpp>
#elif defined(NAZARA_PLATFORM_X11)
#include <Nazara/Platform/X11/MenuImpl.hpp>
#else
#error Lack of implementation: Menu
#endif

#include <Nazara/Platform/Debug.hpp>

namespace Nz
{
	Menu::Menu() :
	m_impl(new MenuImpl(*this))
	{
	}

	MenuButton& Menu::AppendButton(String name)
	{
		return m_impl->AppendButton(std::move(name));
	}

	void Menu::AppendSeparator()
	{
		m_impl->AppendSeparator();
	}

	SubMenu& Menu::AppendSubMenu(String name)
	{
		return m_impl->AppendSubMenu(std::move(name));
	}

	MenuHandle Menu::GetHandle() const
	{
		return m_impl->GetHandle();
	}

	bool Menu::IsAssigned() const
	{
		return m_impl->IsAssigned();
	}

	//bool Menu::IsDirty() const
	//{
	//	return m_impl->IsDirty();
	//}

	void Menu::SetAssigned(bool assigned)
	{
		m_impl->SetAssigned(assigned);
	}

	//void Menu::SetDirty(bool dirty)
	//{
	//	m_impl->SetDirty(dirty);
	//}

	MenuItem::MenuItem(MenuItemType type) :
		m_type(type)
	{
	}

	MenuItemType MenuItem::GetType() const
	{
		return m_type;
	}

	MenuButton::MenuButton(String name, Menu& menu, MenuHandle parentMenu, uint32_t index) :
	m_impl(new MenuButtonImpl(name, menu, parentMenu, index)),
	MenuItem(MenuItemType_Button)
	{
	}

	const String& MenuButton::GetName() const
	{
		return m_impl->GetName();
	}

	bool MenuButton::IsChecked() const
	{
		return m_impl->IsChecked();
	}

	bool MenuButton::IsEnabled() const
	{
		return m_impl->IsEnabled();
	}

	void MenuButton::SetChecked(bool checked)
	{
		m_impl->SetChecked(checked);
	}

	void MenuButton::SetEnabled(bool enabled)
	{
		m_impl->SetEnabled(enabled);
	}

	void MenuButton::SetName(String name)
	{
		m_impl->SetName(name);
	}

	SubMenu::SubMenu(String name, Menu& menu, MenuHandle parentMenu, uint32_t index) :
	m_impl(new SubMenuImpl(name, menu, parentMenu, index)),
	MenuItem(MenuItemType_Button)
	{
	}

	MenuButton& SubMenu::AppendButton(String name)
	{
		return m_impl->AppendButton(std::move(name));
	}

	void SubMenu::AppendSeparator()
	{
		m_impl->AppendSeparator();
	}

	SubMenu& SubMenu::AppendSubMenu(String name)
	{
		return m_impl->AppendSubMenu(std::move(name));
	}

	MenuHandle SubMenu::GetHandle() const
	{
		return m_impl->GetHandle();
	}

	const String& SubMenu::GetName() const
	{
		return m_impl->GetName();
	}
}
