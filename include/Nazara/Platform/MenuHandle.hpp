// Copyright (C) 2019 Charles Seizilles de Mazancourt
// This file is part of the "Nazara Engine - Platform module"
// For conditions of distribution and use, see copyright notice in Config.hpp

#pragma once

#ifndef NAZARA_MENUHANDLE_HPP
#define NAZARA_MENUHANDLE_HPP

#include <Nazara/Prerequisites.hpp>
#if defined(NAZARA_PLATFORM_X11)
#include <xcb/xcb.h>
#endif

namespace Nz
{
	#if defined(NAZARA_PLATFORM_WINDOWS)
	// http://msdn.microsoft.com/en-us/library/aa383751(v=vs.85).aspx
	using MenuHandle = void*;
	#elif defined(NAZARA_PLATFORM_X11)
	// temporary alias
	using MenuHandle = void*;
	#else
		#error Lack of implementation: MenuHandle
	#endif
}

#endif // NAZARA_MENUHANDLE_HPP
