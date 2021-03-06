// Copyright (C) 2020 Jérôme Leclercq
// This file is part of the "Nazara Engine - Vulkan Renderer"
// For conditions of distribution and use, see copyright notice in Config.hpp

#include <Nazara/Renderer/UploadPool.hpp>
#include <cassert>
#include <Nazara/Renderer/Debug.hpp>

namespace Nz
{
	UploadPool::Allocation::~Allocation() = default;
}
