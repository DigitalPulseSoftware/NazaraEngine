// Copyright (C) 2020 Jérôme Leclercq
// This file is part of the "Nazara Engine - Graphics module"
// For conditions of distribution and use, see copyright notice in Config.hpp

// We assume that Debug.hpp has already been included, same thing for Config.hpp
#if NAZARA_GRAPHICS_MANAGE_MEMORY
	#undef delete
	#undef new
#endif
