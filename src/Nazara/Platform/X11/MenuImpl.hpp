// Copyright (C) 2019 Charles Seizilles de Mazancourt
// This file is part of the "Nazara Engine - Platform module"
// For conditions of distribution and use, see copyright notice in Config.hpp

#pragma once

#ifndef NAZARA_MENUIMPL_HPP
#define NAZARA_MENUIMPL_HPP

#include <Nazara/Prerequisites.hpp>
#include <Nazara/Core/String.hpp>
#include <Nazara/Platform/Config.hpp>
#include <Nazara/Platform/Window.hpp>

namespace Nz
{
	class MenuButtonImpl;
	class SubMenuImpl;

	class AbstractButtonedMenuImpl
	{
		public:
			virtual void PressButtonAt(size_t index) = 0;
	};

	class MenuImpl final : public AbstractButtonedMenuImpl
	{
		public:
			MenuImpl(Menu& menu);
			MenuImpl(const MenuImpl&) = delete;
			~MenuImpl();

			MenuButton& AppendButton(String name);
			void AppendSeparator();
			SubMenu& AppendSubMenu(String name);
			MenuHandle GetHandle() const;
			bool IsAssigned() const;
			void PressButtonAt(size_t index) override;
			void SetAssigned(bool assigned = true);

		private:
			Menu& m_menu;
	};

	class MenuButtonImpl
	{
		public:
			MenuButtonImpl(String name, Menu& menu, MenuHandle parentMenu, uint32_t index);
			MenuButtonImpl(const MenuButtonImpl&) = delete;

			const String& GetName() const;
			bool IsChecked() const;
			bool IsEnabled() const;
			void SetChecked(bool checked = true);
			void SetEnabled(bool enabled = true);
			void SetName(String name);
	};

	class SubMenuImpl final : public AbstractButtonedMenuImpl
	{
		public:
			SubMenuImpl(String name, Menu& menu, MenuHandle parentMenu, uint32_t index);
			SubMenuImpl(const SubMenuImpl&) = delete;

			MenuButton& AppendButton(String name);
			void AppendSeparator();
			SubMenu& AppendSubMenu(String name);
			MenuHandle GetHandle() const;
			const String& GetName() const;
			void PressButtonAt(size_t index) override;

		private:
			Menu& m_menu;
	};
}

#endif // NAZARA_MENUIMPL_HPP
