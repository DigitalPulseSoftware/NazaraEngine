// Copyright (C) 2019 Charles Seizilles de Mazancourt
// This file is part of the "Nazara Engine - Platform module"
// For conditions of distribution and use, see copyright notice in Config.hpp

#include <Nazara/Platform/Win32/MenuImpl.hpp>
#include <Nazara/Platform/Config.hpp>
#include <cstdio>
#include <memory>
#include <windowsx.h>
#include <Nazara/Platform/Debug.hpp>

namespace Nz
{
	MenuImpl::MenuImpl(Menu& menu) :
	m_menu(menu),
	m_assigned(false)//,
	//m_dirty(true)
	{
		m_handle = ::CreateMenu();

		MENUINFO menuInfo = {};
		menuInfo.cbSize = sizeof(menuInfo);
		menuInfo.fMask = MIM_STYLE;

		GetMenuInfo(m_handle, &menuInfo);
		menuInfo.fMask = MIM_STYLE | MIM_MENUDATA;
		menuInfo.dwStyle |= MNS_NOTIFYBYPOS;
		menuInfo.dwMenuData = (ULONG_PTR)this;
		SetMenuInfo(m_handle, &menuInfo);
	}

	MenuImpl::~MenuImpl()
	{
		if (!m_assigned)
			::DestroyMenu(m_handle);
	}

	MenuButton& MenuImpl::AppendButton(String name)
	{
		//m_menu.SetDirty();
		::AppendMenuW(m_handle, 0, 0, name.GetWideString().c_str());
		auto menuButton = std::make_unique<MenuButton>(std::move(name), m_menu, m_menu.GetHandle(), static_cast<uint32_t>(m_items.size()));
		m_items.push_back(std::move(menuButton));
		return *reinterpret_cast<MenuButton*>(m_items.back().get());
	}

	void MenuImpl::AppendSeparator()
	{
		//m_menu.SetDirty();
		::AppendMenuW(m_handle, MF_SEPARATOR, 0, nullptr);
		m_items.push_back(nullptr);
	}

	SubMenu& MenuImpl::AppendSubMenu(String name)
	{
		//m_menu.SetDirty();
		auto subMenu = std::make_unique<SubMenu>(std::move(name), m_menu, m_menu.GetHandle(), static_cast<uint32_t>(m_items.size()));
		::AppendMenuW(m_handle, MF_POPUP, reinterpret_cast<UINT_PTR>(subMenu->GetHandle()), subMenu->GetName().GetWideString().c_str());
		m_items.push_back(std::move(subMenu));
		return *reinterpret_cast<SubMenu*>(m_items.back().get());
	}

	MenuHandle MenuImpl::GetHandle() const
	{
		return static_cast<MenuHandle>(m_handle);
	}

	bool MenuImpl::IsAssigned() const
	{
		return m_assigned;
	}

	//bool MenuImpl::IsDirty() const
	//{
	//	return m_dirty;
	//}

	void MenuImpl::PressButtonAt(size_t index)
	{
		if (index < m_items.size())
		{
			if (m_items[index] != nullptr)
			{
				if (m_items[index]->GetType() == MenuItemType_Button)
					reinterpret_cast<MenuButton*>(m_items[index].get())->OnPressed(*reinterpret_cast<MenuButton*>(m_items[index].get()));
			}
		}
	}

	void MenuImpl::SetAssigned(bool assigned)
	{
		m_assigned = assigned;
	}

	//void MenuImpl::SetDirty(bool dirty)
	//{
	//	m_dirty = dirty;
	//}

	MenuButtonImpl::MenuButtonImpl(String name, Menu & menu, MenuHandle parentMenu, uint32_t index) :
	m_name(std::move(name)),
	m_menu(menu),
	m_parentMenu(parentMenu),
	m_index(index),
	m_checked(false),
	m_enabled(false)
	{
	}

	const String& MenuButtonImpl::GetName() const
	{
		return m_name;
	}

	bool MenuButtonImpl::IsChecked() const
	{
		return m_checked;
	}

	bool MenuButtonImpl::IsEnabled() const
	{
		return m_enabled;
	}

	void MenuButtonImpl::SetChecked(bool checked)
	{
		m_checked = checked;

		CheckMenuItem(static_cast<HMENU>(m_parentMenu), m_index, MF_BYPOSITION | (MF_CHECKED * checked));
	}

	void MenuButtonImpl::SetEnabled(bool enabled)
	{
		m_enabled = enabled;

		EnableMenuItem(static_cast<HMENU>(m_parentMenu), m_index, MF_BYPOSITION | (MF_DISABLED * !enabled));
	}

	void MenuButtonImpl::SetName(String name)
	{
		m_name = std::move(name);
		ModifyMenuW(static_cast<HMENU>(m_parentMenu), m_index,
			MF_BYPOSITION | (MF_CHECKED * m_checked) | (MF_DISABLED * m_enabled),
			0,
			m_name.GetWideString().c_str()
		);
	}

	SubMenuImpl::SubMenuImpl(String name, Menu & menu, MenuHandle parentMenu, uint32_t index) :
	m_name(std::move(name)),
	m_menu(menu),
	m_parentMenu(parentMenu),
	m_index(index)
	{
		m_handle = ::CreateMenu();

		MENUINFO menuInfo = {};
		menuInfo.cbSize = sizeof(menuInfo);
		menuInfo.fMask = MIM_STYLE;

		GetMenuInfo(m_handle, &menuInfo);
		menuInfo.fMask = MIM_STYLE | MIM_MENUDATA;
		menuInfo.dwStyle |= MNS_NOTIFYBYPOS;
		menuInfo.dwMenuData = (ULONG_PTR)this;
		SetMenuInfo(m_handle, &menuInfo);
	}

	MenuButton& SubMenuImpl::AppendButton(String name)
	{
		//m_menu.SetDirty();
		::AppendMenuW(m_handle, 0, 0, name.GetWideString().c_str());
		auto menuButton = std::make_unique<MenuButton>(std::move(name), m_menu, m_handle, static_cast<uint32_t>(m_items.size()));
		m_items.push_back(std::move(menuButton));
		return *reinterpret_cast<MenuButton*>(m_items.back().get());
	}

	void SubMenuImpl::AppendSeparator()
	{
		//m_menu.SetDirty();
		::AppendMenuW(m_handle, MF_SEPARATOR, 0, nullptr);
		m_items.push_back(nullptr);
	}

	SubMenu& SubMenuImpl::AppendSubMenu(String name)
	{
		//m_menu.SetDirty();
		auto subMenu = std::make_unique<SubMenu>(std::move(name), m_menu, m_handle, static_cast<uint32_t>(m_items.size()));
		::AppendMenuW(m_handle, MF_POPUP, reinterpret_cast<UINT_PTR>(subMenu->GetHandle()), subMenu->GetName().GetWideString().c_str());
		m_items.push_back(std::move(subMenu));
		return *reinterpret_cast<SubMenu*>(m_items.back().get());
	}

	MenuHandle SubMenuImpl::GetHandle() const
	{
		return m_handle;
	}

	const String& SubMenuImpl::GetName() const
	{
		return m_name;
	}

	void SubMenuImpl::PressButtonAt(size_t index)
	{
		if (index < m_items.size())
		{
			if (m_items[index] != nullptr)
			{
				if (m_items[index]->GetType() == MenuItemType_Button)
					reinterpret_cast<MenuButton*>(m_items[index].get())->OnPressed(*reinterpret_cast<MenuButton*>(m_items[index].get()));
			}
		}
	}
}
