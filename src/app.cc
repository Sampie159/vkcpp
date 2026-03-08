static constexpr u32 SCREEN_FACTOR = 80;
static constexpr u32 WIDTH = 16 * SCREEN_FACTOR;
static constexpr u32 HEIGHT = 9 * SCREEN_FACTOR;

#if DEBUG
static const char* TITLE = "Window! (Debug)";
#else
static const char* TITLE = "Window!";
#endif

static constexpr u32 INVALID_QUEUE = 0xFFFFFFFF;
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

	void run();

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
		VkQueue graphics = VK_NULL_HANDLE;
		VkQueue present = VK_NULL_HANDLE;
		VkQueue compute = VK_NULL_HANDLE;
		VkQueue transfer = VK_NULL_HANDLE;
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

	Arena arena{GB(1)};
};

static void error_callback(s32 code, const char* desc) {
	Panic("GLFW ERROR (%d): %s", code, desc);
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

	f32 queue_priority = 1;

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

	VkPhysicalDeviceVulkan13Features v13{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
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

App::~App() {
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, frames[i].img_available, NULL);
		vkDestroyFence(device, frames[i].in_flight, NULL);
		vkDestroyCommandPool(device, frames[i].graphics_pool, NULL);
		if (frames[i].compute_pool != VK_NULL_HANDLE) vkDestroyCommandPool(device, frames[i].compute_pool, NULL);
		if (frames[i].transfer_pool != VK_NULL_HANDLE) vkDestroyCommandPool(device, frames[i].transfer_pool, NULL);
	}

	for (u32 i = 0; i < swapchain.image_count; i++) {
		vkDestroySemaphore(device, swapchain.render_finished[i], NULL);
		vkDestroyFence(device, swapchain.in_flight[i], NULL);
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

	while (false && !glfwWindowShouldClose(window)) {
		glfwPollEvents();
		arena.reset();
	}
}
