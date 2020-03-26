#include <Nazara/Utility.hpp>
#include <Nazara/Renderer/Renderer.hpp>
#include <Nazara/Renderer/RenderBuffer.hpp>
#include <Nazara/Renderer/RenderWindow.hpp>
#include <Nazara/VulkanRenderer.hpp>
#include <array>
#include <iostream>

VKAPI_ATTR VkBool32 VKAPI_CALL MyDebugReportCallback(
	VkDebugReportFlagsEXT       flags,
	VkDebugReportObjectTypeEXT  objectType,
	uint64_t                    object,
	size_t                      location,
	int32_t                     messageCode,
	const char*                 pLayerPrefix,
	const char*                 pMessage,
	void*                       pUserData)
{
	std::cerr << pMessage << std::endl;
	return VK_FALSE;
}

int main()
{
	Nz::ParameterList params;
	params.SetParameter("VkInstanceInfo_EnabledExtensionCount", 1LL);
	params.SetParameter("VkInstanceInfo_EnabledExtension0", "VK_EXT_debug_report");

	params.SetParameter("VkDeviceInfo_EnabledLayerCount", 1LL);
	params.SetParameter("VkDeviceInfo_EnabledLayer0", "VK_LAYER_LUNARG_standard_validation");
	params.SetParameter("VkInstanceInfo_EnabledLayerCount", 1LL);
	params.SetParameter("VkInstanceInfo_EnabledLayer0", "VK_LAYER_LUNARG_standard_validation");

	Nz::Renderer::SetParameters(params);

	Nz::Initializer<Nz::Renderer> loader;
	if (!loader)
	{
		std::cout << "Failed to initialize Vulkan" << std::endl;;
		return __LINE__;
	}

	Nz::VulkanRenderer* rendererImpl = static_cast<Nz::VulkanRenderer*>(Nz::Renderer::GetRendererImpl());

	Nz::Vk::Instance& instance = Nz::Vulkan::GetInstance();

	VkDebugReportCallbackCreateInfoEXT callbackCreateInfo = {};
	callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	callbackCreateInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT ;
	callbackCreateInfo.pfnCallback = &MyDebugReportCallback;

	/* Register the callback */
	VkDebugReportCallbackEXT callback;

	instance.vkCreateDebugReportCallbackEXT(instance, &callbackCreateInfo, nullptr, &callback);


	std::vector<VkLayerProperties> layerProperties;
	if (!Nz::Vk::Loader::EnumerateInstanceLayerProperties(&layerProperties))
	{
		NazaraError("Failed to enumerate instance layer properties");
		return __LINE__;
	}

	for (const VkLayerProperties& properties : layerProperties)
	{
		std::cout << properties.layerName << ": \t" << properties.description << std::endl;
	}

	Nz::RenderWindow window;

	Nz::MeshParams meshParams;
	meshParams.matrix = Nz::Matrix4f::Rotate(Nz::EulerAnglesf(0.f, 90.f, 180.f)) * Nz::Matrix4f::Scale(Nz::Vector3f(0.002f));
	meshParams.vertexDeclaration = Nz::VertexDeclaration::Get(Nz::VertexLayout_XYZ_Normal_UV);

	Nz::String windowTitle = "Vulkan Test";
	if (!window.Create(Nz::VideoMode(800, 600, 32), windowTitle))
	{
		std::cout << "Failed to create Window" << std::endl;
		return __LINE__;
	}

	std::shared_ptr<Nz::RenderDevice> device = window.GetRenderDevice();

	auto fragmentShader = device->InstantiateShaderStage(Nz::ShaderStageType::Fragment, Nz::ShaderLanguage::SpirV, "resources/shaders/triangle.frag.spv");
	if (!fragmentShader)
	{
		std::cout << "Failed to instantiate fragment shader" << std::endl;
		return __LINE__;
	}

	auto vertexShader = device->InstantiateShaderStage(Nz::ShaderStageType::Vertex, Nz::ShaderLanguage::SpirV, "resources/shaders/triangle.vert.spv");
	if (!vertexShader)
	{
		std::cout << "Failed to instantiate fragment shader" << std::endl;
		return __LINE__;
	}

	Nz::MeshRef drfreak = Nz::Mesh::LoadFromFile("resources/Spaceship/spaceship.obj", meshParams);

	if (!drfreak)
	{
		NazaraError("Failed to load model");
		return __LINE__;
	}

	Nz::StaticMesh* drfreakMesh = static_cast<Nz::StaticMesh*>(drfreak->GetSubMesh(0));

	const Nz::VertexBuffer* drfreakVB = drfreakMesh->GetVertexBuffer();
	const Nz::IndexBuffer* drfreakIB = drfreakMesh->GetIndexBuffer();

	// Index buffer
	std::cout << "Index count: " << drfreakIB->GetIndexCount() << std::endl;

	// Vertex buffer
	std::cout << "Vertex count: " << drfreakVB->GetVertexCount() << std::endl;

	Nz::VkRenderWindow& vulkanWindow = *static_cast<Nz::VkRenderWindow*>(window.GetImpl());
	Nz::VulkanDevice& vulkanDevice = vulkanWindow.GetDevice();

	Nz::Vk::CommandPool cmdPool;
	if (!cmdPool.Create(vulkanDevice, 0, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT))
	{
		NazaraError("Failed to create rendering cmd pool");
		return __LINE__;
	}

	Nz::Vk::QueueHandle graphicsQueue = vulkanDevice.GetQueue(0, 0);

	// Texture
	Nz::ImageRef drfreakImage = Nz::Image::LoadFromFile("resources/Spaceship/Texture/diffuse.png");
	if (!drfreakImage || !drfreakImage->Convert(Nz::PixelFormatType_RGBA8))
	{
		NazaraError("Failed to load image");
		return __LINE__;
	}

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = static_cast<uint32_t>(drfreakImage->GetWidth());
	imageInfo.extent.height = static_cast<uint32_t>(drfreakImage->GetHeight());
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	Nz::Vk::Image vkImage;
	if (!vkImage.Create(vulkanDevice, imageInfo))
	{
		NazaraError("Failed to create vulkan image");
		return __LINE__;
	}

	VkMemoryRequirements imageMemRequirement = vkImage.GetMemoryRequirements();

	Nz::Vk::DeviceMemory imageMemory;
	if (!imageMemory.Create(vulkanDevice, imageMemRequirement.size, imageMemRequirement.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
	{
		NazaraError("Failed to create vulkan image memory");
		return __LINE__;
	}

	vkImage.BindImageMemory(imageMemory);

	// Update texture
	{
		Nz::Vk::Buffer stagingImageBuffer;
		if (!stagingImageBuffer.Create(vulkanDevice, 0, drfreakImage->GetMemoryUsage(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT))
		{
			NazaraError("Failed to create staging buffer");
			return __LINE__;
		}

		VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

		Nz::Vk::DeviceMemory stagingImageMemory;

		VkMemoryRequirements memRequirement = stagingImageBuffer.GetMemoryRequirements();
		if (!stagingImageMemory.Create(vulkanDevice, memRequirement.size, memRequirement.memoryTypeBits, memoryProperties))
		{
			NazaraError("Failed to allocate vertex buffer memory");
			return __LINE__;
		}

		if (!stagingImageBuffer.BindBufferMemory(stagingImageMemory))
		{
			NazaraError("Failed to bind vertex buffer to its memory");
			return __LINE__;
		}

		if (!stagingImageMemory.Map(0, memRequirement.size))
			return __LINE__;

		std::memcpy(stagingImageMemory.GetMappedPointer(), drfreakImage->GetPixels(), drfreakImage->GetMemoryUsage());

		stagingImageMemory.FlushMemory();
		stagingImageMemory.Unmap();

		Nz::Vk::CommandBuffer copyCommand = cmdPool.AllocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		copyCommand.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		copyCommand.SetImageLayout(vkImage, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copyCommand.CopyBufferToImage(stagingImageBuffer, vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, drfreakImage->GetWidth(), drfreakImage->GetHeight());
		copyCommand.SetImageLayout(vkImage, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		if (!copyCommand.End())
			return __LINE__;

		if (!graphicsQueue.Submit(copyCommand))
			return __LINE__;

		graphicsQueue.WaitIdle();
	}

	// Create image view

	VkImageViewCreateInfo imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.components = {
		VK_COMPONENT_SWIZZLE_R,
		VK_COMPONENT_SWIZZLE_G,
		VK_COMPONENT_SWIZZLE_B,
		VK_COMPONENT_SWIZZLE_A
	};
	imageViewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageViewInfo.image = vkImage;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.subresourceRange = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0,
		1,
		0,
		1
	};

	Nz::Vk::ImageView imageView;
	if (!imageView.Create(vulkanDevice, imageViewInfo))
		return __LINE__;

	// Sampler

	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	Nz::Vk::Sampler imageSampler;
	if (!imageSampler.Create(vulkanDevice, samplerInfo))
		return __LINE__;

	struct
	{
		Nz::Matrix4f projectionMatrix;
		Nz::Matrix4f modelMatrix;
		Nz::Matrix4f viewMatrix;
	}
	ubo;

	Nz::Vector2ui windowSize = window.GetSize();
	ubo.projectionMatrix = Nz::Matrix4f::Perspective(70.f, float(windowSize.x) / windowSize.y, 0.1f, 1000.f);
	ubo.viewMatrix = Nz::Matrix4f::Translate(Nz::Vector3f::Backward() * 1);
	ubo.modelMatrix = Nz::Matrix4f::Translate(Nz::Vector3f::Forward() * 2 + Nz::Vector3f::Right());

	Nz::UInt32 uniformSize = sizeof(ubo);

	Nz::RenderPipelineLayoutInfo pipelineLayoutInfo;
	auto& uboBinding = pipelineLayoutInfo.bindings.emplace_back();
	uboBinding.index = 0;
	uboBinding.shaderStageFlags = Nz::ShaderStageType::Vertex;
	uboBinding.type = Nz::ShaderBindingType::UniformBuffer;

	auto& textureBinding = pipelineLayoutInfo.bindings.emplace_back();
	textureBinding.index = 1;
	textureBinding.shaderStageFlags = Nz::ShaderStageType::Fragment;
	textureBinding.type = Nz::ShaderBindingType::Texture;

	std::shared_ptr<Nz::RenderPipelineLayout> renderPipelineLayout = device->InstantiateRenderPipelineLayout(pipelineLayoutInfo);

	Nz::VulkanRenderPipelineLayout* vkPipelineLayout = static_cast<Nz::VulkanRenderPipelineLayout*>(renderPipelineLayout.get());

	Nz::Vk::DescriptorSet descriptorSet = vkPipelineLayout->AllocateDescriptorSet();

	std::unique_ptr<Nz::AbstractBuffer> uniformBuffer = device->InstantiateBuffer(Nz::BufferType_Uniform);
	if (!uniformBuffer->Initialize(uniformSize, Nz::BufferUsage_DeviceLocal))
	{
		NazaraError("Failed to create uniform buffer");
		return __LINE__;
	}

	Nz::VulkanBuffer* uniformBufferImpl = static_cast<Nz::VulkanBuffer*>(uniformBuffer.get());
	descriptorSet.WriteUniformDescriptor(0, uniformBufferImpl->GetBufferHandle(), 0, uniformSize);
	descriptorSet.WriteCombinedImageSamplerDescriptor(1, imageSampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	Nz::RenderPipelineInfo pipelineInfo;
	pipelineInfo.pipelineLayout = renderPipelineLayout;

	pipelineInfo.depthBuffer = true;
	pipelineInfo.shaderStages.emplace_back(fragmentShader);
	pipelineInfo.shaderStages.emplace_back(vertexShader);

	auto& vertexBuffer = pipelineInfo.vertexBuffers.emplace_back();
	vertexBuffer.binding = 0;
	vertexBuffer.declaration = drfreakVB->GetVertexDeclaration();

	std::unique_ptr<Nz::RenderPipeline> pipeline = device->InstantiateRenderPipeline(pipelineInfo);

	Nz::VulkanRenderPipeline::CreateInfo pipelineCreateInfo = Nz::VulkanRenderPipeline::BuildCreateInfo(pipelineInfo);
	pipelineCreateInfo.pipelineInfo.renderPass = vulkanWindow.GetRenderPass();

	std::array<VkClearValue, 2> clearValues;
	clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
	clearValues[1].depthStencil = {1.f, 0};

	Nz::UInt32 imageCount = vulkanWindow.GetFramebufferCount();
	std::vector<Nz::Vk::CommandBuffer> renderCmds = cmdPool.AllocateCommandBuffers(imageCount, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	Nz::RenderBuffer* renderBufferIB = static_cast<Nz::RenderBuffer*>(drfreakIB->GetBuffer()->GetImpl());
	Nz::RenderBuffer* renderBufferVB = static_cast<Nz::RenderBuffer*>(drfreakVB->GetBuffer()->GetImpl());

	if (!renderBufferIB->Synchronize(&vulkanDevice))
	{
		NazaraError("Failed to synchronize render buffer");
		return __LINE__;
	}

	if (!renderBufferVB->Synchronize(&vulkanDevice))
	{
		NazaraError("Failed to synchronize render buffer");
		return __LINE__;
	}

	Nz::VulkanBuffer* indexBufferImpl = static_cast<Nz::VulkanBuffer*>(renderBufferIB->GetHardwareBuffer(&vulkanDevice));
	Nz::VulkanBuffer* vertexBufferImpl = static_cast<Nz::VulkanBuffer*>(renderBufferVB->GetHardwareBuffer(&vulkanDevice));

	Nz::VulkanRenderPipeline* vkPipeline = static_cast<Nz::VulkanRenderPipeline*>(pipeline.get());

	for (Nz::UInt32 i = 0; i < imageCount; ++i)
	{
		Nz::Vk::CommandBuffer& renderCmd = renderCmds[i];

		VkRect2D renderArea = {
			{                                           // VkOffset2D                     offset
				0,                                          // int32_t                        x
				0                                           // int32_t                        y
			},
			{                                           // VkExtent2D                     extent
				windowSize.x,                                        // int32_t                        width
				windowSize.y,                                        // int32_t                        height
			}
		};

		VkRenderPassBeginInfo render_pass_begin_info = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,     // VkStructureType                sType
			nullptr,                                      // const void                    *pNext
			vulkanWindow.GetRenderPass(),                            // VkRenderPass                   renderPass
			vulkanWindow.GetFrameBuffer(i),                       // VkFramebuffer                  framebuffer
			renderArea,
			2U,                                            // uint32_t                       clearValueCount
			clearValues.data()                                  // const VkClearValue            *pClearValues
		};

		renderCmd.Begin();

		renderCmd.BeginRenderPass(render_pass_begin_info);
		renderCmd.BindIndexBuffer(indexBufferImpl->GetBufferHandle(), 0, VK_INDEX_TYPE_UINT16);
		renderCmd.BindVertexBuffer(0, vertexBufferImpl->GetBufferHandle(), 0);
		renderCmd.BindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineLayout->GetPipelineLayout(), 0, descriptorSet);
		renderCmd.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline->Get(vulkanWindow.GetRenderPass()));
		renderCmd.SetScissor(Nz::Recti{0, 0, int(windowSize.x), int(windowSize.y)});
		renderCmd.SetViewport({0.f, 0.f, float(windowSize.x), float(windowSize.y)}, 0.f, 1.f);
		renderCmd.DrawIndexed(drfreakIB->GetIndexCount());
		renderCmd.EndRenderPass();

		if (!renderCmd.End())
		{
			NazaraError("Failed to specify render cmd");
			return __LINE__;
		}
	}

	Nz::Vector3f viewerPos = Nz::Vector3f::Zero();

	Nz::EulerAnglesf camAngles(0.f, 0.f, 0.f);
	Nz::Quaternionf camQuat(camAngles);

	window.EnableEventPolling(true);

	struct ImageSync
	{
		Nz::Vk::Fence inflightFence;
		Nz::Vk::Semaphore imageAvailableSemaphore;
		Nz::Vk::Semaphore renderFinishedSemaphore;
	};

	const std::size_t MaxConcurrentImage = imageCount;

	std::vector<ImageSync> frameSync(MaxConcurrentImage);
	for (ImageSync& syncData : frameSync)
	{
		syncData.imageAvailableSemaphore.Create(vulkanDevice);
		syncData.renderFinishedSemaphore.Create(vulkanDevice);

		syncData.inflightFence.Create(vulkanDevice, VK_FENCE_CREATE_SIGNALED_BIT);
	}

	std::vector<Nz::Vk::Fence*> inflightFences(imageCount, nullptr);

	std::size_t currentFrame = 0;

	Nz::Clock updateClock;
	Nz::Clock secondClock;
	unsigned int fps = 0;

	while (window.IsOpen())
	{
		bool updateUniforms = false;

		Nz::WindowEvent event;
		while (window.PollEvent(&event))
		{
			switch (event.type)
			{
				case Nz::WindowEventType_Quit:
					window.Close();
					break;

				case Nz::WindowEventType_MouseMoved: // La souris a bougé
				{
					// Gestion de la caméra free-fly (Rotation)
					float sensitivity = 0.3f; // Sensibilité de la souris

					// On modifie l'angle de la caméra grâce au déplacement relatif sur X de la souris
					camAngles.yaw = Nz::NormalizeAngle(camAngles.yaw - event.mouseMove.deltaX*sensitivity);

					// Idem, mais pour éviter les problèmes de calcul de la matrice de vue, on restreint les angles
					camAngles.pitch = Nz::Clamp(camAngles.pitch + event.mouseMove.deltaY*sensitivity, -89.f, 89.f);

					camQuat = camAngles;

					// Pour éviter que le curseur ne sorte de l'écran, nous le renvoyons au centre de la fenêtre
					// Cette fonction est codée de sorte à ne pas provoquer d'évènement MouseMoved
					Nz::Mouse::SetPosition(windowSize.x / 2, windowSize.y / 2, window);
					updateUniforms = true;
					break;
				}
			}
		}

		if (updateClock.GetMilliseconds() > 1000 / 60)
		{
			float elapsedTime = updateClock.GetSeconds();
			updateClock.Restart();

			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::Up))
			{
				viewerPos += camQuat * Nz::Vector3f::Forward() * elapsedTime;
				updateUniforms = true;
			}

			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::Down))
			{
				viewerPos += camQuat * Nz::Vector3f::Backward() * elapsedTime;
				updateUniforms = true;
			}
		}

		ImageSync& syncPrimitives = frameSync[currentFrame];
		syncPrimitives.inflightFence.Wait();

		Nz::UInt32 imageIndex;
		if (!vulkanWindow.Acquire(&imageIndex, syncPrimitives.imageAvailableSemaphore))
		{
			std::cout << "Failed to acquire next image" << std::endl;
			return EXIT_FAILURE;
		}

		if (inflightFences[imageIndex])
			inflightFences[imageIndex]->Wait();

		inflightFences[imageIndex] = &syncPrimitives.inflightFence;
		inflightFences[imageIndex]->Reset();

		if (updateUniforms)
		{
			ubo.viewMatrix = Nz::Matrix4f::ViewMatrix(viewerPos, camAngles);

			void* mappedPtr = uniformBufferImpl->Map(Nz::BufferAccess_DiscardAndWrite, 0, sizeof(ubo));
			if (mappedPtr)
			{
				std::memcpy(mappedPtr, &ubo, sizeof(ubo));
				uniformBufferImpl->Unmap();
			}
		}

		if (!graphicsQueue.Submit(renderCmds[imageIndex], syncPrimitives.imageAvailableSemaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, syncPrimitives.renderFinishedSemaphore, syncPrimitives.inflightFence))
			return false;

		vulkanWindow.Present(imageIndex, syncPrimitives.renderFinishedSemaphore);

		// On incrémente le compteur de FPS improvisé
		fps++;

		if (secondClock.GetMilliseconds() >= 1000) // Toutes les secondes
		{
			// Et on insère ces données dans le titre de la fenêtre
			window.SetTitle(windowTitle + " - " + Nz::String::Number(fps) + " FPS");

			/*
			Note: En C++11 il est possible d'insérer de l'Unicode de façon standard, quel que soit l'encodage du fichier,
			via quelque chose de similaire à u8"Cha\u00CEne de caract\u00E8res".
			Cependant, si le code source est encodé en UTF-8 (Comme c'est le cas dans ce fichier),
			cela fonctionnera aussi comme ceci : "Chaîne de caractères".
			*/

			// Et on réinitialise le compteur de FPS
			fps = 0;

			// Et on relance l'horloge pour refaire ça dans une seconde
			secondClock.Restart();
		}

		currentFrame = (currentFrame + 1) % imageCount;
	}

	instance.vkDestroyDebugReportCallbackEXT(instance, callback, nullptr);

	return EXIT_SUCCESS;
}