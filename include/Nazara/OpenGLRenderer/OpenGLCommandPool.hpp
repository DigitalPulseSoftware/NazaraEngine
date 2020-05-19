// Copyright (C) 2020 Jérôme Leclercq
// This file is part of the "Nazara Engine - Renderer module"
// For conditions of distribution and use, see copyright notice in Config.hpp

#pragma once

#ifndef NAZARA_OPENGLRENDERER_OPENGLCOMMANDPOOL_HPP
#define NAZARA_OPENGLRENDERER_OPENGLCOMMANDPOOL_HPP

#include <Nazara/Prerequisites.hpp>
#include <Nazara/Renderer/CommandPool.hpp>
#include <Nazara/OpenGLRenderer/Config.hpp>

namespace Nz
{
	class NAZARA_OPENGLRENDERER_API OpenGLCommandPool final : public CommandPool
	{
		public:
			OpenGLCommandPool() = default;
			OpenGLCommandPool(const OpenGLCommandPool&) = delete;
			OpenGLCommandPool(OpenGLCommandPool&&) noexcept = default;
			~OpenGLCommandPool() = default;

			std::unique_ptr<CommandBuffer> BuildCommandBuffer(const std::function<void(CommandBufferBuilder& builder)>& callback) override;

			OpenGLCommandPool& operator=(const OpenGLCommandPool&) = delete;
			OpenGLCommandPool& operator=(OpenGLCommandPool&&) = delete;
	};
}

#include <Nazara/OpenGLRenderer/OpenGLCommandPool.inl>

#endif // NAZARA_OPENGLRENDERER_OPENGLCOMMANDPOOL_HPP