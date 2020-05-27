// Copyright (C) 2017 Jérôme Leclercq
// This file is part of the "Nazara Engine - Platform module"
// For conditions of distribution and use, see copyright notice in Config.hpp

#pragma once

#ifndef NAZARA_ENUMS_PLATFORM_HPP
#define NAZARA_ENUMS_PLATFORM_HPP

#include <Nazara/Core/Flags.hpp>

namespace Nz
{
	enum SystemCursor
	{
		SystemCursor_Crosshair,
		SystemCursor_Default,
		SystemCursor_Hand,
		SystemCursor_Help,
		SystemCursor_Move,
		SystemCursor_None,
		SystemCursor_Pointer,
		SystemCursor_Progress,
		SystemCursor_ResizeE,
		SystemCursor_ResizeN,
		SystemCursor_ResizeNE,
		SystemCursor_ResizeNW,
		SystemCursor_ResizeS,
		SystemCursor_ResizeSE,
		SystemCursor_ResizeSW,
		SystemCursor_ResizeW,
		SystemCursor_Text,
		SystemCursor_Wait,

		SystemCursor_Max = SystemCursor_Wait
	};

	enum WindowEventType
	{
		WindowEventType_GainedFocus,
		WindowEventType_LostFocus,
		WindowEventType_KeyPressed,
		WindowEventType_KeyReleased,
		WindowEventType_MouseButtonDoubleClicked,
		WindowEventType_MouseButtonPressed,
		WindowEventType_MouseButtonReleased,
		WindowEventType_MouseEntered,
		WindowEventType_MouseLeft,
		WindowEventType_MouseMoved,
		WindowEventType_MouseWheelMoved,
		WindowEventType_Moved,
		WindowEventType_Quit,
		WindowEventType_Resized,
		WindowEventType_TextEntered,

		WindowEventType_Max = WindowEventType_TextEntered
	};

	enum WindowStyle
	{
		WindowStyle_None,       ///< Window has no border nor titlebar.
		WindowStyle_Fullscreen, ///< At the window creation, the OS tries to set it in fullscreen.

		WindowStyle_Closable,   ///< Allows the window to be closed by a button in the titlebar, generating a Quit event.
		WindowStyle_Resizable,  ///< Allows the window to be resized by dragging its corners or by a button of the titlebar.
		WindowStyle_Titlebar,   ///< Adds a titlebar to the window, this option is automatically enabled if buttons of the titlebar are enabled.

		WindowStyle_Threaded,   ///< Runs the window into a thread, allowing the application to keep updating while resizing/dragging the window.

		WindowStyle_Max = WindowStyle_Threaded
	};

	enum MenuItemType
	{
		MenuItemType_Button,
		MenuItemType_SubMenu,

		MenuItemType_Max = MenuItemType_SubMenu
	};

	template<>
	struct EnumAsFlags<WindowStyle>
	{
		static constexpr WindowStyle max = WindowStyle_Max;
	};

	using WindowStyleFlags = Flags<WindowStyle>;

	constexpr WindowStyleFlags WindowStyle_Default = WindowStyle_Closable | WindowStyle_Resizable | WindowStyle_Titlebar;
}

#endif // NAZARA_ENUMS_PLATFORM_HPP
