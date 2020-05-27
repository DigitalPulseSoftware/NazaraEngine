// Copyright (C) 2015 Jérôme Leclercq
// This file is part of the "Nazara Engine - Platform module"
// For conditions of distribution and use, see copyright notice in Config.hpp

// Un grand merci à Laurent Gomila pour la SFML qui m'aura bien aidé à réaliser cette implémentation

#include <Nazara/Platform/X11/WindowImpl.hpp>
#include <Nazara/Core/CallOnExit.hpp>
#include <Nazara/Core/Error.hpp>
#include <Nazara/Platform/Cursor.hpp>
#include <Nazara/Platform/Event.hpp>
#include <Nazara/Platform/Icon.hpp>
#include <Nazara/Platform/VideoMode.hpp>
#include <Nazara/Platform/Window.hpp>
#include <Nazara/Platform/X11/CursorImpl.hpp>
#include <Nazara/Platform/X11/Display.hpp>
#include <Nazara/Platform/X11/IconImpl.hpp>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/Xutil.h>
#include <xcb/xcb_keysyms.h>
#include <Nazara/Platform/Debug.hpp>

/*
	Things to do left:

    Icon working sometimes (No idea)
    EnableKeyRepeat (Working but is it the right behaviour ?)
    Fullscreen (No alt + tab)
    Smooth scroll (No equivalent for X11)
    Threaded window (Not tested a lot)
    Event listener (Not tested)
    Cleanup
    IsVisible (Not working as expected)
    SetStayOnTop (Equivalent for X11 ?)
    Opengl Context (glXCreateContextAttribs should be loaded like in window and the version for the context should be the one of NzContextParameters)

*/

namespace Nz
{
	namespace
	{
		Nz::WindowImpl* fullscreenWindow = nullptr;

		const uint32_t eventMask = XCB_EVENT_MASK_FOCUS_CHANGE   | XCB_EVENT_MASK_BUTTON_PRESS     |
								   XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION    |
								   XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_KEY_PRESS        |
								   XCB_EVENT_MASK_KEY_RELEASE    | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
								   XCB_EVENT_MASK_ENTER_WINDOW   | XCB_EVENT_MASK_LEAVE_WINDOW;

		xcb_connection_t* connection = nullptr;
	}

	WindowImpl::WindowImpl(Window* parent) :
	m_window(0),
	m_style(0),
	m_parent(parent),
	m_smoothScrolling(false),
	m_mousePos(0, 0),
	m_keyRepeat(true)
	{
		std::memset(&m_size_hints, 0, sizeof(m_size_hints));
	}

	WindowImpl::~WindowImpl()
	{
		// Cleanup graphical resources
		CleanUp();

		// We clean up the event queue
		UpdateEventQueue(nullptr);
		UpdateEventQueue(nullptr);
	}

	bool WindowImpl::Create(const VideoMode& mode, const String& title, WindowStyleFlags style)
	{
		bool fullscreen = (style & Nz::WindowStyle_Fullscreen) != 0;
		m_eventListener = true;
		m_ownsWindow = true;
		m_style = style;

		std::memset(&m_oldVideoMode, 0, sizeof(m_oldVideoMode));

		m_screen = X11::XCBDefaultScreen(connection);

		// Compute position and size
		int left = fullscreen ? 0 : (m_screen->width_in_pixels  - mode.width) / 2;
		int top = fullscreen ? 0 : (m_screen->height_in_pixels - mode.height) / 2;
		int width  = mode.width;
		int height = mode.height;

		// Define the window attributes
		xcb_colormap_t colormap = xcb_generate_id(connection);
		xcb_create_colormap(connection, XCB_COLORMAP_ALLOC_NONE, colormap, m_screen->root, m_screen->root_visual);
		const uint32_t value_list[] = { fullscreen, eventMask, colormap };

		CallOnExit onExit([&](){
			if (!X11::CheckCookie(
				connection,
				xcb_free_colormap(
					connection,
					colormap
				))
			)
				NazaraError("Failed to free colormap");
		});

		// Create the window
		m_window = xcb_generate_id(connection);

		if (!X11::CheckCookie(
			connection,
			xcb_create_window_checked(
				connection,
				XCB_COPY_FROM_PARENT,
				m_window,
				m_screen->root,
				left, top,
				width, height,
				0,
				XCB_WINDOW_CLASS_INPUT_OUTPUT,
				m_screen->root_visual,
				XCB_CW_EVENT_MASK | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_COLORMAP,
				value_list
			)))
		{
			NazaraError("Failed to create window");
			return false;
		}

		// Flush the commands queue
		xcb_flush(connection);

		// We get default normal hints for the new window
		ScopedXCB<xcb_generic_error_t> error(nullptr);
		xcb_icccm_get_wm_normal_hints_reply(
			connection,
			xcb_icccm_get_wm_normal_hints(
				connection,
				m_window),
			&m_size_hints,
			&error
		);
		if (error)
			NazaraError("Failed to get size hints");

		// And we modify the size and the position.
		xcb_icccm_size_hints_set_position(&m_size_hints, false, left, top);
		xcb_icccm_size_hints_set_size(&m_size_hints, false, width, height);
		if (!UpdateNormalHints())
			NazaraError("Failed to set window configuration");

		// Do some common initializations
		CommonInitialize();

		if (!(m_style & Nz::WindowStyle_Fullscreen))
			SetMotifHints();

		// Flush the commands queue
		xcb_flush(connection);

		// Set the window's name
		SetTitle(title);

		if (m_style & WindowStyle_Threaded)
		{
			Mutex mutex;
			ConditionVariable condition;
			m_threadActive = true;

			// Wait until the thread is ready
			mutex.Lock();
			m_thread = Thread(WindowThread, this, &mutex, &condition);
			condition.Wait(&mutex);
			mutex.Unlock();
		}

		// Set fullscreen video mode and switch to fullscreen if necessary
		if (fullscreen)
		{
			SetPosition(0, 0);
			SetVideoMode(mode);
			SwitchToFullscreen();
		}

		return true;
	}

	bool WindowImpl::Create(WindowHandle handle)
	{
		std::memset(&m_oldVideoMode, 0, sizeof(m_oldVideoMode));

		m_screen = X11::XCBDefaultScreen(connection);

		if (!handle)
		{
			NazaraError("Invalid handle");
			return false;
		}

		// Save the window handle
		m_window = handle;

		m_ownsWindow = false;
		m_eventListener = false;

		ScopedXCB<xcb_generic_error_t> error(nullptr);

		// We try to get informations from the shared window.
		xcb_icccm_get_wm_normal_hints_reply(
			connection,
			xcb_icccm_get_wm_normal_hints(
				connection,
				m_window),
			&m_size_hints,
			&error
		);

		if (error)
		{
			NazaraError("Failed to obtain sizes and positions");
			return false;
		}

		// Do some common initializations
		CommonInitialize();

		// Flush the commands queue
		xcb_flush(connection);

		return true;
	}

	void WindowImpl::Destroy()
	{
		if (m_ownsWindow)
		{
			if (m_style & WindowStyle_Threaded)
			{
				if (m_thread.IsJoinable())
				{
					m_threadActive = false;
					m_thread.Join();
				}
			}

			// Destroy the window
			if (m_window && m_ownsWindow)
			{
				// Unhide the mouse cursor (in case it was hidden)
				SetCursor(*Cursor::Get(SystemCursor_Default));

				if (!X11::CheckCookie(
					connection,
					xcb_destroy_window(
						connection,
						m_window
					)))
					NazaraError("Failed to destroy window");

				xcb_flush(connection);
			}
		}
		else
			SetEventListener(false);
	}

	void WindowImpl::EnableKeyRepeat(bool enable)
	{
		m_keyRepeat = enable;
	}

	void WindowImpl::EnableSmoothScrolling(bool enable)
	{
		m_smoothScrolling = enable;
	}

	void WindowImpl::ForceDrawMenu()
	{
	}

	WindowHandle WindowImpl::GetHandle() const
	{
		return m_window;
	}

	unsigned int WindowImpl::GetHeight() const
	{
		return m_size_hints.height;
	}

	Vector2i WindowImpl::GetPosition() const
	{
		return { m_size_hints.x, m_size_hints.y };
	}

	Vector2ui WindowImpl::GetSize() const
	{
		return Vector2ui(m_size_hints.width, m_size_hints.height);
	}

	WindowStyleFlags WindowImpl::GetStyle() const
	{
		return m_style;
	}

	String WindowImpl::GetTitle() const
	{
		ScopedXCBEWMHConnection ewmhConnection(connection);

		ScopedXCB<xcb_generic_error_t> error(nullptr);

		xcb_ewmh_get_utf8_strings_reply_t data;
		xcb_ewmh_get_wm_name_reply(ewmhConnection,
			xcb_ewmh_get_wm_name(ewmhConnection, m_window), &data, &error);

		if (error)
			NazaraError("Failed to get window's title");

		String tmp(data.strings, data.strings_len);

		xcb_ewmh_get_utf8_strings_reply_wipe(&data);

		return tmp;
	}

	unsigned int WindowImpl::GetWidth() const
	{
		return m_size_hints.width;
	}

	bool WindowImpl::HasFocus() const
	{
		ScopedXCB<xcb_generic_error_t> error(nullptr);

		ScopedXCB<xcb_get_input_focus_reply_t> reply(xcb_get_input_focus_reply(
			connection,
			xcb_get_input_focus_unchecked(
				connection
			),
			&error
		));

		if (error)
			NazaraError("Failed to check if window has focus");

		return (reply->focus == m_window);
	}

	void WindowImpl::IgnoreNextMouseEvent(int mouseX, int mouseY)
	{
		// Petite astuce ...
		m_mousePos.x = mouseX;
		m_mousePos.y = mouseY;
	}

	bool WindowImpl::IsMinimized() const
	{
		ScopedXCBEWMHConnection ewmhConnection(connection);

		ScopedXCB<xcb_generic_error_t> error(nullptr);
		bool isMinimized = false;

		xcb_ewmh_get_atoms_reply_t atomReply;
		if (xcb_ewmh_get_wm_state_reply(ewmhConnection,
				xcb_ewmh_get_wm_state(ewmhConnection, m_window), &atomReply, &error) == 1)
		{
			for (unsigned int i = 0; i < atomReply.atoms_len; i++)
				if (atomReply.atoms[i] == ewmhConnection->_NET_WM_STATE_HIDDEN)
					isMinimized = true;

			xcb_ewmh_get_atoms_reply_wipe(&atomReply);
		}

		if (error)
			NazaraError("Failed to determine if window is minimized");

		return isMinimized;
	}

	bool WindowImpl::IsVisible() const
	{
		return !IsMinimized(); // Visibility event ?
	}

	void WindowImpl::ProcessEvents(bool block)
	{
		if (m_ownsWindow)
		{
			xcb_generic_event_t* event = nullptr;

			if (block)
			{
				event = xcb_wait_for_event(connection);
				if (event)
				{
					UpdateEventQueue(event);
					ProcessEvent(event);
				}
			}
			else
			{
				event = xcb_poll_for_event(connection);
				while (event)
				{
					UpdateEventQueue(event);
					xcb_generic_event_t* tmp = xcb_poll_for_event(connection);
					UpdateEventQueue(tmp);
					ProcessEvent(event);
					if (tmp)
						ProcessEvent(tmp);
					event = xcb_poll_for_event(connection);
				}
			}
		}
	}

	void WindowImpl::RefreshCursor()
	{
	}

	void WindowImpl::SetCursor(const Cursor& cursor)
	{
		xcb_cursor_t cursorImpl = cursor.m_impl->GetCursor();
		if (!X11::CheckCookie(connection, xcb_change_window_attributes(connection, m_window, XCB_CW_CURSOR, &cursorImpl)))
			NazaraError("Failed to change mouse cursor");

		xcb_flush(connection);
	}

	void WindowImpl::SetEventListener(bool listener)
	{
		if (m_ownsWindow)
			m_eventListener = listener;
		else if (listener != m_eventListener)
		{
			if (listener)
			{
				const uint32_t value_list[] = { eventMask };

				if (!X11::CheckCookie(
					connection,
					xcb_change_window_attributes(
						connection,
						m_window,
						XCB_CW_EVENT_MASK,
						value_list
					))
				)
					NazaraError("Failed to change event for listener");

				m_eventListener = true;
			}
			else if (m_eventListener)
			{
				const uint32_t value_list[] = { XCB_EVENT_MASK_NO_EVENT };

				if (!X11::CheckCookie(
					connection,
					xcb_change_window_attributes(
						connection,
						m_window,
						XCB_CW_EVENT_MASK,
						value_list
					))
				)
					NazaraError("Failed to change event for listener");

				m_eventListener = false;
			}
		}
	}

	void WindowImpl::SetFocus()
	{
		if (!X11::CheckCookie(
			connection,
			xcb_set_input_focus(
				connection,
				XCB_INPUT_FOCUS_POINTER_ROOT,
				m_window,
				XCB_CURRENT_TIME
			))
		)
			NazaraError("Failed to set input focus");

		const uint32_t values[] = { XCB_STACK_MODE_ABOVE };

		if (!X11::CheckCookie(
			connection,
			xcb_configure_window(
				connection,
				m_window,
				XCB_CONFIG_WINDOW_STACK_MODE,
				values
			))
		)
			NazaraError("Failed to set focus");
	}

	void WindowImpl::SetIcon(const Icon& icon)
	{
		if (!icon.IsValid())
		{
			NazaraError("Icon is not valid");
			return;
		}

		xcb_pixmap_t icon_pixmap = icon.m_impl->GetIcon();
		xcb_pixmap_t mask_pixmap = icon.m_impl->GetMask();

		ScopedXCB<xcb_generic_error_t> error(nullptr);

		xcb_icccm_wm_hints_t hints;
		std::memset(&hints, 0, sizeof(hints));

		xcb_icccm_get_wm_hints_reply(
			connection,
			xcb_icccm_get_wm_hints(
				connection,
				m_window),
			&hints,
			&error
		);

		if (error)
			NazaraError("Failed to get wm hints");

		xcb_icccm_wm_hints_set_icon_pixmap(&hints, icon_pixmap);
		xcb_icccm_wm_hints_set_icon_mask(&hints, mask_pixmap);

		if (!X11::CheckCookie(
			connection,
			xcb_icccm_set_wm_hints(
				connection,
				m_window,
				&hints
			))
		)
			NazaraError("Failed to set wm hints");

		xcb_flush(connection);
	}

	void WindowImpl::SetMaximumSize(int width, int height)
	{
		if (width < 0)
			width = m_screen->width_in_pixels;
		if (height < 0)
			height = m_screen->height_in_pixels;

		xcb_icccm_size_hints_set_max_size(&m_size_hints, width, height);
		if (!UpdateNormalHints())
			NazaraError("Failed to set maximum size");

		xcb_flush(connection);
	}

	void WindowImpl::SetMenu(Menu& menu)
	{
	}

	void WindowImpl::SetMinimumSize(int width, int height)
	{
		xcb_icccm_size_hints_set_min_size(&m_size_hints, width, height);
		if (!UpdateNormalHints())
			NazaraError("Failed to set minimum size");

		xcb_flush(connection);
	}

	void WindowImpl::SetPosition(int x, int y)
	{
		xcb_icccm_size_hints_set_position(&m_size_hints, true, x, y);
		if (!UpdateNormalHints())
			NazaraError("Failed to set size hints position");

		const uint32_t values[] = { static_cast<uint32_t>(x), static_cast<uint32_t>(y) };
		if (!X11::CheckCookie(
			connection,
			xcb_configure_window(
				connection,
				m_window,
				XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
				values
			))
		)
			NazaraError("Failed to set position");

		xcb_flush(connection);
	}

	void WindowImpl::SetSize(unsigned int width, unsigned int height)
	{
		xcb_icccm_size_hints_set_size(&m_size_hints, true, width, height);
		if (!UpdateNormalHints())
			NazaraError("Failed to set size hints sizes");

		const uint32_t values[] = { width, height };
		if (!X11::CheckCookie(
			connection,
			xcb_configure_window(
				connection,
				m_window,
				XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
				values
			))
		)
			NazaraError("Failed to set sizes");

		xcb_flush(connection);
	}

	void WindowImpl::SetStayOnTop(bool stayOnTop)
	{
		ScopedXCBEWMHConnection ewmhConnection(connection);

		xcb_atom_t onTop; // It is not really working
		if (stayOnTop)
			onTop = ewmhConnection->_NET_WM_STATE_ABOVE;
		else
			onTop = ewmhConnection->_NET_WM_STATE_BELOW;

		if (!X11::CheckCookie(
			connection,
			xcb_ewmh_set_wm_state(
				ewmhConnection,
				m_window,
				1,
				&onTop
			))
		)
			NazaraError("Failed to set stay on top");

		xcb_flush(connection);
	}

	void WindowImpl::SetTitle(const String& title)
	{
		ScopedXCBEWMHConnection ewmhConnection(connection);

		if (!X11::CheckCookie(
			connection,
			xcb_ewmh_set_wm_name(
				ewmhConnection,
				m_window,
				title.GetSize(),
				title.GetConstBuffer()
			))
		)
			NazaraError("Failed to set title");

		xcb_flush(connection);
	}

	void WindowImpl::SetVisible(bool visible)
	{
		if (visible)
		{
			if (!X11::CheckCookie(
				connection,
				xcb_map_window(
					connection,
					m_window
				))
			)
				NazaraError("Failed to change window visibility to visible");
		}
		else
		{
			if (!X11::CheckCookie(
				connection,
				xcb_unmap_window(
					connection,
					m_window
				))
			)
				NazaraError("Failed to change window visibility to invisible");
		}

		xcb_flush(connection);
	}

	bool WindowImpl::Initialize()
	{
		X11::Initialize();

		connection = X11::OpenConnection();

		return true;
	}

	void WindowImpl::Uninitialize()
	{
		X11::CloseConnection(connection);

		X11::Uninitialize();
	}

	void WindowImpl::CleanUp()
	{
		// Restore the previous video mode (in case we were running in fullscreen)
		ResetVideoMode();
	}

	xcb_keysym_t WindowImpl::ConvertKeyCodeToKeySym(xcb_keycode_t keycode, uint16_t state)
	{
		xcb_key_symbols_t* keysyms = X11::XCBKeySymbolsAlloc(connection);
		if (!keysyms)
		{
			NazaraError("Failed to get key symbols");
			return XCB_NO_SYMBOL;
		}

		xcb_keysym_t k0, k1;

		CallOnExit onExit([&](){
			X11::XCBKeySymbolsFree(keysyms);
		});

		// Based on documentation in https://cgit.freedesktop.org/xcb/util-keysyms/tree/keysyms/keysyms.c
		// Mode switch = ctlr and alt gr = XCB_MOD_MASK_5

		// The first four elements of the list are split into two groups of KeySyms.
		if (state & XCB_MOD_MASK_1)
		{
			k0 = xcb_key_symbols_get_keysym(keysyms, keycode, 2);
			k1 = xcb_key_symbols_get_keysym(keysyms, keycode, 3);
		}
		if (state & XCB_MOD_MASK_5)
		{
			k0 = xcb_key_symbols_get_keysym(keysyms, keycode, 4);
			k1 = xcb_key_symbols_get_keysym(keysyms, keycode, 5);
		}
		else
		{
			k0 = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
			k1 = xcb_key_symbols_get_keysym(keysyms, keycode, 1);
		}

		// If the second element of the group is NoSymbol, then the group should be treated as if the second element were the same as the first element.
		if (k1 == XCB_NO_SYMBOL)
			k1 = k0;

		/* The numlock modifier is on and the second KeySym is a keypad KeySym
		The numlock modifier is on and the second KeySym is a keypad KeySym. In
		this case, if the Shift modifier is on, or if the Lock modifier is on
		and is interpreted as ShiftLock, then the first KeySym is used,
		otherwise the second KeySym is used.
		*/
		if ((state & XCB_MOD_MASK_2) && xcb_is_keypad_key(k1))
		{
			if ((state & XCB_MOD_MASK_SHIFT) ||	(state & XCB_MOD_MASK_LOCK && (state & XCB_MOD_MASK_SHIFT)))
				return k0;
			else
				return k1;
		}

		/* The Shift and Lock modifiers are both off. In this case, the first
		KeySym is used.*/
		else if (!(state & XCB_MOD_MASK_SHIFT) && !(state & XCB_MOD_MASK_LOCK))
			return k0;

		/* The Shift modifier is off, and the Lock modifier is on and is
		interpreted as CapsLock. In this case, the first KeySym is used, but
		if that KeySym is lowercase alphabetic, then the corresponding
		uppercase KeySym is used instead. */
		else if (!(state & XCB_MOD_MASK_SHIFT) && (state & XCB_MOD_MASK_LOCK && (state & XCB_MOD_MASK_SHIFT)))
			return k0;

		/* The Shift modifier is on, and the Lock modifier is on and is
		interpreted as CapsLock. In this case, the second KeySym is used, but
		if that KeySym is lowercase alphabetic, then the corresponding
		uppercase KeySym is used instead.*/
		else if ((state & XCB_MOD_MASK_SHIFT) && (state & XCB_MOD_MASK_LOCK && (state & XCB_MOD_MASK_SHIFT)))
			return k1;

		/* The Shift modifier is on, or the Lock modifier is on and is
		interpreted as ShiftLock, or both. In this case, the second KeySym is
		used. */
		else if ((state & XCB_MOD_MASK_SHIFT) || (state & XCB_MOD_MASK_LOCK && (state & XCB_MOD_MASK_SHIFT)))
			return k1;

		return XCB_NO_SYMBOL;
	}

	Keyboard::Key WindowImpl::ConvertVirtualKey(xcb_keysym_t symbol)
	{
		// First convert to uppercase (to avoid dealing with two different keysyms for the same key)
		KeySym lower, key;
		XConvertCase(symbol, &lower, &key);

		switch (key)
		{
			// Lettres
			case XK_A: return Keyboard::A;
			case XK_B: return Keyboard::B;
			case XK_C: return Keyboard::C;
			case XK_D: return Keyboard::D;
			case XK_E: return Keyboard::E;
			case XK_F: return Keyboard::F;
			case XK_G: return Keyboard::G;
			case XK_H: return Keyboard::H;
			case XK_I: return Keyboard::I;
			case XK_J: return Keyboard::J;
			case XK_K: return Keyboard::K;
			case XK_L: return Keyboard::L;
			case XK_M: return Keyboard::M;
			case XK_N: return Keyboard::N;
			case XK_O: return Keyboard::O;
			case XK_P: return Keyboard::P;
			case XK_Q: return Keyboard::Q;
			case XK_R: return Keyboard::R;
			case XK_S: return Keyboard::S;
			case XK_T: return Keyboard::T;
			case XK_U: return Keyboard::U;
			case XK_V: return Keyboard::V;
			case XK_W: return Keyboard::W;
			case XK_X: return Keyboard::X;
			case XK_Y: return Keyboard::Y;
			case XK_Z: return Keyboard::Z;

			// Touches de fonction
			case XK_F1: return Keyboard::F1;
			case XK_F2: return Keyboard::F2;
			case XK_F3: return Keyboard::F3;
			case XK_F4: return Keyboard::F4;
			case XK_F5: return Keyboard::F5;
			case XK_F6: return Keyboard::F6;
			case XK_F7: return Keyboard::F7;
			case XK_F8: return Keyboard::F8;
			case XK_F9: return Keyboard::F9;
			case XK_F10: return Keyboard::F10;
			case XK_F11: return Keyboard::F11;
			case XK_F12: return Keyboard::F12;
			case XK_F13: return Keyboard::F13;
			case XK_F14: return Keyboard::F14;
			case XK_F15: return Keyboard::F15;

			// Flèches directionnelles
			case XK_Down: return Keyboard::Down;
			case XK_Left: return Keyboard::Left;
			case XK_Right: return Keyboard::Right;
			case XK_Up: return Keyboard::Up;

			// Pavé numérique
			case XK_KP_Add: return Keyboard::Add;
			case XK_KP_Decimal: return Keyboard::Decimal;
			case XK_KP_Delete: return Keyboard::Decimal;
			case XK_KP_Divide: return Keyboard::Divide;
			case XK_KP_Multiply: return Keyboard::Multiply;
			case XK_KP_Insert: return Keyboard::Numpad0;
			case XK_KP_End: return Keyboard::Numpad1;
			case XK_KP_Down: return Keyboard::Numpad2;
			case XK_KP_Page_Down: return Keyboard::Numpad3;
			case XK_KP_Left: return Keyboard::Numpad4;
			case XK_KP_Begin: return Keyboard::Numpad5;
			case XK_KP_Right: return Keyboard::Numpad6;
			case XK_KP_Home: return Keyboard::Numpad7;
			case XK_KP_Up: return Keyboard::Numpad8;
			case XK_KP_Page_Up: return Keyboard::Numpad9;
			case XK_KP_Enter: return Keyboard::Return;
			case XK_KP_Subtract: return Keyboard::Subtract;

			// Divers
			case XK_backslash: return Keyboard::Backslash;
			case XK_BackSpace: return Keyboard::Backspace;
			case XK_Clear: return Keyboard::Clear;
			case XK_comma: return Keyboard::Comma;
			case XK_minus: return Keyboard::Dash;
			case XK_Delete: return Keyboard::Delete;
			case XK_End: return Keyboard::End;
			case XK_equal: return Keyboard::Equal;
			case XK_Escape: return Keyboard::Escape;
			case XK_Home: return Keyboard::Home;
			case XK_Insert: return Keyboard::Insert;
			case XK_Alt_L: return Keyboard::LAlt;
			case XK_bracketleft: return Keyboard::LBracket;
			case XK_Control_L: return Keyboard::LControl;
			case XK_Shift_L: return Keyboard::LShift;
			case XK_Super_L: return Keyboard::LSystem;
			case XK_0: return Keyboard::Num0;
			case XK_1: return Keyboard::Num1;
			case XK_2: return Keyboard::Num2;
			case XK_3: return Keyboard::Num3;
			case XK_4: return Keyboard::Num4;
			case XK_5: return Keyboard::Num5;
			case XK_6: return Keyboard::Num6;
			case XK_7: return Keyboard::Num7;
			case XK_8: return Keyboard::Num8;
			case XK_9: return Keyboard::Num9;
			case XK_Page_Down: return Keyboard::PageDown;
			case XK_Page_Up: return Keyboard::PageUp;
			case XK_Pause: return Keyboard::Pause;
			case XK_period: return Keyboard::Period;
			case XK_Print: return Keyboard::Print;
			case XK_Sys_Req: return Keyboard::PrintScreen;
			case XK_quotedbl: return Keyboard::Quote;
			case XK_Alt_R: return Keyboard::RAlt;
			case XK_bracketright: return Keyboard::RBracket;
			case XK_Control_R: return Keyboard::RControl;
			case XK_Return: return Keyboard::Return;
			case XK_Shift_R: return Keyboard::RShift;
			case XK_Super_R: return Keyboard::RSystem;
			case XK_semicolon: return Keyboard::Semicolon;
			case XK_slash: return Keyboard::Slash;
			case XK_space: return Keyboard::Space;
			case XK_Tab: return Keyboard::Tab;
			case XK_grave: return Keyboard::Tilde;

			// Touches navigateur
			case XF86XK_Back: return Keyboard::Browser_Back;
			case XF86XK_Favorites: return Keyboard::Browser_Favorites;
			case XF86XK_Forward: return Keyboard::Browser_Forward;
			case XF86XK_HomePage: return Keyboard::Browser_Home;
			case XF86XK_Refresh: return Keyboard::Browser_Refresh;
			case XF86XK_Search: return Keyboard::Browser_Search;
			case XF86XK_Stop: return Keyboard::Browser_Stop;

			// Touches de contrôle
			case XF86XK_AudioNext: return Keyboard::Media_Next;
			case XF86XK_AudioPlay: return Keyboard::Media_Play;
			case XF86XK_AudioPrev: return Keyboard::Media_Previous;
			case XF86XK_AudioStop: return Keyboard::Media_Stop;

			// Touches de contrôle du volume
			case XF86XK_AudioLowerVolume: return Keyboard::Volume_Down;
			case XF86XK_AudioMute: return Keyboard::Volume_Mute;
			case XF86XK_AudioRaiseVolume: return Keyboard::Volume_Up;

			// Touches à verrouillage
			case XK_Caps_Lock: return Keyboard::CapsLock;
			case XK_Num_Lock: return Keyboard::NumLock;
			case XK_Scroll_Lock: return Keyboard::ScrollLock;

			default:
				return Keyboard::Undefined;
		}
	}

	void WindowImpl::CommonInitialize()
	{
		// Show the window
		SetVisible(true);

		// Raise the window and grab input focus
		SetFocus();

		xcb_atom_t protocols[] =
		{
			X11::GetAtom("WM_DELETE_WINDOW"),
		};

		if (!X11::CheckCookie(
			connection,
			xcb_icccm_set_wm_protocols(
				connection,
				m_window,
				X11::GetAtom("WM_PROTOCOLS"),
				sizeof(protocols),
				protocols
			))
		)
			NazaraError("Failed to get atom for deleting a window");

		// Flush the commands queue
		xcb_flush(connection);
	}

	char32_t WindowImpl::GetRepresentation(xcb_keysym_t keysym) const
	{
		switch (keysym)
		{
			case XK_KP_Space:
				return ' ';
			case XK_BackSpace:
				return '\b';
			case XK_Tab:
			case XK_KP_Tab:
				return '\t';
			case XK_Linefeed:
				return '\n';
			case XK_Return:
				return '\r';
			// Numpad
			case XK_KP_Multiply:
				return '*';
			case XK_KP_Add:
				return '+';
			case XK_KP_Separator:
				return ','; // In french, it's '.'
			case XK_KP_Subtract:
				return '-';
			case XK_KP_Decimal:
				return '.'; // In french, it's ','
			case XK_KP_Divide:
				return '/';
			case XK_KP_0:
				return '0';
			case XK_KP_1:
				return '1';
			case XK_KP_2:
				return '2';
			case XK_KP_3:
				return '3';
			case XK_KP_4:
				return '4';
			case XK_KP_5:
				return '5';
			case XK_KP_6:
				return '6';
			case XK_KP_7:
				return '7';
			case XK_KP_8:
				return '8';
			case XK_KP_9:
				return '9';
			case XK_KP_Enter:
				return '\r';
			default:
				if (xcb_is_modifier_key(keysym) == true)
					return '\0';
				else
					return keysym;
		}
	}

	void WindowImpl::ProcessEvent(xcb_generic_event_t* windowEvent)
	{
		if (!m_eventListener)
			return;

		// Convert the xcb event to a Event
		switch (windowEvent->response_type & ~0x80)
		{
			// Destroy event
			case XCB_DESTROY_NOTIFY:
			{
				// The window is about to be destroyed: we must cleanup resources
				CleanUp();
				break;
			}

			// Gain focus event
			case XCB_FOCUS_IN:
			{
				const uint32_t value_list[] = { eventMask };
				if (!X11::CheckCookie(
					connection,
					xcb_change_window_attributes(
						connection,
						m_window,
						XCB_CW_EVENT_MASK,
						value_list
					))
				)
					NazaraError("Failed to change event mask");

				WindowEvent event;
				event.type = Nz::WindowEventType_GainedFocus;
				m_parent->PushEvent(event);

				break;
			}

			// Lost focus event
			case XCB_FOCUS_OUT:
			{
				WindowEvent event;
				event.type = Nz::WindowEventType_LostFocus;
				m_parent->PushEvent(event);

				const uint32_t values[] = { XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_FOCUS_CHANGE };
				if (!X11::CheckCookie(
					connection,
					xcb_change_window_attributes(
						connection,
						m_window,
						XCB_CW_EVENT_MASK,
						values
					))
				)
					NazaraError("Failed to change event mask");
				
				break;
			}

			// Resize event
			case XCB_CONFIGURE_NOTIFY:
			{
				xcb_configure_notify_event_t* configureNotifyEvent = (xcb_configure_notify_event_t*)windowEvent;
				// ConfigureNotify can be triggered for other reasons, check if the size has actually changed
				if ((configureNotifyEvent->width != m_size_hints.width) || (configureNotifyEvent->height != m_size_hints.width))
				{
					WindowEvent event;
					event.type        = Nz::WindowEventType_Resized;
					event.size.width  = configureNotifyEvent->width;
					event.size.height = configureNotifyEvent->height;
					m_parent->PushEvent(event);

					m_size_hints.width = configureNotifyEvent->width;
					m_size_hints.height = configureNotifyEvent->height;
				}
				if ((configureNotifyEvent->x != m_size_hints.x) || (configureNotifyEvent->y != m_size_hints.y))
				{
					WindowEvent event;
					event.type        = Nz::WindowEventType_Moved;
					event.size.width  = configureNotifyEvent->x;
					event.size.height = configureNotifyEvent->y;
					m_parent->PushEvent(event);

					m_size_hints.x = configureNotifyEvent->x;
					m_size_hints.y = configureNotifyEvent->y;
				}
				break;
			}

			// Close event
			case XCB_CLIENT_MESSAGE:
			{
				xcb_client_message_event_t* clientMessageEvent = (xcb_client_message_event_t*)windowEvent;

				if (clientMessageEvent->type != X11::GetAtom("WM_PROTOCOLS"))
					break;
				if (clientMessageEvent->data.data32[0] == X11::GetAtom("WM_DELETE_WINDOW"))
				{
					WindowEvent event;
					event.type = Nz::WindowEventType_Quit;
					m_parent->PushEvent(event);
				}

				break;
			}

			// Key down event
			case XCB_KEY_PRESS:
			{
				xcb_key_press_event_t* keyPressEvent = (xcb_key_press_event_t*)windowEvent;

				if (!m_keyRepeat && m_eventQueue.curr && m_eventQueue.next)
				{
					xcb_key_press_event_t* current = (xcb_key_release_event_t*)m_eventQueue.curr;
					// keyPressEvent == next

					if ((current->time == keyPressEvent->time) && (current->detail == keyPressEvent->detail))
						break;
				}

				auto keysym = ConvertKeyCodeToKeySym(keyPressEvent->detail, keyPressEvent->state);

				WindowEvent event;
				event.type        = Nz::WindowEventType_KeyPressed;
				event.key.code    = ConvertVirtualKey(keysym);
				event.key.alt     = keyPressEvent->state & XCB_MOD_MASK_1;
				event.key.control = keyPressEvent->state & XCB_MOD_MASK_CONTROL;
				event.key.shift   = keyPressEvent->state & XCB_MOD_MASK_SHIFT;
				event.key.system  = keyPressEvent->state & XCB_MOD_MASK_4;
				m_parent->PushEvent(event);

				char32_t codePoint = GetRepresentation(keysym);

				// if (std::isprint(codePoint)) Is not working ? + handle combining ?
				{
					event.type           = Nz::WindowEventType_TextEntered;
					event.text.character = codePoint;
					event.text.repeated  = false;
					m_parent->PushEvent(event);
				}

				break;
			}

			// Key up event
			case XCB_KEY_RELEASE:
			{
				xcb_key_release_event_t* keyReleaseEvent = (xcb_key_release_event_t*)windowEvent;

				if (!m_keyRepeat && m_eventQueue.curr && m_eventQueue.next)
				{
					// keyReleaseEvent == current
					xcb_key_press_event_t* next = (xcb_key_press_event_t*)m_eventQueue.next;

					if ((keyReleaseEvent->time == next->time) && (keyReleaseEvent->detail == next->detail))
						break;
				}

				auto keysym = ConvertKeyCodeToKeySym(keyReleaseEvent->detail, keyReleaseEvent->state);

				WindowEvent event;
				event.type        = Nz::WindowEventType_KeyReleased;
				event.key.code    = ConvertVirtualKey(keysym);
				event.key.alt     = keyReleaseEvent->state & XCB_MOD_MASK_1;
				event.key.control = keyReleaseEvent->state & XCB_MOD_MASK_CONTROL;
				event.key.shift   = keyReleaseEvent->state & XCB_MOD_MASK_SHIFT;
				event.key.system  = keyReleaseEvent->state & XCB_MOD_MASK_4;
				m_parent->PushEvent(event);

				break;
			}

			// Mouse button pressed
			case XCB_BUTTON_PRESS:
			{
				xcb_button_press_event_t* buttonPressEvent = (xcb_button_press_event_t*)windowEvent;

				WindowEvent event;
				event.type          = Nz::WindowEventType_MouseButtonPressed;
				event.mouseButton.x = buttonPressEvent->event_x;
				event.mouseButton.y = buttonPressEvent->event_y;

				if (buttonPressEvent->detail == XCB_BUTTON_INDEX_1)
					event.mouseButton.button = Mouse::Left;
				else if (buttonPressEvent->detail == XCB_BUTTON_INDEX_2)
					event.mouseButton.button = Mouse::Middle;
				else if (buttonPressEvent->detail == XCB_BUTTON_INDEX_3)
					event.mouseButton.button = Mouse::Right;
				else if (buttonPressEvent->detail == XCB_BUTTON_INDEX_4)
					event.mouseButton.button = Mouse::XButton1;
				else if (buttonPressEvent->detail == XCB_BUTTON_INDEX_5)
					event.mouseButton.button = Mouse::XButton2;
				else
					NazaraWarning("Mouse button not handled");

				m_parent->PushEvent(event);

				break;
			}

			// Mouse button released
			case XCB_BUTTON_RELEASE:
			{
				xcb_button_release_event_t* buttonReleaseEvent = (xcb_button_release_event_t*)windowEvent;

				WindowEvent event;

				switch (buttonReleaseEvent->detail)
				{
					case XCB_BUTTON_INDEX_4:
					case XCB_BUTTON_INDEX_5:
					{
						event.type             = Nz::WindowEventType_MouseWheelMoved;
						event.mouseWheel.delta = (buttonReleaseEvent->detail == XCB_BUTTON_INDEX_4) ? 1 : -1;
						event.mouseWheel.x = buttonReleaseEvent->event_x;
						event.mouseWheel.y = buttonReleaseEvent->event_y;
						break;
					}
					default:
					{
						event.type          = Nz::WindowEventType_MouseButtonReleased;
						event.mouseButton.x = buttonReleaseEvent->event_x;
						event.mouseButton.y = buttonReleaseEvent->event_y;

						if (buttonReleaseEvent->detail == XCB_BUTTON_INDEX_1)
							event.mouseButton.button = Mouse::Left;
						else if (buttonReleaseEvent->detail == XCB_BUTTON_INDEX_2)
							event.mouseButton.button = Mouse::Middle;
						else if (buttonReleaseEvent->detail == XCB_BUTTON_INDEX_3)
							event.mouseButton.button = Mouse::Right;
						else if (buttonReleaseEvent->detail == XCB_BUTTON_INDEX_4)
							event.mouseButton.button = Mouse::XButton1;
						else if (buttonReleaseEvent->detail == XCB_BUTTON_INDEX_5)
							event.mouseButton.button = Mouse::XButton2;
						else
							NazaraWarning("Mouse button not handled");
					}
				}

				m_parent->PushEvent(event);

				break;
			}

			// Mouse moved
			case XCB_MOTION_NOTIFY:
			{
				xcb_motion_notify_event_t* motionNotifyEvent = (xcb_motion_notify_event_t*)windowEvent;

				// We use the sequence to determine whether the motion is linked to a Mouse::SetPosition
				if (m_mousePos.x == motionNotifyEvent->event_x && m_mousePos.y == motionNotifyEvent->event_y)
					break;

				WindowEvent event;
				event.type        = Nz::WindowEventType_MouseMoved;
				event.mouseMove.deltaX = motionNotifyEvent->event_x - m_mousePos.x;
				event.mouseMove.deltaY = motionNotifyEvent->event_y - m_mousePos.y;
				event.mouseMove.x = motionNotifyEvent->event_x;
				event.mouseMove.y = motionNotifyEvent->event_y;

				m_mousePos.x = motionNotifyEvent->event_x;
				m_mousePos.y = motionNotifyEvent->event_y;

				m_parent->PushEvent(event);

				break;
			}

			// Mouse entered
			case XCB_ENTER_NOTIFY:
			{
				WindowEvent event;
				event.type = Nz::WindowEventType_MouseEntered;
				m_parent->PushEvent(event);

				break;
			}

			// Mouse left
			case XCB_LEAVE_NOTIFY:
			{
				WindowEvent event;
				event.type = Nz::WindowEventType_MouseLeft;
				m_parent->PushEvent(event);

				break;
			}

			// Parent window changed
			case XCB_REPARENT_NOTIFY:
			{
				// Catch reparent events to properly apply fullscreen on
				// some "strange" window managers (like Awesome) which
				// seem to make use of temporary parents during mapping
				if (m_style & WindowStyle_Fullscreen)
					SwitchToFullscreen();

				break;
			}
		}
	}

	void WindowImpl::ResetVideoMode()
	{
		if (fullscreenWindow == this)
		{
			// Get current screen info
			ScopedXCB<xcb_generic_error_t> error(nullptr);

			// Reset the video mode
			ScopedXCB<xcb_randr_set_screen_config_reply_t> setScreenConfig(xcb_randr_set_screen_config_reply(
				connection,
				xcb_randr_set_screen_config(
					connection,
					m_oldVideoMode.root,
					XCB_CURRENT_TIME,
					m_oldVideoMode.config_timestamp,
					m_oldVideoMode.sizeID,
					m_oldVideoMode.rotation,
					m_oldVideoMode.rate
				),
				&error
			));

			if (error)
				NazaraError("Failed to reset old screen configuration");

			// Reset the fullscreen window
			fullscreenWindow = nullptr;
		}
	}

	void WindowImpl::SetMotifHints()
	{
		ScopedXCB<xcb_generic_error_t> error(nullptr);

		const char MOTIF_WM_HINTS[] = "_MOTIF_WM_HINTS";
		ScopedXCB<xcb_intern_atom_reply_t> hintsAtomReply(xcb_intern_atom_reply(
			connection,
			xcb_intern_atom(
				connection,
				0,
				sizeof(MOTIF_WM_HINTS) - 1,
				MOTIF_WM_HINTS
			),
			&error
		));

		if (!error && hintsAtomReply)
		{
			const uint32_t MWM_HINTS_FUNCTIONS   = 1 << 0;
			const uint32_t MWM_HINTS_DECORATIONS = 1 << 1;

			//const uint32_t MWM_DECOR_ALL         = 1 << 0;
			const uint32_t MWM_DECOR_BORDER      = 1 << 1;
			const uint32_t MWM_DECOR_RESIZEH     = 1 << 2;
			const uint32_t MWM_DECOR_TITLE       = 1 << 3;
			const uint32_t MWM_DECOR_MENU        = 1 << 4;
			const uint32_t MWM_DECOR_MINIMIZE    = 1 << 5;
			const uint32_t MWM_DECOR_MAXIMIZE    = 1 << 6;

			//const uint32_t MWM_FUNC_ALL          = 1 << 0;
			const uint32_t MWM_FUNC_RESIZE       = 1 << 1;
			const uint32_t MWM_FUNC_MOVE         = 1 << 2;
			const uint32_t MWM_FUNC_MINIMIZE     = 1 << 3;
			const uint32_t MWM_FUNC_MAXIMIZE     = 1 << 4;
			const uint32_t MWM_FUNC_CLOSE        = 1 << 5;

			struct MotifWMHints
			{
				uint32_t flags;
				uint32_t functions;
				uint32_t decorations;
				int32_t  inputMode;
				uint32_t state;
			};

			MotifWMHints hints;
			hints.flags       = MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
			hints.decorations = 0;
			hints.functions   = 0;
			hints.inputMode   = 0;
			hints.state       = 0;

			if (m_style & Nz::WindowStyle_Titlebar)
			{
				hints.decorations |= MWM_DECOR_BORDER | MWM_DECOR_TITLE | MWM_DECOR_MINIMIZE | MWM_DECOR_MENU;
				hints.functions   |= MWM_FUNC_MOVE | MWM_FUNC_MINIMIZE;
			}
			if (m_style & Nz::WindowStyle_Resizable)
			{
				hints.decorations |= MWM_DECOR_MAXIMIZE | MWM_DECOR_RESIZEH;
				hints.functions   |= MWM_FUNC_MAXIMIZE | MWM_FUNC_RESIZE;
			}
			if (m_style & Nz::WindowStyle_Closable)
			{
				hints.decorations |= 0;
				hints.functions   |= MWM_FUNC_CLOSE;
			}

			ScopedXCB<xcb_generic_error_t> propertyError(xcb_request_check(
				connection,
				xcb_change_property_checked(
					connection,
					XCB_PROP_MODE_REPLACE,
					m_window,
					hintsAtomReply->atom,
					hintsAtomReply->atom,
					32,
					5,
					&hints
				)
			));

			if (propertyError)
				NazaraError("xcb_change_property failed, could not set window hints");
		}
		else
			NazaraError("Failed to request _MOTIF_WM_HINTS atom.");
	}

	void WindowImpl::SetVideoMode(const VideoMode& mode)
	{
		// Skip mode switching if the new mode is equal to the desktop mode
		if (mode == VideoMode::GetDesktopMode())
			return;

		ScopedXCB<xcb_generic_error_t> error(nullptr);

		// Check if the RandR extension is present
		const xcb_query_extension_reply_t* randrExt = xcb_get_extension_data(connection, &xcb_randr_id);

		if (!randrExt || !randrExt->present)
		{
			// RandR extension is not supported: we cannot use fullscreen mode
			NazaraError("Fullscreen is not supported, switching to window mode");
			return;
		}

		// Load RandR and check its version
		ScopedXCB<xcb_randr_query_version_reply_t> randrVersion(xcb_randr_query_version_reply(
			connection,
			xcb_randr_query_version(
				connection,
				1,
				1
			),
			&error
		));

		if (error)
		{
			NazaraError("Failed to load RandR, switching to window mode");
			return;
		}

		// Get the current configuration
		ScopedXCB<xcb_randr_get_screen_info_reply_t> config(xcb_randr_get_screen_info_reply(
			connection,
			xcb_randr_get_screen_info(
				connection,
				m_screen->root
			),
			&error
		));

		if (error || !config)
		{
			// Failed to get the screen configuration
			NazaraError("Failed to get the current screen configuration for fullscreen mode, switching to window mode");
			return;
		}

		// Save the current video mode before we switch to fullscreen
		m_oldVideoMode = *config.get();

		// Get the available screen sizes
		xcb_randr_screen_size_t* sizes = xcb_randr_get_screen_info_sizes(config.get());

		if (!sizes || !config->nSizes)
		{
			NazaraError("Failed to get the fullscreen sizes, switching to window mode");
			return;
		}

		// Search for a matching size
		for (int i = 0; i < config->nSizes; ++i)
		{
			if (config->rotation == XCB_RANDR_ROTATION_ROTATE_90 ||
				config->rotation == XCB_RANDR_ROTATION_ROTATE_270)
				std::swap(sizes[i].height, sizes[i].width);

			if ((sizes[i].width  == static_cast<int>(mode.width)) &&
				(sizes[i].height == static_cast<int>(mode.height)))
			{
				// Switch to fullscreen mode
				ScopedXCB<xcb_randr_set_screen_config_reply_t> setScreenConfig(xcb_randr_set_screen_config_reply(
					connection,
					xcb_randr_set_screen_config(
						connection,
						config->root,
						XCB_CURRENT_TIME,
						config->config_timestamp,
						i,
						config->rotation,
						config->rate
					),
					&error
				));

				if (error)
					NazaraError("Failed to set new screen configuration");

				// Set "this" as the current fullscreen window
				fullscreenWindow = this;
				return;
			}
		}

		NazaraError("Failed to find matching fullscreen size, switching to window mode");
	}

	void WindowImpl::SwitchToFullscreen()
	{
		SetFocus();

		ScopedXCBEWMHConnection ewmhConnection(connection);

		if (!X11::CheckCookie(
			connection,
			xcb_ewmh_set_wm_state(
				ewmhConnection,
				m_window,
				1,
				&ewmhConnection->_NET_WM_STATE_FULLSCREEN
			))
		)
			NazaraError("Failed to switch to fullscreen");
	}

	void WindowImpl::UpdateEventQueue(xcb_generic_event_t* event)
	{
		std::free(m_eventQueue.curr);
		m_eventQueue.curr = m_eventQueue.next;
		m_eventQueue.next = event;
	}

	bool WindowImpl::UpdateNormalHints()
	{
		return X11::CheckCookie(
			connection,
			xcb_icccm_set_wm_normal_hints(
				connection,
				m_window,
				&m_size_hints
			));
	}

	void WindowImpl::WindowThread(WindowImpl* window, Mutex* mutex, ConditionVariable* condition)
	{
		mutex->Lock();
		condition->Signal();
		mutex->Unlock(); // mutex and condition may be destroyed after this line

		if (!window->m_window)
			return;

		while (window->m_threadActive)
			window->ProcessEvents(true);

		window->Destroy();
	}
}
