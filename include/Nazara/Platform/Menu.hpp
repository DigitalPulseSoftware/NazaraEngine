// Copyright (C) 2019 Charles Seizilles de Mazancourt
// This file is part of the "Nazara Engine - Platform module"
// For conditions of distribution and use, see copyright notice in Config.hpp

#pragma once

#ifndef NAZARA_MENU_HPP
#define NAZARA_MENU_HPP

#include <Nazara/Prerequisites.hpp>
#include <Nazara/Core/MovablePtr.hpp>
#include <Nazara/Core/Signal.hpp>
#include <Nazara/Core/String.hpp>
#include <Nazara/Platform/Config.hpp>
#include <Nazara/Platform/Enums.hpp>
#include <Nazara/Platform/MenuHandle.hpp>

namespace Nz
{
	class MenuButton;
	class MenuButtonImpl;
	class MenuImpl;
	class SubMenu;
	class SubMenuImpl;

	class NAZARA_PLATFORM_API Menu
	{
		public:
			Menu();
			Menu(const Menu&) = delete;

			MenuButton& AppendButton(String name);
			void AppendSeparator();
			SubMenu& AppendSubMenu(String name);
			MenuHandle GetHandle() const;
			bool IsAssigned() const;
			//bool IsDirty() const;
			void SetAssigned(bool assigned = true);
			//void SetDirty(bool dirty = true);

		private:
			MovablePtr<MenuImpl> m_impl;
	};

	class NAZARA_PLATFORM_API MenuItem
	{
		public:
			MenuItem(MenuItemType type);
			MenuItem(const MenuItem&) = delete;

			MenuItemType GetType() const;
		protected:
			MenuItemType m_type;
	};

	class NAZARA_PLATFORM_API MenuButton final : public MenuItem
	{
		public:
			MenuButton(String name, Menu & menu, MenuHandle parentMenu, uint32_t index);
			MenuButton(const MenuButton&) = delete;

			const String& GetName() const;
			bool IsChecked() const;
			bool IsEnabled() const;
			void SetChecked(bool checked = true);
			void SetEnabled(bool enabled = true);
			void SetName(String name);

			NazaraSignal(OnPressed, MenuButton& /*menuButton*/);

		private:
			MovablePtr<MenuButtonImpl> m_impl;
	};

	class NAZARA_PLATFORM_API SubMenu final : public MenuItem
	{
		public:
			SubMenu(String name, Menu & menu, MenuHandle parentMenu, uint32_t index);
			SubMenu(const SubMenu&) = delete;

			MenuButton& AppendButton(String name);
			void AppendSeparator();
			SubMenu& AppendSubMenu(String name);
			MenuHandle GetHandle() const;
			const String& GetName() const;

		private:
			MovablePtr<SubMenuImpl> m_impl;
	};
}

//#include <Nazara/Platform/Window.inl>

#endif // NAZARA_MENU_HPP
