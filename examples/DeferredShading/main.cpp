#include <Nazara/Core.hpp>
#include <Nazara/Platform.hpp>
#include <Nazara/Graphics.hpp>
#include <Nazara/Graphics/FrameGraph.hpp>
#include <Nazara/Renderer.hpp>
#include <Nazara/Shader.hpp>
#include <Nazara/Shader/SpirvConstantCache.hpp>
#include <Nazara/Shader/SpirvPrinter.hpp>
#include <Nazara/Shader/ShaderLangLexer.hpp>
#include <Nazara/Shader/ShaderLangParser.hpp>
#include <Nazara/Utility.hpp>
#include <array>
#include <iostream>
#include <random>

/*
struct PointLight
{
	color: vec3<f32>,
	position: vec3<f32>,

	constant: f32,
	linear: f32,
	quadratic: f32,
}
*/

struct PointLight
{
	Nz::Color color = Nz::Color::White;
	Nz::Vector3f position = Nz::Vector3f::Zero();

	float constantAttenuation = 0.2f;
	float linearAttenuation = 0.1f;
	float quadraticAttenuation = 0.01f;
};

int main()
{
	std::filesystem::path resourceDir = "resources";
	if (!std::filesystem::is_directory(resourceDir) && std::filesystem::is_directory(".." / resourceDir))
		resourceDir = ".." / resourceDir;

	Nz::Renderer::Config rendererConfig;
	std::cout << "Run using Vulkan? (y/n)" << std::endl;
	if (std::getchar() == 'y')
		rendererConfig.preferredAPI = Nz::RenderAPI::Vulkan;
	else
		rendererConfig.preferredAPI = Nz::RenderAPI::OpenGL;

	Nz::Modules<Nz::Graphics> nazara(rendererConfig);

	Nz::RenderWindow window;

	Nz::MeshParams meshParams;
	meshParams.storage = Nz::DataStorage_Software;
	meshParams.matrix = Nz::Matrix4f::Rotate(Nz::EulerAnglesf(0.f, 90.f, 180.f)) * Nz::Matrix4f::Scale(Nz::Vector3f(0.002f));
	meshParams.vertexDeclaration = Nz::VertexDeclaration::Get(Nz::VertexLayout_XYZ_Normal_UV);

	std::string windowTitle = "Graphics Test";
	if (!window.Create(Nz::VideoMode(1280, 720, 32), windowTitle))
	{
		std::cout << "Failed to create Window" << std::endl;
		return __LINE__;
	}

	std::shared_ptr<Nz::RenderDevice> device = window.GetRenderDevice();
	const Nz::RenderDeviceInfo& deviceInfo = device->GetDeviceInfo();

	Nz::MeshRef drfreak = Nz::Mesh::LoadFromFile(resourceDir / "Spaceship/spaceship.obj", meshParams);
	if (!drfreak)
	{
		NazaraError("Failed to load model");
		return __LINE__;
	}

	std::shared_ptr<Nz::GraphicalMesh> gfxMesh = std::make_shared<Nz::GraphicalMesh>(drfreak);

	// Texture
	Nz::ImageRef drfreakImage = Nz::Image::LoadFromFile(resourceDir / "Spaceship/Texture/diffuse.png");
	if (!drfreakImage || !drfreakImage->Convert(Nz::PixelFormat_RGBA8_SRGB))
	{
		NazaraError("Failed to load image");
		return __LINE__;
	}

	Nz::TextureInfo texParams;
	texParams.pixelFormat = drfreakImage->GetFormat();
	texParams.type = drfreakImage->GetType();
	texParams.width = drfreakImage->GetWidth();
	texParams.height = drfreakImage->GetHeight();
	texParams.depth = drfreakImage->GetDepth();

	std::shared_ptr<Nz::Texture> texture = device->InstantiateTexture(texParams);
	if (!texture->Update(drfreakImage->GetConstPixels()))
	{
		NazaraError("Failed to update texture");
		return __LINE__;
	}

	// Texture (alpha-map)
	Nz::ImageRef alphaImage = Nz::Image::LoadFromFile(resourceDir / "alphatile.png");
	if (!alphaImage || !alphaImage->Convert(Nz::PixelFormat_RGBA8))
	{
		NazaraError("Failed to load image");
		return __LINE__;
	}

	Nz::TextureInfo alphaTexParams;
	alphaTexParams.pixelFormat = alphaImage->GetFormat();
	alphaTexParams.type = alphaImage->GetType();
	alphaTexParams.width = alphaImage->GetWidth();
	alphaTexParams.height = alphaImage->GetHeight();
	alphaTexParams.depth = alphaImage->GetDepth();

	std::shared_ptr<Nz::Texture> alphaTexture = device->InstantiateTexture(alphaTexParams);
	if (!alphaTexture->Update(alphaImage->GetConstPixels()))
	{
		NazaraError("Failed to update texture");
		return __LINE__;
	}

	auto customSettings = Nz::BasicMaterial::GetSettings()->GetBuilderData();
	customSettings.shaders[UnderlyingCast(Nz::ShaderStageType::Fragment)] = std::make_shared<Nz::UberShader>(Nz::ShaderStageType::Fragment, Nz::ShaderLang::Parse(resourceDir / "deferred_frag.nzsl"));
	customSettings.shaders[UnderlyingCast(Nz::ShaderStageType::Vertex)] = std::make_shared<Nz::UberShader>(Nz::ShaderStageType::Vertex, Nz::ShaderLang::Parse(resourceDir / "deferred_vert.nzsl"));

	std::shared_ptr<Nz::Material> material = std::make_shared<Nz::Material>(std::make_shared<Nz::MaterialSettings>(std::move(customSettings)));
	material->EnableDepthBuffer(true);

	Nz::BasicMaterial basicMat(*material);
	basicMat.EnableAlphaTest(false);
	basicMat.SetAlphaMap(alphaTexture);
	basicMat.SetDiffuseMap(texture);

	Nz::Model model(std::move(gfxMesh));
	for (std::size_t i = 0; i < model.GetSubMeshCount(); ++i)
		model.SetMaterial(i, material);

	Nz::PredefinedInstanceData instanceUboOffsets = Nz::PredefinedInstanceData::GetOffsets();
	Nz::PredefinedViewerData viewerUboOffsets = Nz::PredefinedViewerData::GetOffsets();

	std::vector<std::uint8_t> viewerDataBuffer(viewerUboOffsets.totalSize);

	Nz::Vector2ui windowSize = window.GetSize();

	Nz::AccessByOffset<Nz::Matrix4f&>(viewerDataBuffer.data(), viewerUboOffsets.viewMatrixOffset) = Nz::Matrix4f::Translate(Nz::Vector3f::Backward() * 1);
	Nz::AccessByOffset<Nz::Matrix4f&>(viewerDataBuffer.data(), viewerUboOffsets.projMatrixOffset) = Nz::Matrix4f::Perspective(70.f, float(windowSize.x) / windowSize.y, 0.1f, 1000.f);

	std::vector<std::uint8_t> instanceDataBuffer(instanceUboOffsets.totalSize);

	Nz::ModelInstance modelInstance1(material->GetSettings());
	{
		material->UpdateShaderBinding(modelInstance1.GetShaderBinding());

		Nz::AccessByOffset<Nz::Matrix4f&>(instanceDataBuffer.data(), instanceUboOffsets.worldMatrixOffset) = Nz::Matrix4f::Translate(Nz::Vector3f::Forward() * 2 + Nz::Vector3f::Right());

		std::shared_ptr<Nz::AbstractBuffer>& instanceDataUBO = modelInstance1.GetInstanceBuffer();
		instanceDataUBO->Fill(instanceDataBuffer.data(), 0, instanceDataBuffer.size());
	}

	Nz::ModelInstance modelInstance2(material->GetSettings());
	{
		material->UpdateShaderBinding(modelInstance2.GetShaderBinding());

		Nz::AccessByOffset<Nz::Matrix4f&>(instanceDataBuffer.data(), instanceUboOffsets.worldMatrixOffset) = Nz::Matrix4f::Translate(Nz::Vector3f::Forward() * 2 + Nz::Vector3f::Right() * 3.f);

		std::shared_ptr<Nz::AbstractBuffer>& instanceDataUBO = modelInstance2.GetInstanceBuffer();
		instanceDataUBO->Fill(instanceDataBuffer.data(), 0, instanceDataBuffer.size());
	}

	std::shared_ptr<Nz::AbstractBuffer> viewerDataUBO = Nz::Graphics::Instance()->GetViewerDataUBO();

	Nz::RenderWindowImpl* windowImpl = window.GetImpl();
	std::shared_ptr<Nz::CommandPool> commandPool = windowImpl->CreateCommandPool(Nz::QueueType::Graphics);

	Nz::RenderPipelineLayoutInfo fullscreenPipelineLayoutInfo;
	fullscreenPipelineLayoutInfo.bindings.push_back({
		Nz::ShaderBindingType::Texture,
		Nz::ShaderStageType::Fragment,
		0
	});

	Nz::RenderPipelineLayoutInfo lightingPipelineLayoutInfo;
	for (unsigned int i = 0; i < 3; ++i)
	{
		lightingPipelineLayoutInfo.bindings.push_back({
			Nz::ShaderBindingType::Texture,
			Nz::ShaderStageType::Fragment,
			i
		});
	}

	lightingPipelineLayoutInfo.bindings.push_back({
		Nz::ShaderBindingType::UniformBuffer,
		Nz::ShaderStageType::Fragment,
		3
	});

	Nz::FieldOffsets pointLightOffsets(Nz::StructLayout_Std140);
	std::size_t colorOffset = pointLightOffsets.AddField(Nz::StructFieldType_Float3);
	std::size_t positionOffset = pointLightOffsets.AddField(Nz::StructFieldType_Float3);
	std::size_t constantOffset = pointLightOffsets.AddField(Nz::StructFieldType_Float1);
	std::size_t linearOffset = pointLightOffsets.AddField(Nz::StructFieldType_Float1);
	std::size_t quadraticOffset = pointLightOffsets.AddField(Nz::StructFieldType_Float1);

	std::size_t alignedPointLightSize = Nz::Align(pointLightOffsets.GetSize(), static_cast<std::size_t>(deviceInfo.limits.minUniformBufferOffsetAlignment));

	constexpr std::size_t MaxPointLight = 100;

	std::shared_ptr<Nz::AbstractBuffer> lightUbo = device->InstantiateBuffer(Nz::BufferType_Uniform);
	if (!lightUbo->Initialize(MaxPointLight * alignedPointLightSize, Nz::BufferUsage_DeviceLocal | Nz::BufferUsage_Dynamic))
		return __LINE__;

	std::vector<PointLight> pointLights;

	std::random_device rng;
	std::mt19937 randomEngine(rng());
	std::uniform_int_distribution<unsigned int> colorDis(0, 255);
	std::uniform_real_distribution<float> posDis(-5.f, 5.f);

	for (std::size_t i = 0; i < 50; ++i)
	{
		auto& light = pointLights.emplace_back();
		light.color = Nz::Color(colorDis(randomEngine), colorDis(randomEngine), colorDis(randomEngine));
		light.position = Nz::Vector3f(posDis(randomEngine), posDis(randomEngine), posDis(randomEngine));
		light.constantAttenuation = 0.7f;
		light.linearAttenuation = 0.0f;
		light.quadraticAttenuation = 0.2f;
	}


	const Nz::VertexDeclarationRef& vertexDeclaration = Nz::VertexDeclaration::Get(Nz::VertexLayout_XYZ_UV);


	unsigned int offscreenWidth = window.GetSize().x;
	unsigned int offscreenHeight = window.GetSize().y;

	// Fullscreen data

	Nz::RenderPipelineInfo fullscreenPipelineInfo;
	fullscreenPipelineInfo.primitiveMode = Nz::PrimitiveMode_TriangleStrip;
	fullscreenPipelineInfo.pipelineLayout = device->InstantiateRenderPipelineLayout(fullscreenPipelineLayoutInfo);
	fullscreenPipelineInfo.vertexBuffers.push_back({
		0,
		vertexDeclaration
	});

	fullscreenPipelineInfo.shaderModules.push_back(device->InstantiateShaderModule(Nz::ShaderStageType::Fragment, Nz::ShaderLanguage::NazaraBinary, resourceDir / "fullscreen.frag.shader", {}));
	fullscreenPipelineInfo.shaderModules.push_back(device->InstantiateShaderModule(Nz::ShaderStageType::Vertex, Nz::ShaderLanguage::NazaraBinary, resourceDir / "fullscreen.vert.shader", {}));


	std::shared_ptr<Nz::RenderPipeline> fullscreenPipeline = device->InstantiateRenderPipeline(fullscreenPipelineInfo);

	Nz::RenderPipelineInfo lightingPipelineInfo;
	lightingPipelineInfo.blending = true;
	lightingPipelineInfo.blend.dstColor = Nz::BlendFunc::One;
	lightingPipelineInfo.blend.srcColor = Nz::BlendFunc::One;
	lightingPipelineInfo.primitiveMode = Nz::PrimitiveMode_TriangleStrip;
	lightingPipelineInfo.pipelineLayout = device->InstantiateRenderPipelineLayout(lightingPipelineLayoutInfo);
	lightingPipelineInfo.vertexBuffers.push_back({
		0,
		vertexDeclaration
	});

	lightingPipelineInfo.shaderModules.push_back(device->InstantiateShaderModule(Nz::ShaderStageType::Fragment | Nz::ShaderStageType::Vertex, Nz::ShaderLanguage::NazaraShader, resourceDir / "lighting.nzsl", {}));

	std::shared_ptr<Nz::RenderPipeline> lightingPipeline = device->InstantiateRenderPipeline(lightingPipelineInfo);

	std::vector<std::shared_ptr<Nz::ShaderBinding>> lightingShaderBindings;

	/*
	std::array<Nz::VertexStruct_XYZ_UV, 3> vertexData = {
		{
			{
				Nz::Vector3f(-1.f, 1.f, 0.0f),
				Nz::Vector2f(0.0f, 1.0f),
			},
			{
				Nz::Vector3f(-1.f, -3.f, 0.0f),
				Nz::Vector2f(0.0f, -1.0f),
			},
			{
				Nz::Vector3f(3.f, 1.f, 0.0f),
				Nz::Vector2f(2.0f, 1.0f),
			}
		}
	};
	*/

	std::array<Nz::VertexStruct_XYZ_UV, 4> vertexData = {
		{
			{
				Nz::Vector3f(-1.f, -1.f, 0.0f),
				Nz::Vector2f(0.0f, 0.0f),
			},
			{
				Nz::Vector3f(1.f, -1.f, 0.0f),
				Nz::Vector2f(1.0f, 0.0f),
			},
			{
				Nz::Vector3f(-1.f, 1.f, 0.0f),
				Nz::Vector2f(0.0f, 1.0f),
			},
			{
				Nz::Vector3f(1.f, 1.f, 0.0f),
				Nz::Vector2f(1.0f, 1.0f),
			},
		}
	};

	std::shared_ptr<Nz::AbstractBuffer> vertexBuffer = device->InstantiateBuffer(Nz::BufferType_Vertex);
	if (!vertexBuffer->Initialize(vertexDeclaration->GetStride() * vertexData.size(), Nz::BufferUsage_DeviceLocal))
		return __LINE__;

	if (!vertexBuffer->Fill(vertexData.data(), 0, vertexBuffer->GetSize()))
		return __LINE__;

	std::shared_ptr<Nz::ShaderBinding> finalBlitBinding = fullscreenPipelineInfo.pipelineLayout->AllocateShaderBinding();

	std::size_t colorTexture;
	std::size_t normalTexture;
	std::size_t positionTexture;
	std::size_t depthBuffer;
	std::size_t backbuffer;

	bool viewerUboUpdate = true;

	Nz::BakedFrameGraph bakedGraph = [&]{
		Nz::FrameGraph graph;

		colorTexture = graph.AddAttachment({
			"Color",
			Nz::PixelFormat_RGBA8
		});
		
		normalTexture = graph.AddAttachment({
			"Normal",
			Nz::PixelFormat_RGBA8
		});

		positionTexture = graph.AddAttachment({
			"Position",
			Nz::PixelFormat_RGBA32F
		});

		depthBuffer = graph.AddAttachment({
			"Depth buffer",
			Nz::PixelFormat_Depth24Stencil8
		});

		backbuffer = graph.AddAttachment({
			"Backbuffer",
			Nz::PixelFormat_RGBA8
		});

		Nz::FramePass& gbufferPass = graph.AddPass("GBuffer");

		std::size_t geometryAlbedo = gbufferPass.AddOutput(colorTexture);
		gbufferPass.SetClearColor(geometryAlbedo, Nz::Color::Black);

		std::size_t geometryNormal = gbufferPass.AddOutput(normalTexture);
		gbufferPass.SetClearColor(geometryNormal, Nz::Color::Black);

		std::size_t positionAttachment = gbufferPass.AddOutput(positionTexture);
		gbufferPass.SetClearColor(positionAttachment, Nz::Color::Black);

		gbufferPass.SetDepthStencilClear(1.f, 0);

		gbufferPass.SetDepthStencilOutput(depthBuffer);

		gbufferPass.SetCommandCallback([&](Nz::CommandBufferBuilder& builder)
		{
			builder.SetScissor(Nz::Recti{ 0, 0, int(offscreenWidth), int(offscreenHeight) });
			builder.SetViewport(Nz::Recti{ 0, 0, int(offscreenWidth), int(offscreenHeight) });

			for (Nz::ModelInstance& modelInstance : { std::ref(modelInstance1), std::ref(modelInstance2) })
			{
				builder.BindShaderBinding(modelInstance.GetShaderBinding());

				for (std::size_t i = 0; i < model.GetSubMeshCount(); ++i)
				{
					builder.BindIndexBuffer(model.GetIndexBuffer(i).get());
					builder.BindVertexBuffer(0, model.GetVertexBuffer(i).get());
					builder.BindPipeline(*model.GetRenderPipeline(i));

					builder.DrawIndexed(static_cast<Nz::UInt32>(model.GetIndexCount(i)));
				}
			}
		});

		Nz::FramePass& lightingPass = graph.AddPass("Lighting pass");
		lightingPass.SetExecutionCallback([&]
		{
			return (viewerUboUpdate) ? Nz::FramePassExecution::UpdateAndExecute : Nz::FramePassExecution::Execute;
		});

		lightingPass.SetCommandCallback([&](Nz::CommandBufferBuilder& builder)
		{
			builder.SetScissor(Nz::Recti{ 0, 0, int(offscreenWidth), int(offscreenHeight) });
			builder.SetViewport(Nz::Recti{ 0, 0, int(offscreenWidth), int(offscreenHeight) });

			builder.BindPipeline(*lightingPipeline);
			builder.BindVertexBuffer(0, vertexBuffer.get());

			for (std::size_t i = 0; i < pointLights.size(); ++i)
			{
				builder.BindShaderBinding(*lightingShaderBindings[i]);
				builder.Draw(4);
			}
		});

		lightingPass.AddInput(colorTexture);
		lightingPass.AddInput(normalTexture);
		lightingPass.AddInput(positionTexture);
		lightingPass.SetClearColor(lightingPass.AddOutput(backbuffer), Nz::Color::Black);
		//lightingPass.SetDepthStencilInput(depthBuffer);

		graph.SetBackbufferOutput(backbuffer);

		return graph.Bake();
	}();

	bakedGraph.Resize(offscreenWidth, offscreenHeight);

	std::shared_ptr<Nz::TextureSampler> textureSampler = device->InstantiateTextureSampler({});


	for (std::size_t i = 0; i < MaxPointLight; ++i)
	{
		std::shared_ptr<Nz::ShaderBinding> lightingShaderBinding = lightingPipelineInfo.pipelineLayout->AllocateShaderBinding();
		lightingShaderBinding->Update({
			{
				0,
				Nz::ShaderBinding::TextureBinding {
					bakedGraph.GetAttachmentTexture(colorTexture).get(),
					textureSampler.get()
				}
			},
			{
				1,
				Nz::ShaderBinding::TextureBinding {
					bakedGraph.GetAttachmentTexture(normalTexture).get(),
					textureSampler.get()
				}
			},
			{
				2,
				Nz::ShaderBinding::TextureBinding {
					bakedGraph.GetAttachmentTexture(positionTexture).get(),
					textureSampler.get()
				}
			},
			{
				3,
				Nz::ShaderBinding::UniformBufferBinding {
					lightUbo.get(),
					i * alignedPointLightSize, pointLightOffsets.GetSize()
				}
			}
		});

		lightingShaderBindings.emplace_back(std::move(lightingShaderBinding));
	}

	finalBlitBinding->Update({
		{
			0,
			Nz::ShaderBinding::TextureBinding {
				bakedGraph.GetAttachmentTexture(backbuffer).get(),
				textureSampler.get()
			}
		}
	});


	Nz::CommandBufferPtr drawCommandBuffer;
	auto RebuildCommandBuffer = [&]
	{
		Nz::Vector2ui windowSize = window.GetSize();

		drawCommandBuffer = commandPool->BuildCommandBuffer([&](Nz::CommandBufferBuilder& builder)
		{
			Nz::Recti windowRenderRect(0, 0, window.GetSize().x, window.GetSize().y);

			builder.TextureBarrier(Nz::PipelineStage::ColorOutput, Nz::PipelineStage::FragmentShader, Nz::MemoryAccess::ColorWrite, Nz::MemoryAccess::ShaderRead, Nz::TextureLayout::ColorOutput, Nz::TextureLayout::ColorInput, *bakedGraph.GetAttachmentTexture(backbuffer));

			builder.BeginDebugRegion("Main window rendering", Nz::Color::Green);
			{
				builder.BeginRenderPass(windowImpl->GetFramebuffer(), windowImpl->GetRenderPass(), windowRenderRect);
				{
					builder.SetScissor(Nz::Recti{ 0, 0, int(windowSize.x), int(windowSize.y) });
					builder.SetViewport(Nz::Recti{ 0, 0, int(windowSize.x), int(windowSize.y) });

					builder.BindShaderBinding(*finalBlitBinding);
					builder.BindPipeline(*fullscreenPipeline);
					builder.BindVertexBuffer(0, vertexBuffer.get());
					builder.Draw(4);
				}
				builder.EndRenderPass();
			}
			builder.EndDebugRegion();
		});
	};
	RebuildCommandBuffer();


	Nz::Vector3f viewerPos = Nz::Vector3f::Zero();

	Nz::EulerAnglesf camAngles(0.f, 0.f, 0.f);
	Nz::Quaternionf camQuat(camAngles);

	window.EnableEventPolling(true);

	Nz::Clock updateClock;
	Nz::Clock secondClock;
	unsigned int fps = 0;

	std::size_t totalFrameCount = 0;

	Nz::Mouse::SetRelativeMouseMode(true);

	while (window.IsOpen())
	{
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
					
					viewerUboUpdate = true;
					break;
				}

				case Nz::WindowEventType_KeyPressed:
				{
					if (event.key.scancode == Nz::Keyboard::Scancode::Space)
					{
						auto& whiteLight = pointLights.emplace_back();
						whiteLight.constantAttenuation = 0.8f;
						whiteLight.position = viewerPos;

						viewerUboUpdate = true;
					}
					break;
				}

				case Nz::WindowEventType_Resized:
				{
					Nz::Vector2ui windowSize = window.GetSize();
					Nz::AccessByOffset<Nz::Matrix4f&>(viewerDataBuffer.data(), viewerUboOffsets.projMatrixOffset) = Nz::Matrix4f::Perspective(70.f, float(windowSize.x) / windowSize.y, 0.1f, 1000.f);
					viewerUboUpdate = true;
					break;
				}

				default:
					break;
			}
		}

		if (updateClock.GetMilliseconds() > 1000 / 60)
		{
			float cameraSpeed = 2.f * updateClock.GetSeconds();
			updateClock.Restart();

			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::VKey::Up) || Nz::Keyboard::IsKeyPressed(Nz::Keyboard::VKey::Z))
				viewerPos += camQuat * Nz::Vector3f::Forward() * cameraSpeed;

			// Si la flèche du bas ou la touche S est pressée, on recule
			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::VKey::Down) || Nz::Keyboard::IsKeyPressed(Nz::Keyboard::VKey::S))
				viewerPos += camQuat * Nz::Vector3f::Backward() * cameraSpeed;

			// Etc...
			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::VKey::Left) || Nz::Keyboard::IsKeyPressed(Nz::Keyboard::VKey::Q))
				viewerPos += camQuat * Nz::Vector3f::Left() * cameraSpeed;

			// Etc...
			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::VKey::Right) || Nz::Keyboard::IsKeyPressed(Nz::Keyboard::VKey::D))
				viewerPos += camQuat * Nz::Vector3f::Right() * cameraSpeed;

			// Majuscule pour monter, notez l'utilisation d'une direction globale (Non-affectée par la rotation)
			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::VKey::LShift) || Nz::Keyboard::IsKeyPressed(Nz::Keyboard::VKey::RShift))
				viewerPos += Nz::Vector3f::Up() * cameraSpeed;

			// Contrôle (Gauche ou droite) pour descendre dans l'espace global, etc...
			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::VKey::LControl) || Nz::Keyboard::IsKeyPressed(Nz::Keyboard::VKey::RControl))
				viewerPos += Nz::Vector3f::Down() * cameraSpeed;

			viewerUboUpdate = true;
		}

		Nz::RenderFrame frame = windowImpl->Acquire();
		if (!frame)
			continue;

		if (frame.IsFramebufferInvalidated())
			RebuildCommandBuffer();

		if (viewerUboUpdate)
		{
			Nz::AccessByOffset<Nz::Matrix4f&>(viewerDataBuffer.data(), viewerUboOffsets.viewMatrixOffset) = Nz::Matrix4f::ViewMatrix(viewerPos, camAngles);

			//Nz::AccessByOffset<Nz::Vector3f&>(lightData.data(), colorOffset) = Nz::Vector3f(std::sin(totalFrameCount / 10000.f) * 0.5f + 0.5f, std::cos(totalFrameCount / 1000.f) * 0.5f + 0.5f, std::sin(totalFrameCount / 100.f) * 0.5f + 0.5f);

			Nz::UploadPool& uploadPool = frame.GetUploadPool();

			frame.Execute([&](Nz::CommandBufferBuilder& builder)
			{
				builder.BeginDebugRegion("UBO Update", Nz::Color::Yellow);
				{
					builder.PreTransferBarrier();

					auto& viewerDataAllocation = uploadPool.Allocate(viewerDataBuffer.size());
					std::memcpy(viewerDataAllocation.mappedPtr, viewerDataBuffer.data(), viewerDataBuffer.size());
					builder.CopyBuffer(viewerDataAllocation, viewerDataUBO.get());

					if (!pointLights.empty())
					{
						auto& lightDataAllocation = uploadPool.Allocate(alignedPointLightSize * pointLights.size());
						Nz::UInt8* lightDataPtr = static_cast<Nz::UInt8*>(lightDataAllocation.mappedPtr);
						for (const PointLight& pointLight : pointLights)
						{
							Nz::AccessByOffset<Nz::Vector3f&>(lightDataPtr, colorOffset) = Nz::Vector3f(pointLight.color.r / 255.f, pointLight.color.g / 255.f, pointLight.color.b / 255.f);
							Nz::AccessByOffset<Nz::Vector3f&>(lightDataPtr, positionOffset) = pointLight.position;
							Nz::AccessByOffset<float&>(lightDataPtr, constantOffset) = pointLight.constantAttenuation;
							Nz::AccessByOffset<float&>(lightDataPtr, linearOffset) = pointLight.linearAttenuation;
							Nz::AccessByOffset<float&>(lightDataPtr, quadraticOffset) = pointLight.quadraticAttenuation;

							lightDataPtr += alignedPointLightSize;
						}

						builder.CopyBuffer(lightDataAllocation, lightUbo.get());
					}

					material->UpdateBuffers(uploadPool, builder);

					builder.PostTransferBarrier();
				}
				builder.EndDebugRegion();
			}, Nz::QueueType::Transfer);
		}

		bakedGraph.Execute(frame);
		frame.SubmitCommandBuffer(drawCommandBuffer.get(), Nz::QueueType::Graphics);

		frame.Present();

		window.Display();

		viewerUboUpdate = false;

		// On incrémente le compteur de FPS improvisé
		fps++;
		totalFrameCount++;

		if (secondClock.GetMilliseconds() >= 1000) // Toutes les secondes
		{
			// Et on insère ces données dans le titre de la fenêtre
			window.SetTitle(windowTitle + " - " + Nz::NumberToString(fps) + " FPS");

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
	}

	return EXIT_SUCCESS;
}