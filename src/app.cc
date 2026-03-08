static constexpr u32 SCREEN_FACTOR = 80;
static constexpr u32 WIDTH = 16 * SCREEN_FACTOR;
static constexpr u32 HEIGHT = 9 * SCREEN_FACTOR;

#if DEBUG
static const char* TITLE = "Window! (Debug)";
#else
static const char* TITLE = "Window!";
#endif

static constexpr u32 INVALID_QUEUE = UINT32_MAX;
static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

struct App {
	App();
	~App();

	void init_glfw();
	void create_instance();
	void pick_device();
	void create_device();
	void create_surface();
	void create_swapchain();
	void create_sync_objects();
	void create_command_buffers();
	void create_graphics_pipeline();

	void run();
	VkShaderModule create_shader_module(const char* path);
	void record_draw_buffer(VkCommandBuffer cmd, u32 image_index);
	void draw_frame();

	GLFWwindow* window;
	VkInstance instance;
	VkPhysicalDeviceSurfaceInfo2KHR surface_info;
	VkSurfaceCapabilities2KHR surface_capabilities;
	VkDebugUtilsMessengerEXT messenger;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device;

    struct {
		VkSwapchainKHR swapchain;
		VkImage* images;
		VkImageView* image_views;
		u32 image_count;
		VkExtent2D extent;
		VkFormat format;

		VkSemaphore* render_finished;
		VkFence* in_flight;
	} swapchain;

	struct {
		VkQueue graphics;
		VkQueue present;
		VkQueue compute;
		VkQueue transfer;
	} queues;

	struct {
		u32 graphics = INVALID_QUEUE;
		u32 present = INVALID_QUEUE;
		u32 compute = INVALID_QUEUE;
		u32 transfer = INVALID_QUEUE;
	} families;

	struct {
		VkCommandPool graphics_pool;
		VkCommandPool compute_pool;
		VkCommandPool transfer_pool;

		VkCommandBuffer graphics_cmd;
		VkCommandBuffer compute_cmd;
		VkCommandBuffer transfer_cmd;

		VkSemaphore img_available;
		VkFence in_flight;
	} frames[MAX_FRAMES_IN_FLIGHT];

	struct {
		VkPipelineLayout graphics_layout;
		VkPipeline graphics;
	} pipeline;

	Arena arena{GB(1)};
	u32 current_frame = 0;
};

static void error_callback(s32 code, const char* desc) {
	Panic("GLFW ERROR (%d): %s", code, desc);
}

static void key_callback(GLFWwindow* window, s32 key, s32 scancode, s32 action, s32 mods) {
	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}

#if DEBUG
static VkBool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
							   VkDebugUtilsMessageTypeFlagsEXT type,
							   const VkDebugUtilsMessengerCallbackDataEXT* data,
							   void* user_data)
{
	switch (severity) {
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		Log("(%d) %s:\n%s", data->messageIdNumber, data->pMessageIdName, data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		Warn("(%d) %s:\n%s", data->messageIdNumber, data->pMessageIdName, data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		Error("(%d) %s:\n%s", data->messageIdNumber, data->pMessageIdName, data->pMessage);
		break;
	default:
		Log("(%d) %s:\n%s", data->messageIdNumber, data->pMessageIdName, data->pMessage);
		break;
	}

	return VK_FALSE;
}
#endif

App::App() {
	init_glfw();
	create_instance();
	pick_device();
	create_device();
	create_surface();
	create_swapchain();
	create_sync_objects();
	create_command_buffers();
	create_graphics_pipeline();
}

void App::init_glfw() {
	glfwSetErrorCallback(error_callback);
	if (glfwInit() != GLFW_TRUE) {
		Panic("Failed to initialize GLFW!");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(WIDTH, HEIGHT, TITLE, NULL, NULL);
	if (!window) {
		glfwTerminate();
		Panic("Failed to create window!");
	}
	glfwSetKeyCallback(window, key_callback);
}

void App::create_instance() {
	VkApplicationInfo app_info{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Sam's Playground!",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "Sam's Engine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_4,
	};

	u32 extensions_count;
	const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
	const char** final_extensions = arena.alloc<const char*>(extensions_count + 2);
	for (u32 i = 0; i < extensions_count; i++) {
		final_extensions[i] = extensions[i];
	}
	final_extensions[extensions_count++] = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;

#if DEBUG
	VkValidationFeatureEnableEXT features[] = {
		VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
	};

	VkValidationFeaturesEXT validation_features{
		.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
		.enabledValidationFeatureCount = 1,
		.pEnabledValidationFeatures = features,
	};

	VkDebugUtilsMessengerCreateInfoEXT dbg_info{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.pNext = &validation_features,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
		                 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		             | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = debug_callback,
		.pUserData = NULL,
	};

	final_extensions[extensions_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

	const char* validation_layers[] = {
		"VK_LAYER_KHRONOS_validation",
	};
#endif

	VkInstanceCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
#if DEBUG
		.pNext = &dbg_info,
#endif
		.pApplicationInfo = &app_info,
#if DEBUG
		.enabledLayerCount = 1,
		.ppEnabledLayerNames = validation_layers,
#endif
		.enabledExtensionCount = extensions_count,
		.ppEnabledExtensionNames = final_extensions,
	};

	if (vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS) {
		Panic("Failed to create Vulkan instance!");
	}

#if DEBUG
	auto create_dbg_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    create_dbg_messenger(instance, &dbg_info, NULL, &messenger);
#endif
}

void App::pick_device() {
	u32 device_count;
	vkEnumeratePhysicalDevices(instance, &device_count, NULL);
	VkPhysicalDevice* devices = arena.alloc<VkPhysicalDevice>(device_count);
	vkEnumeratePhysicalDevices(instance, &device_count, devices);

	u32 best_score = 0;
	for (u32 i = 0; i < device_count; i++) {
		u8* mark = arena.mark();
		u32 family_count;
		vkGetPhysicalDeviceQueueFamilyProperties2(devices[i], &family_count, NULL);
		VkQueueFamilyProperties2* properties = arena.alloc<VkQueueFamilyProperties2>(family_count);
		for (u32 j = 0; j < family_count; j++) {
			properties[j] = {
				.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,
				.pNext = NULL,
			};
		}
		vkGetPhysicalDeviceQueueFamilyProperties2(devices[i], &family_count, properties);

		VkPhysicalDeviceProperties2 props{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		vkGetPhysicalDeviceProperties2(devices[i], &props);

		u32 score = 0;
		switch (props.properties.deviceType) {
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score = 5; break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score = 4; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: score = 3; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: score = 2; break;
		case VK_PHYSICAL_DEVICE_TYPE_OTHER: score = 1; break;
		default: score = 0; break;
		}

		u32 graphics_family = INVALID_QUEUE;
		u32 compute_family = INVALID_QUEUE;
		u32 present_family = INVALID_QUEUE;
		u32 transfer_family = INVALID_QUEUE;

		for (u32 j = 0; j < family_count; j++) {
			VkQueueFlags flags = properties[j].queueFamilyProperties.queueFlags;
			bool graphics = flags & VK_QUEUE_GRAPHICS_BIT;
			bool compute = flags & VK_QUEUE_COMPUTE_BIT;
			bool transfer = flags & VK_QUEUE_TRANSFER_BIT;
			bool present = glfwGetPhysicalDevicePresentationSupport(instance, devices[i], j);

			if (graphics && present && graphics_family == INVALID_QUEUE && present_family == INVALID_QUEUE) {
				graphics_family = j;
				present_family = j;
			}

			if (graphics && graphics_family == INVALID_QUEUE) {
				graphics_family = j;
			}

			if (compute && !graphics && compute_family == INVALID_QUEUE) {
				compute_family = j;
			}

			if (present && present_family == INVALID_QUEUE) {
				present_family = j;
			}

			if (transfer && !graphics && !compute && transfer_family == INVALID_QUEUE) {
				transfer_family = j;
			}
		}

		if (graphics_family != INVALID_QUEUE && present_family != INVALID_QUEUE) {
			if (transfer_family == INVALID_QUEUE) {
				transfer_family = graphics_family;
			}

			if (compute_family == INVALID_QUEUE) {
				compute_family = graphics_family;
			}

			if (score > best_score) {
				families = {
					.graphics = graphics_family,
					.present = present_family,
					.compute = compute_family,
					.transfer = transfer_family,
				};
				best_score = score;
				physical_device = devices[i];
			}
		}

		arena.reset(mark);
	}

	if (physical_device == VK_NULL_HANDLE) {
		Panic("Failed to find suitable device!");
	}
}

void App::create_device() {
	u32 family_indices[] = {
		families.graphics,
		families.present,
		families.compute,
		families.transfer,
	};

	f32 queue_priority = 1.0f;

	VkDeviceQueueCreateInfo queue_infos[4];
	u32 queue_info_count = 0;

	for (u32 i = 0; i < 4; i++) {
		bool duplicate = false;

		for (u32 j = 0; j < queue_info_count; j++) {
			if (queue_infos[j].queueFamilyIndex == family_indices[i]) {
				duplicate = true;
				break;
			}
		}

		if (duplicate) {
			continue;
		}

		queue_infos[queue_info_count++] = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = family_indices[i],
			.queueCount = 1,
			.pQueuePriorities = &queue_priority,
		};
	}

	VkPhysicalDeviceVulkan11Features v11{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
		.shaderDrawParameters = VK_TRUE,
	};

	VkPhysicalDeviceVulkan13Features v13{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.pNext = &v11,
		.synchronization2 = VK_TRUE,
		.dynamicRendering = VK_TRUE,
	};

	VkPhysicalDeviceFeatures2 dev_features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &v13,
		.features = {
			.samplerAnisotropy = VK_TRUE,
		},
	};

	const char* extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
	};

	VkDeviceCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &dev_features,
		.queueCreateInfoCount = queue_info_count,
		.pQueueCreateInfos = queue_infos,
		.enabledExtensionCount = sizeof(extensions) / sizeof(const char*),
		.ppEnabledExtensionNames = extensions,
	};

	if (vkCreateDevice(physical_device, &create_info, NULL, &device) != VK_SUCCESS) {
		Panic("Failed to create logical device!");
	}

	vkGetDeviceQueue(device, families.graphics, 0, &queues.graphics);
	vkGetDeviceQueue(device, families.present, 0, &queues.present);
	vkGetDeviceQueue(device, families.compute, 0, &queues.compute);
	vkGetDeviceQueue(device, families.transfer, 0, &queues.transfer);

	if (queues.graphics == VK_NULL_HANDLE ||
		queues.present == VK_NULL_HANDLE ||
		queues.compute == VK_NULL_HANDLE ||
		queues.transfer == VK_NULL_HANDLE) {
		Panic("Failed to get device queues!");
	}
}

void App::create_surface() {
	surface_info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
	surface_info.pNext = NULL;

	if (glfwCreateWindowSurface(instance, window, NULL, &surface_info.surface) != VK_SUCCESS) {
		Panic("Failed to create window surface!");
	}

	surface_capabilities = {
		.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
	};
	if (vkGetPhysicalDeviceSurfaceCapabilities2KHR(physical_device, &surface_info, &surface_capabilities) != VK_SUCCESS) {
		Panic("Failed to get surface capabilities!");
	}
}

void App::create_swapchain() {
	if (surface_capabilities.surfaceCapabilities.currentExtent.width != UINT32_MAX) {
		swapchain.extent = surface_capabilities.surfaceCapabilities.currentExtent;
	} else {
		glfwGetFramebufferSize(window, (s32*)&swapchain.extent.width, (s32*)&swapchain.extent.height);
	}

	swapchain.format = VK_FORMAT_R8G8B8A8_SRGB;

	u32 image_count = surface_capabilities.surfaceCapabilities.minImageCount + 1;
	if (surface_capabilities.surfaceCapabilities.maxImageCount > 0) {
		image_count = MIN(image_count, surface_capabilities.surfaceCapabilities.maxImageCount);
	}

	VkSwapchainCreateInfoKHR create_info{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface_info.surface,
		.minImageCount = image_count,
		.imageFormat = swapchain.format,
		.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		.imageExtent = swapchain.extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = surface_capabilities.surfaceCapabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR,
		.clipped = VK_TRUE,
		.oldSwapchain = VK_NULL_HANDLE,
	};

	u32 queue_indices[] = { families.graphics, families.present };
	if (families.graphics != families.present) {
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_indices;
	} else {
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	if (vkCreateSwapchainKHR(device, &create_info, NULL, &swapchain.swapchain) != VK_SUCCESS) {
		Panic("Failed to create swapchain!");
	}

	vkGetSwapchainImagesKHR(device, swapchain.swapchain, &swapchain.image_count, NULL);
	swapchain.images = (VkImage*)malloc(sizeof(VkImage) * swapchain.image_count);
	swapchain.image_views = (VkImageView*)malloc(sizeof(VkImageView) * swapchain.image_count);
	vkGetSwapchainImagesKHR(device, swapchain.swapchain, &swapchain.image_count, swapchain.images);

	for (u32 i = 0; i < swapchain.image_count; i++) {
		VkImageViewCreateInfo img_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain.images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = swapchain.format,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		if (vkCreateImageView(device, &img_info, NULL, &swapchain.image_views[i]) != VK_SUCCESS) {
			Panic("Failed to create image view %u!", i);
		}
	}
}

void App::create_sync_objects() {
	VkSemaphoreCreateInfo sem_info{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	VkFenceCreateInfo fence_info{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(device, &sem_info, NULL, &frames[i].img_available) != VK_SUCCESS ||
			vkCreateFence(device, &fence_info, NULL, &frames[i].in_flight) != VK_SUCCESS) {
			Panic("Failed to create sync object %u!", i);
		}
	}

	swapchain.render_finished = (VkSemaphore*)malloc(sizeof(VkSemaphore) * swapchain.image_count);
	swapchain.in_flight = (VkFence*)malloc(sizeof(VkFence) * swapchain.image_count);
	for (u32 i = 0; i < swapchain.image_count; i++) {
		if (vkCreateSemaphore(device, &sem_info, NULL, &swapchain.render_finished[i]) != VK_SUCCESS) {
			Panic("Failed to create swapchain sync object %u!", i);
		}
		swapchain.in_flight[i] = VK_NULL_HANDLE;
	}
}

void App::create_command_buffers() {
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkCommandPoolCreateInfo pool_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		};
		
		pool_info.queueFamilyIndex = families.graphics;
		if (vkCreateCommandPool(device, &pool_info, NULL, &frames[i].graphics_pool) != VK_SUCCESS) {
			Panic("Failed to create graphics command pool %u!", i);
		}

		VkCommandBufferAllocateInfo alloc_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = frames[i].graphics_pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		if (vkAllocateCommandBuffers(device, &alloc_info, &frames[i].graphics_cmd) != VK_SUCCESS) {
			Panic("Failed to allocate graphics command buffer %u", i);
		}

		frames[i].compute_pool = VK_NULL_HANDLE;
		frames[i].transfer_pool = VK_NULL_HANDLE;
		frames[i].compute_cmd = VK_NULL_HANDLE;
		frames[i].transfer_cmd = VK_NULL_HANDLE;

		if (families.compute != families.graphics) {
			pool_info.queueFamilyIndex = families.compute;
			if (vkCreateCommandPool(device, &pool_info, NULL, &frames[i].compute_pool) != VK_SUCCESS) {
				Panic("Failed to create compute command pool %u!", i);
			}

			alloc_info.commandPool = frames[i].compute_pool;
			if (vkAllocateCommandBuffers(device, &alloc_info, &frames[i].compute_cmd) != VK_SUCCESS) {
				Panic("Failed to allocate compute command buffer %u", i);
			}
		}

		if (families.transfer != families.graphics &&
			families.transfer != families.compute) {
			pool_info.queueFamilyIndex = families.transfer;
			pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
         				    | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			if (vkCreateCommandPool(device, &pool_info, NULL, &frames[i].transfer_pool) != VK_SUCCESS) {
				Panic("Failed to create transfer command pool %u!", i);
			}

			alloc_info.commandPool = frames[i].transfer_pool;
			if (vkAllocateCommandBuffers(device, &alloc_info, &frames[i].transfer_cmd) != VK_SUCCESS) {
				Panic("Failed to allocate transfer command buffer %u", i);
			}
		}
	}
}

void App::create_graphics_pipeline() {
	VkPipelineLayoutCreateInfo graphics_layout = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	};

	if (vkCreatePipelineLayout(device, &graphics_layout, NULL, &pipeline.graphics_layout) != VK_SUCCESS) {
		Panic("Failed to create graphics pipeline layout!");
	}

	VkShaderModule tri_module = create_shader_module("shaders/tri.spv");

	VkPipelineShaderStageCreateInfo vsci{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = tri_module,
		.pName = "vert_main",
	};

	VkPipelineShaderStageCreateInfo fsci{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = tri_module,
		.pName = "frag_main",
	};

	VkPipelineShaderStageCreateInfo stages[] = { vsci, fsci };

	VkPipelineVertexInputStateCreateInfo vertex_input{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	};

	VkPipelineInputAssemblyStateCreateInfo input_assembly {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	VkPipelineTessellationStateCreateInfo tessellation{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
	};

	VkPipelineRasterizationStateCreateInfo raster{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f,
	};

	VkPipelineMultisampleStateCreateInfo multisample{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	VkPipelineDepthStencilStateCreateInfo depth_stencil{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_FALSE,
	};

	VkPipelineColorBlendAttachmentState color_attachment{
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};

	VkPipelineColorBlendStateCreateInfo color_blend{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.attachmentCount = 1,
		.pAttachments = &color_attachment,
	};

	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
		VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
	};

	VkPipelineDynamicStateCreateInfo dynamic_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = sizeof(dynamic_states) / sizeof(VkDynamicState),
		.pDynamicStates = dynamic_states,
	};

	VkPipelineRenderingCreateInfo rendering{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &swapchain.format,
	};

	VkGraphicsPipelineCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &rendering,
		.stageCount = 2,
		.pStages = stages,
		.pVertexInputState = &vertex_input,
		.pInputAssemblyState = &input_assembly,
		.pTessellationState = &tessellation,
		.pRasterizationState = &raster,
		.pMultisampleState = &multisample,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &color_blend,
		.pDynamicState = &dynamic_state,
		.layout = pipeline.graphics_layout,
	};

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &create_info, NULL, &pipeline.graphics) != VK_SUCCESS) {
		Panic("Failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(device, tri_module, NULL);
}

App::~App() {
	vkDeviceWaitIdle(device);
	vkDestroyPipeline(device, pipeline.graphics, NULL);
	vkDestroyPipelineLayout(device, pipeline.graphics_layout, NULL);

	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, frames[i].img_available, NULL);
		vkDestroyFence(device, frames[i].in_flight, NULL);
		vkDestroyCommandPool(device, frames[i].graphics_pool, NULL);
		if (frames[i].compute_pool != VK_NULL_HANDLE) vkDestroyCommandPool(device, frames[i].compute_pool, NULL);
		if (frames[i].transfer_pool != VK_NULL_HANDLE) vkDestroyCommandPool(device, frames[i].transfer_pool, NULL);
	}

	for (u32 i = 0; i < swapchain.image_count; i++) {
		vkDestroySemaphore(device, swapchain.render_finished[i], NULL);
		vkDestroyImageView(device, swapchain.image_views[i], NULL);
	}

	vkDestroySwapchainKHR(device, swapchain.swapchain, NULL);
	vkDestroyDevice(device, NULL);
	vkDestroySurfaceKHR(instance, surface_info.surface, NULL);
#if DEBUG
	auto destroy_dbg_messenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	destroy_dbg_messenger(instance, messenger, NULL);
#endif
	vkDestroyInstance(instance, NULL);
	glfwDestroyWindow(window);
	glfwTerminate();
}

void App::run() {
	arena.reset();

	while (!glfwWindowShouldClose(window)) {
		Log("Loop!");
		glfwPollEvents();
		draw_frame();
		arena.reset();
	}
}

VkShaderModule App::create_shader_module(const char* path) {
	auto code = read_file(path);

	VkShaderModuleCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code.size(),
		.pCode = reinterpret_cast<const u32*>(code.data()),
	};

	VkShaderModule res;
	if (vkCreateShaderModule(device, &create_info, NULL, &res) != VK_SUCCESS) {
		Panic("Failed to create shader module %s!", path);
	}

	return res;
}

void App::record_draw_buffer(VkCommandBuffer cmd, u32 image_index) {
	VkCommandBufferBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	vkBeginCommandBuffer(cmd, &begin_info);

	VkImageMemoryBarrier2 barrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
		.srcAccessMask = 0,
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.image = swapchain.images[image_index],
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	VkDependencyInfo dep{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier,
	};
	vkCmdPipelineBarrier2(cmd, &dep);

	VkRenderingAttachmentInfo attach_info{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = swapchain.image_views[image_index],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = { .color = { .float32 = { 0.0, 0.0, 0.0, 1.0 } } },
	};

	VkRenderingInfo render_info{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = {{0, 0}, swapchain.extent},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &attach_info,
	};

	vkCmdBeginRendering(cmd, &render_info);

	VkViewport viewport{
		0, 0,
		(f32)swapchain.extent.width,
		(f32)swapchain.extent.height,
		0, 1,
	};
	vkCmdSetViewportWithCount(cmd, 1, &viewport);

	VkRect2D scissor{
		{0, 0},
		swapchain.extent,
	};
	vkCmdSetScissorWithCount(cmd, 1, &scissor);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.graphics);
	vkCmdDraw(cmd, 3, 1, 0, 0);
	vkCmdEndRendering(cmd);

	barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
	barrier.dstAccessMask = 0;
	barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	vkCmdPipelineBarrier2(cmd, &dep);

	vkEndCommandBuffer(cmd);
}

void App::draw_frame() {
	vkWaitForFences(device, 1, &frames[current_frame].in_flight, VK_TRUE, UINT64_MAX);

	u32 image_index;
	VkAcquireNextImageInfoKHR acquire_info{
		.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
		.swapchain = swapchain.swapchain,
		.timeout = UINT64_MAX,
		.semaphore = frames[current_frame].img_available,
		.fence = VK_NULL_HANDLE,
		.deviceMask = 1,
	};
	vkAcquireNextImage2KHR(device, &acquire_info, &image_index);

	if (swapchain.in_flight[image_index] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &swapchain.in_flight[image_index], VK_TRUE, UINT64_MAX);
	}
	swapchain.in_flight[image_index] = frames[current_frame].in_flight;

	vkResetFences(device, 1, &frames[current_frame].in_flight);
	vkResetCommandPool(device, frames[current_frame].graphics_pool, 0);
	record_draw_buffer(frames[current_frame].graphics_cmd, image_index);

	VkSemaphoreSubmitInfo wait_sem_info{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = frames[current_frame].img_available,
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
	};

	VkCommandBufferSubmitInfo cmd_submit_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.commandBuffer = frames[current_frame].graphics_cmd,
	};

	VkSemaphoreSubmitInfo signal_sem_info{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = swapchain.render_finished[image_index],
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
	};

	VkSubmitInfo2 submit_info{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount = 1,
		.pWaitSemaphoreInfos = &wait_sem_info,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmd_submit_info,
		.signalSemaphoreInfoCount = 1,
		.pSignalSemaphoreInfos = &signal_sem_info,
	};
	vkQueueSubmit2(queues.graphics, 1, &submit_info, frames[current_frame].in_flight);

	VkPresentInfoKHR present_info{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &swapchain.render_finished[image_index],
		.swapchainCount = 1,
		.pSwapchains = &swapchain.swapchain,
		.pImageIndices = &image_index,
	};
	vkQueuePresentKHR(queues.present, &present_info);

	current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}
