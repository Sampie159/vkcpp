static constexpr u32 SCREEN_FACTOR = 80;
static constexpr u32 WIDTH = 16 * SCREEN_FACTOR;
static constexpr u32 HEIGHT = 9 * SCREEN_FACTOR;

#if DEBUG
static const char* TITLE = "Hello Window (Debug)";
#else
static const char* TITLE = "Hello Window";
#endif

static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

#if DEBUG
static constexpr const char* validation_layers[] = {
	"VK_LAYER_KHRONOS_validation",
};
static constexpr u32 validation_layers_count = sizeof(validation_layers) / sizeof(const char*);
#endif

static constexpr const char* device_extensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
static constexpr u32 device_extensions_count = sizeof(device_extensions) / sizeof(const char*);

struct QueueFamilyIndices {
	u32 graphics_family = MAX_U32;
	u32 present_family = MAX_U32;

	inline bool complete() const {
		return graphics_family != MAX_U32 && present_family != MAX_U32;
	}
};

struct SwapchainSupportDetails {
	VkSurfaceCapabilities2KHR capabilities;
	VkSurfaceFormat2KHR* formats;
	VkPresentModeKHR* present_modes;

	u32 formats_count;
	u32 present_modes_count;
};

struct App {
	App();
	~App();

	void init_glfw();
	void create_instance();
	void pick_physical_device();
	void create_logical_device();
	void create_swapchain();
	void create_image_views();
	void create_graphics_pipeline();
	void create_command_pool();
	void create_command_buffers();
	void create_sync_objects();

	void run();

	inline bool running() const { return !glfwWindowShouldClose(window); }
	void record_command_buffer(VkCommandBuffer buffer, u32 image_index);
	void draw_frame();

	GLFWwindow* window;
	VkInstance instance;
	VkPhysicalDeviceSurfaceInfo2KHR surface;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device;
	VkQueue graphics_queue;
	VkQueue present_queue;
	VkSwapchainKHR swapchain;
	VkImage* swapchain_images;
	VkImageView* swapchain_image_views;
	u32 image_count;
	VkFormat swapchain_format;
	VkExtent2D swapchain_extent;
	VkPipelineLayout pipeline_layout;
	VkPipeline graphics_pipeline;
	VkCommandPool command_pool;
	VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore img_available_sem[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore* render_finished_sems;
	VkFence in_flight_fence[MAX_FRAMES_IN_FLIGHT];
	VkImageLayout* swapchain_layouts;

	QueueFamilyIndices indices;
	SwapchainSupportDetails swapchain_support;
	u32 current_frame = 0;

#if DEBUG
	bool check_validation_layer_support();
	void setup_vk_debugger();
	VkDebugUtilsMessengerEXT messenger;
#endif

	bool is_device_suitable(VkPhysicalDevice device);
	QueueFamilyIndices find_queue_families(VkPhysicalDevice device);
	bool check_device_extension_support(VkPhysicalDevice device);
	SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice);
	VkSurfaceFormat2KHR choose_swap_surface_format(const VkSurfaceFormat2KHR* formats, u32 formats_count);
	VkPresentModeKHR choose_swap_present_mode(const VkPresentModeKHR* modes, u32 modes_count);
	VkExtent2D choose_swap_extent(const VkSurfaceCapabilities2KHR& capabilities);
	VkShaderModule create_shader_module(std::string_view code);

	Arena arena{GB(1)};
};

App::App() {
	init_glfw();
	create_instance();
#if DEBUG
	setup_vk_debugger();
#endif
	pick_physical_device();
	create_logical_device();
	create_swapchain();
	create_image_views();
	create_graphics_pipeline();
	create_command_pool();
	create_command_buffers();
	create_sync_objects();
}

App::~App() {
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, img_available_sem[i], NULL);
		vkDestroyFence(device, in_flight_fence[i], NULL);
	}
	vkDestroyCommandPool(device, command_pool, NULL);
	vkDestroyPipeline(device, graphics_pipeline, NULL);
	vkDestroyPipelineLayout(device, pipeline_layout, NULL);
	
	for (u32 i = 0; i < image_count; i++) {
		vkDestroySemaphore(device, render_finished_sems[i], NULL);
		vkDestroyImageView(device, swapchain_image_views[i], NULL);
	}
	
	vkDestroySwapchainKHR(device, swapchain, NULL);
	vkDestroyDevice(device, NULL);
#if DEBUG
	auto destroy_debug_messenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (destroy_debug_messenger) {
		destroy_debug_messenger(instance, messenger, NULL);
	}
#endif
	vkDestroySurfaceKHR(instance, surface.surface, NULL);
	vkDestroyInstance(instance, NULL);
	glfwDestroyWindow(window);
	glfwTerminate();
}

static void error_callback(s32 code, const char* desc) {
	Panic("GLFW ERROR (%d): %s", code, desc);
}

static void key_callback(GLFWwindow* window, s32 key, s32 scancode, s32 action, s32 mods) {
	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}

void App::init_glfw() {
	glfwSetErrorCallback(error_callback);
	if (glfwInit() != GLFW_TRUE) {
		Panic("Failed to initialize GLFW!");
	}

	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(WIDTH, HEIGHT, TITLE, NULL, NULL);
	if (!window) {
		Panic("Failed to create GLFW Window!");
	}
	glfwSetKeyCallback(window, key_callback);
}

#if DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT* data,
	void* user_data
) {
	switch (severity) {
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		Error("Validation Layer: %s", data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		Warn("Validation Layer: %s", data->pMessage);
		break;
	default:
		Log("Validation Layer: %s", data->pMessage);
	}

	return VK_FALSE;
}
#endif

void App::create_instance() {
	u32 extension_count = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
	VkExtensionProperties* ext_props = arena.alloc<VkExtensionProperties>(extension_count);
	vkEnumerateInstanceExtensionProperties(NULL, &extension_count, ext_props);

	const char** extensions = arena.alloc<const char*>(extension_count);
	for (u32 i = 0; i < extension_count; i++) {
		extensions[i] = ext_props[i].extensionName;
	}

#if DEBUG
	if (!check_validation_layer_support()) {
		Panic("Validation layers requested are not available!");
	}

	arena.alloc<const char*>();
	extensions[extension_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	extension_count += 1;

	Log("Extension Count: %u", extension_count);
	Log("Extensions:");
	for (u32 i = 0; i < extension_count; i++) {
		Log("\t%s", extensions[i]);
	}
#endif

	VkApplicationInfo app_info{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Vulkan App",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "No Engine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_4,
	};

	VkInstanceCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = 0,
		.enabledExtensionCount = extension_count,
		.ppEnabledExtensionNames = extensions,
	};

#if DEBUG
	create_info.enabledLayerCount = validation_layers_count;
	create_info.ppEnabledLayerNames = validation_layers;

	VkDebugUtilsMessengerCreateInfoEXT msg_info{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		                 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		             | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = debug_callback,
		.pUserData = NULL,
	};

	create_info.pNext = &msg_info;

	check_validation_layer_support();
#endif

	if (vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS) {
		Panic("Failed to create Vulkan Instance!");
	}

	surface.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
	surface.pNext = NULL;

	if (glfwCreateWindowSurface(instance, window, NULL, &surface.surface) != VK_SUCCESS) {
		Panic("Failed to create window surface!");
	}
}

#if DEBUG
bool App::check_validation_layer_support() {
	u8* mark = arena.mark();

	u32 layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, NULL);
	VkLayerProperties* layers = arena.alloc<VkLayerProperties>(layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, layers);

	bool layer_found = false;
	for (u32 i = 0; i < validation_layers_count; i++) {
		for (u32 j = 0; j < layer_count; j++) {
			if (strcmp(validation_layers[i], layers[j].layerName) == 0) {
				layer_found = true;
				break;
			}
		}
	}

	arena.reset(mark);
	return layer_found;
}

void App::setup_vk_debugger() {
	VkDebugUtilsMessengerCreateInfoEXT msg_info{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		                 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		             | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = debug_callback,
		.pUserData = NULL,
	};

	auto create_debug_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (create_debug_messenger) {
		if (create_debug_messenger(instance, &msg_info, NULL, &messenger) != VK_SUCCESS) {
			Panic("Failed to create debug messenger!");
		}
	} else {
		Panic("Failed to load debug extension!");
	}
}
#endif

void App::run() {
	arena.reset();

	while (running()) {
		glfwPollEvents();
		draw_frame();
	}

	vkDeviceWaitIdle(device);
}

void App::pick_physical_device() {
	u32 device_count;
	vkEnumeratePhysicalDevices(instance, &device_count, NULL);
	VkPhysicalDevice* devices = arena.alloc<VkPhysicalDevice>(device_count);
	vkEnumeratePhysicalDevices(instance, &device_count, devices);

	for (u32 i = 0; i < device_count; i++) {
		if (is_device_suitable(devices[i])) {
			physical_device = devices[i];
			break;
		}
	}

	if (physical_device == VK_NULL_HANDLE) {
		Panic("Failed to find suitable GPU!");
	}
}

bool App::is_device_suitable(VkPhysicalDevice device) {
	QueueFamilyIndices family_indices = find_queue_families(device);

	bool res = family_indices.complete();
	if (res) {
		indices = family_indices;
	}

	bool extensions_supported = check_device_extension_support(device);
	bool swapchain_adequate = false;
	if (extensions_supported) {
		SwapchainSupportDetails support = query_swapchain_support(device);
		swapchain_adequate = support.formats_count > 0 && support.present_modes_count > 0;
		if (swapchain_adequate) {
			swapchain_support = support;
		}
	}

	return res && swapchain_adequate;
}

QueueFamilyIndices App::find_queue_families(VkPhysicalDevice device) {
	QueueFamilyIndices family_indices;

	u32 queue_family_count;
	vkGetPhysicalDeviceQueueFamilyProperties2(device, &queue_family_count, NULL);
	VkQueueFamilyProperties2* families = arena.alloc<VkQueueFamilyProperties2>(queue_family_count);
	for (u32 i = 0; i < queue_family_count; i++) {
		families[i] = {
			.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,
			.pNext = NULL,
		};
	}
	vkGetPhysicalDeviceQueueFamilyProperties2(device, &queue_family_count, families);

	for (u32 i = 0; i < queue_family_count; i++) {
		if (families[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			family_indices.graphics_family = i;
		}

		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface.surface, &present_support);
		if (present_support) {
			family_indices.present_family = i;
		}

		if (family_indices.complete()) {
			break;
		}
	}

	return family_indices;
}

void App::create_logical_device() {
	f32 queue_priority = 1;

	u32 unique_families_count = 1;
	u32* unique_families = arena.alloc<u32>(2);
	unique_families[0] = indices.graphics_family;

	if (indices.graphics_family != indices.present_family) {
		unique_families_count = 2;
		unique_families[1] = indices.present_family;
	}

	auto qcis = arena.alloc<VkDeviceQueueCreateInfo>(unique_families_count);
	for (u32 i = 0; i < unique_families_count; i++) {
		qcis[i] = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = unique_families[i],
			.queueCount = 1,
			.pQueuePriorities = &queue_priority,
		};
	}

	VkPhysicalDeviceVulkan13Features v13{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.synchronization2 = VK_TRUE,
		.dynamicRendering = VK_TRUE,
	};	

	VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extended{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
		.pNext = &v13,
		.extendedDynamicState = VK_TRUE,
	};

	VkPhysicalDeviceFeatures2 enabled_features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &extended,
	};

	VkDeviceCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &enabled_features,
		.queueCreateInfoCount = unique_families_count,
		.pQueueCreateInfos = qcis,
#if DEBUG
		.enabledLayerCount = validation_layers_count,
		.ppEnabledLayerNames = validation_layers,
#else
		.enabledLayerCount = 0,
#endif
		.enabledExtensionCount = device_extensions_count,
		.ppEnabledExtensionNames = device_extensions,
	};

	if (vkCreateDevice(physical_device, &create_info, NULL, &device) != VK_SUCCESS) {
		Panic("Failed to create logical device!");
	}

	vkGetDeviceQueue(device, indices.graphics_family, 0, &graphics_queue);
	vkGetDeviceQueue(device, indices.present_family, 0, &present_queue);
}

bool App::check_device_extension_support(VkPhysicalDevice device) {
	u32 extension_count;
	vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
	VkExtensionProperties* available_extensions = arena.alloc<VkExtensionProperties>(extension_count);
	vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);

	u32 required_count = device_extensions_count;
	for (u32 i = 0; i < extension_count; i++) {
		for (u32 j = 0; j < device_extensions_count; j++) {
			if (strcmp(available_extensions[i].extensionName, device_extensions[j]) == 0) {
				required_count -= 1;
			}

			if (required_count == 0) {
				break;
			}
		}
	}

	return required_count == 0;
}

SwapchainSupportDetails App::query_swapchain_support(VkPhysicalDevice device) {
	SwapchainSupportDetails details{
		.capabilities = {
			.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
			.pNext = NULL,
		},
	};
	vkGetPhysicalDeviceSurfaceCapabilities2KHR(device, &surface, &details.capabilities);

	vkGetPhysicalDeviceSurfaceFormats2KHR(device, &surface, &details.formats_count, NULL);
	if (details.formats_count > 0) {
		details.formats = arena.alloc<VkSurfaceFormat2KHR>(details.formats_count);
		for (u32 i = 0; i < details.formats_count; i++) {
			details.formats[i] = {
				.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR,
				.pNext = NULL,
			};
		}
		vkGetPhysicalDeviceSurfaceFormats2KHR(device, &surface, &details.formats_count, details.formats);
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface.surface, &details.present_modes_count, NULL);
	details.present_modes = arena.alloc<VkPresentModeKHR>(details.present_modes_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface.surface, &details.present_modes_count, details.present_modes);

	return details;
}

VkSurfaceFormat2KHR App::choose_swap_surface_format(const VkSurfaceFormat2KHR* formats, u32 format_count) {
	for (u32 i = 0; i < format_count; i++) {
		const VkSurfaceFormatKHR& format = formats[i].surfaceFormat;
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return formats[i];
		}
	}

	return formats[0];
}

VkPresentModeKHR App::choose_swap_present_mode(const VkPresentModeKHR* modes, u32 modes_count) {
	for (u32 i = 0; i < modes_count; i++) {
		if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return modes[i];
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D App::choose_swap_extent(const VkSurfaceCapabilities2KHR& capabilities) {
	const VkSurfaceCapabilitiesKHR& c = capabilities.surfaceCapabilities;
	if (c.currentExtent.width != MAX_U32) {
		return c.currentExtent;
	}

	s32 width, height;
	glfwGetFramebufferSize(window, &width, &height);

	VkExtent2D actual_extent = {
		.width = CLAMP(c.minImageExtent.width, c.maxImageExtent.width, u32(width)),
		.height = CLAMP(c.minImageExtent.height, c.maxImageExtent.height, u32(height)),
	};

	return actual_extent;
}

void App::create_swapchain() {
	VkSurfaceFormat2KHR surface_format = choose_swap_surface_format(swapchain_support.formats, swapchain_support.formats_count);
	VkPresentModeKHR present_mode = choose_swap_present_mode(swapchain_support.present_modes, swapchain_support.present_modes_count);
	VkExtent2D extent = choose_swap_extent(swapchain_support.capabilities);

	const VkSurfaceCapabilitiesKHR& c = swapchain_support.capabilities.surfaceCapabilities;
	u32 image_count = c.minImageCount + 1;

	if (c.maxImageCount > 0) {
		image_count = MIN(image_count, c.maxImageCount);
	}

	VkSwapchainCreateInfoKHR create_info{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface.surface,
		.minImageCount = image_count,
		.imageFormat = surface_format.surfaceFormat.format,
		.imageColorSpace = surface_format.surfaceFormat.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = c.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = present_mode,
		.clipped = VK_TRUE,
		.oldSwapchain = VK_NULL_HANDLE,
	};

	u32 family_indices[] = {indices.graphics_family, indices.present_family};
	if (indices.graphics_family != indices.present_family) {
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = family_indices;
	} else {
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices = NULL;
	}

	if (vkCreateSwapchainKHR(device, &create_info, NULL, &swapchain) != VK_SUCCESS) {
		Panic("Failed to create swapchain!");
	}

	vkGetSwapchainImagesKHR(device, swapchain, &image_count, NULL);
	this->image_count = image_count;
	swapchain_images = (VkImage*)malloc(sizeof(VkImage) * image_count);
	swapchain_image_views = (VkImageView*)malloc(sizeof(VkImageView) * image_count);
	vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images);

	swapchain_format = surface_format.surfaceFormat.format;
	swapchain_extent = extent;
	swapchain_layouts = (VkImageLayout*)malloc(sizeof(VkImageLayout) * image_count);
	for (u32 i = 0; i < image_count; i++) {
		swapchain_layouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;
	}
}

void App::create_image_views() {
	for (u32 i = 0; i < image_count; i++) {
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = swapchain_format,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		if (vkCreateImageView(device, &create_info, NULL, &swapchain_image_views[i]) != VK_SUCCESS) {
			Panic("Failed to create image view!");
		}
	}
}

void App::create_graphics_pipeline() {
	auto vert_shader_code = read_file("shaders/tri.vert.spv");
	auto frag_shader_code = read_file("shaders/tri.frag.spv");

	VkShaderModule vert_shader_module = create_shader_module(vert_shader_code);
	VkShaderModule frag_shader_module = create_shader_module(frag_shader_code);

	VkPipelineShaderStageCreateInfo vssi{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = vert_shader_module,
		.pName = "main",
	};

	VkPipelineShaderStageCreateInfo fssi{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = frag_shader_module,
		.pName = "main",
	};

	VkPipelineShaderStageCreateInfo shader_stages[] = { vssi, fssi };

	VkPipelineVertexInputStateCreateInfo vii{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = NULL,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = NULL,
	};

	VkPipelineInputAssemblyStateCreateInfo input_assembly{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
		VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
		VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
		VK_DYNAMIC_STATE_CULL_MODE,
		VK_DYNAMIC_STATE_FRONT_FACE,
	};

	VkPipelineDynamicStateCreateInfo dynamic_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = sizeof(dynamic_states) / sizeof(VkDynamicState),
		.pDynamicStates = dynamic_states,
	};

	VkPipelineViewportStateCreateInfo viewport_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
	};

	VkPipelineRasterizationStateCreateInfo raster{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 1,
		.depthBiasClamp = 0,
		.depthBiasSlopeFactor = 0,
		.lineWidth = 1,
	};

	VkPipelineMultisampleStateCreateInfo multisample{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 1,
		.pSampleMask = NULL,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};

	VkPipelineColorBlendAttachmentState color_blend_attach{
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
		                | VK_COLOR_COMPONENT_A_BIT,
	};

	VkPipelineColorBlendStateCreateInfo color_blend{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &color_blend_attach,
		.blendConstants = { 0, 0, 0, 0 },
	};

	VkPipelineLayoutCreateInfo pipeline_layout_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 0,
		.pSetLayouts = NULL,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = NULL,
	};

	if (vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &pipeline_layout) != VK_SUCCESS) {
		Panic("Failed to create pipeline layout!");
	}

	VkPipelineRenderingCreateInfo render_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &swapchain_format,
		.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	};

	VkGraphicsPipelineCreateInfo pipeline_info{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &render_info,
		.stageCount = 2,
		.pStages = shader_stages,
		.pVertexInputState = &vii,
		.pInputAssemblyState = &input_assembly,
		.pViewportState = &viewport_state,
		.pRasterizationState = &raster,
		.pMultisampleState = &multisample,
		.pDepthStencilState = NULL,
		.pColorBlendState = &color_blend,
		.pDynamicState = &dynamic_state,
		.layout = pipeline_layout,
		.renderPass = VK_NULL_HANDLE,
	};

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &graphics_pipeline) != VK_SUCCESS) {
		Panic("Failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(device, vert_shader_module, NULL);
	vkDestroyShaderModule(device, frag_shader_module, NULL);
}

VkShaderModule App::create_shader_module(std::string_view code) {
	VkShaderModuleCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code.size(),
		.pCode = (u32*)code.data(),
	};

	VkShaderModule shader_module;
	if (vkCreateShaderModule(device, &create_info, NULL, &shader_module) != VK_SUCCESS) {
		Panic("Failed to create shader module!");
	}

	return shader_module;
}

void App::create_command_pool() {
	VkCommandPoolCreateInfo pool_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = indices.graphics_family,
	};

	if (vkCreateCommandPool(device, &pool_info, NULL, &command_pool) != VK_SUCCESS) {
		Panic("Failed to create command pool!");
	}
}

void App::create_command_buffers() {
	VkCommandBufferAllocateInfo alloc_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = MAX_FRAMES_IN_FLIGHT,
	};

	if (vkAllocateCommandBuffers(device, &alloc_info, command_buffers) != VK_SUCCESS) {
		Panic("Failed to allocate command buffers!");
	}
}

void App::record_command_buffer(VkCommandBuffer buffer, u32 image_index) {
	VkCommandBufferBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = 0,
		.pInheritanceInfo = NULL,
	};

	if (vkBeginCommandBuffer(buffer, &begin_info) != VK_SUCCESS) {
		Panic("Failed to begin recording command buffer!");
	}

	VkRenderingAttachmentInfo color_attachment{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = swapchain_image_views[image_index],
		.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = { .color = { .float32 = { 0, 0, 0, 1 } } },
	};

	VkRenderingInfo render_info{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = {{0, 0}, swapchain_extent},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment,
	};

	VkViewport viewport{
		0, 0,
		f32(swapchain_extent.width),
		f32(swapchain_extent.height),
		0, 1
	};

	VkRect2D scissor{{0, 0}, swapchain_extent};

	VkImageMemoryBarrier2 barrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.oldLayout = swapchain_layouts[image_index],
		.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.image = swapchain_images[image_index],
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
	swapchain_layouts[image_index] = barrier.newLayout;

	VkDependencyInfo dep_info{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier,
	};

	vkCmdPipelineBarrier2(buffer, &dep_info);
	vkCmdBeginRendering(buffer, &render_info);
	vkCmdSetViewportWithCount(buffer, 1, &viewport);
	vkCmdSetScissorWithCount(buffer, 1, &scissor);
	vkCmdSetPrimitiveTopology(buffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	vkCmdSetCullMode(buffer, VK_CULL_MODE_BACK_BIT);
	vkCmdSetFrontFace(buffer, VK_FRONT_FACE_CLOCKWISE);
	vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
	vkCmdDraw(buffer, 3, 1, 0, 0);
	vkCmdEndRendering(buffer);

	barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
	barrier.dstAccessMask = VK_ACCESS_2_NONE;
	barrier.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	swapchain_layouts[image_index] = barrier.newLayout;

	vkCmdPipelineBarrier2(buffer, &dep_info);

	if (vkEndCommandBuffer(buffer) != VK_SUCCESS) {
		Panic("Failed to record command buffer!");
	}
}

void App::draw_frame() {
	vkWaitForFences(device, 1, &in_flight_fence[current_frame], VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &in_flight_fence[current_frame]);

	VkAcquireNextImageInfoKHR acquire_info{
		.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
		.swapchain = swapchain,
		.timeout = UINT64_MAX,
		.semaphore = img_available_sem[current_frame],
		.fence = VK_NULL_HANDLE,
		.deviceMask = 1,
	};

	u32 image_index;
	vkAcquireNextImage2KHR(device, &acquire_info, &image_index);

	vkResetCommandBuffer(command_buffers[current_frame], 0);
	record_command_buffer(command_buffers[current_frame], image_index);

	VkSemaphoreSubmitInfo wait_info{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = img_available_sem[current_frame],
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
	};

	VkSemaphoreSubmitInfo signal_info{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = render_finished_sems[image_index],
		.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
	};

	VkCommandBufferSubmitInfo cmd_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.commandBuffer = command_buffers[current_frame],
		.deviceMask = 1,
	};

	VkSubmitInfo2 submit_info{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount = 1,
		.pWaitSemaphoreInfos = &wait_info,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmd_info,
		.signalSemaphoreInfoCount = 1,
		.pSignalSemaphoreInfos = &signal_info,
	};

	if (vkQueueSubmit2(graphics_queue, 1, &submit_info, in_flight_fence[current_frame]) != VK_SUCCESS) {
		Panic("Failed to submit queue!");
	}

	VkPresentInfoKHR present_info{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &render_finished_sems[image_index],
		.swapchainCount = 1,
		.pSwapchains = &swapchain,
		.pImageIndices = &image_index,
	};

	vkQueuePresentKHR(present_queue, &present_info);
	current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
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
		if (vkCreateSemaphore(device, &sem_info, NULL, &img_available_sem[i]) != VK_SUCCESS ||
			vkCreateFence(device, &fence_info, NULL, &in_flight_fence[i]) != VK_SUCCESS) {
			Panic("Failed to create synchronization objects!");
		}
	}

	render_finished_sems = (VkSemaphore*)malloc(sizeof(VkSemaphore) * image_count);
	for (u32 i = 0; i < image_count; i++) {
		if (vkCreateSemaphore(device, &sem_info, NULL, &render_finished_sems[i]) != VK_SUCCESS) {
			Panic("Failed to create synchronization objects!");
		}
	}
}
