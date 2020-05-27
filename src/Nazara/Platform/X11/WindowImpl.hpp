// Copyright (C) 2015 Jérôme Leclercq
// This file is part of the "Nazara Engine - Platform module"
// For conditions of distribution and use, see copyright notice in Config.hpp

// Interface inspirée de la SFML par Laurent Gomila

#pragma once

#ifndef NAZARA_WINDOWIMPL_HPP
#define NAZARA_WINDOWIMPL_HPP

#include <Nazara/Prerequisites.hpp>
#include <Nazara/Core/Thread.hpp>
#include <Nazara/Math/Vector2.hpp>
#include <Nazara/Platform/Enums.hpp>
#include <Nazara/Platform/Keyboard.hpp>
#include <Nazara/Platform/Menu.hpp>
#include <Nazara/Platform/WindowHandle.hpp>
#include <xcb/randr.h>
#include <xcb/xcb_icccm.h>

namespace Nz
{
	class ConditionVariable;
	class Mutex;
	class Cursor;
	class Icon;
	class VideoMode;
	class Window;

	class WindowImpl
	{
		public:
			WindowImpl(Window* parent);
			WindowImpl(const WindowImpl&) = delete;
			WindowImpl(WindowImpl&&) = delete; ///TODO?
			~WindowImpl();

			bool Create(const VideoMode& mode, const String& title, WindowStyleFlags style);
			bool Create(WindowHandle handle);

			void Destroy();

			void EnableKeyRepeat(bool enable);
			void EnableSmoothScrolling(bool enable);

			void ForceDrawMenu();

			WindowHandle GetHandle() const;
			unsigned int GetHeight() const;
			Vector2i GetPosition() const;
			Vector2ui GetSize() const;
			WindowStyleFlags GetStyle() const;
			String GetTitle() const;
			unsigned int GetWidth() const;

			bool HasFocus() const;

			void IgnoreNextMouseEvent(int mouseX, int mouseY);

			bool IsMinimized() const;
			bool IsVisible() const;

			void RefreshCursor();

			void ProcessEvents(bool block);

			void SetCursor(const Cursor& cursor);
			void SetEventListener(bool listener);
			void SetFocus();
			void SetIcon(const Icon& icon);
			void SetMaximumSize(int width, int height);
			void SetMenu(Menu& menu);
			void SetMinimumSize(int width, int height);
			void SetPosition(int x, int y);
			void SetSize(unsigned int width, unsigned int height);
			void SetStayOnTop(bool stayOnTop);
			void SetTitle(const String& title);
			void SetVisible(bool visible);

			WindowImpl& operator=(const WindowImpl&) = delete;
			WindowImpl& operator=(WindowImpl&&) = delete; ///TODO?

			static bool Initialize();
			static void Uninitialize();

		private:

			void CleanUp();
			xcb_keysym_t ConvertKeyCodeToKeySym(xcb_keycode_t keycode, uint16_t state);
			Keyboard::Key ConvertVirtualKey(xcb_keysym_t symbol);
			void CommonInitialize();

			char32_t GetRepresentation(xcb_keysym_t keysym) const;

			void ProcessEvent(xcb_generic_event_t* windowEvent);

			void ResetVideoMode();

			void SetMotifHints();
			void SetVideoMode(const VideoMode& mode);
			void SwitchToFullscreen();

			bool UpdateNormalHints();
			void UpdateEventQueue(xcb_generic_event_t* event);

			static void WindowThread(WindowImpl* window, Mutex* mutex, ConditionVariable* condition);

			xcb_window_t                      m_window;
			xcb_screen_t*                     m_screen;
			xcb_randr_get_screen_info_reply_t m_oldVideoMode;
			xcb_size_hints_t m_size_hints;
			Thread m_thread;
			WindowStyleFlags m_style;
			Window* m_parent;
			bool m_eventListener;
			bool m_ownsWindow;
			bool m_smoothScrolling;
			bool m_threadActive;
			Vector2i m_mousePos;
			bool m_keyRepeat;

			struct
			{
				xcb_generic_event_t* curr = nullptr;
				xcb_generic_event_t* next = nullptr;
			} m_eventQueue;
	};
}

#endif // NAZARA_WINDOWIMPL_HPP
