static constexpr u32 SCREEN_FACTOR = 80;
static constexpr u32 WIDTH = 16 * SCREEN_FACTOR;
static constexpr u32 HEIGHT = 9 * SCREEN_FACTOR;

#if DEBUG
static const char* TITLE = "Hello Window (Debug)";
#else
static const char* TITLE = "Hello Window";
#endif

#if DEBUG
static constexpr const char* validation_layers[] = {
	"VK_LAYER_KHRONOS_validation",
};
static constexpr u32 validation_layers_count = sizeof(validation_layers) / sizeof(const char*);
#endif

static constexpr const char* device_extensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
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
	void create_command_buffer();
	void create_sync_objects();

	void run();

	inline bool running() const { return !glfwWindowShouldClose(window); }
	void record_command_buffer(VkCommandBuffer buffer, u32 image_index);
	void draw_frame();

	GLFWwindow* window;
	VkInstance  instance;
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
	VkCommandBuffer command_buffer;
	VkSemaphore img_available_sem;
	VkSemaphore render_finished_sem;
	VkFence in_flight_fence;

	QueueFamilyIndices indices;
	SwapchainSupportDetails swapchain_support;

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
	create_command_buffer();
	create_sync_objects();
}

App::~App() {
	vkDestroySemaphore(device, img_available_sem, NULL);
	vkDestroySemaphore(device, render_finished_sem, NULL);
	vkDestroyFence(device, in_flight_fence, NULL);
	vkDestroyCommandPool(device, command_pool, NULL);
	vkDestroyPipeline(device, graphics_pipeline, NULL);
	vkDestroyPipelineLayout(device, pipeline_layout, NULL);
	
	for (u32 i = 0; i < image_count; i++) {
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

	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Vulkan App";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "No Engine";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_4;

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledExtensionCount = extension_count;
	create_info.ppEnabledExtensionNames = extensions;
#if DEBUG
	create_info.enabledLayerCount = validation_layers_count;
	create_info.ppEnabledLayerNames = validation_layers;

	VkDebugUtilsMessengerCreateInfoEXT msg_info{};
	msg_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	msg_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		                     | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	msg_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		                 | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	msg_info.pfnUserCallback = debug_callback;
	msg_info.pUserData = NULL;

	create_info.pNext = &msg_info;
#else
	create_info.enabledLayerCount = 0;
#endif

#if DEBUG
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
	VkDebugUtilsMessengerCreateInfoEXT msg_info{};
	msg_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	msg_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		                     | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	msg_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		                 | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	msg_info.pfnUserCallback = debug_callback;
	msg_info.pUserData = NULL;

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
		families[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
		families[i].pNext = NULL;
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
		qcis[i] = {};
		qcis[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qcis[i].queueFamilyIndex = unique_families[i];
		qcis[i].queueCount = 1;
		qcis[i].pQueuePriorities = &queue_priority;
	}

	VkPhysicalDeviceVulkan11Features supported_v11{};
	supported_v11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

	VkPhysicalDeviceVulkan12Features supported_v12{};
	supported_v12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	supported_v12.pNext = &supported_v11;

	VkPhysicalDeviceVulkan13Features supported_v13{};
	supported_v13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	supported_v13.pNext = &supported_v12;

	VkPhysicalDeviceVulkan14Features supported_v14{};
	supported_v14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
	supported_v14.pNext = &supported_v13;

	VkPhysicalDeviceFeatures2 supported_features{};
	supported_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	supported_features.pNext = &supported_v14;
	
	vkGetPhysicalDeviceFeatures2(physical_device, &supported_features);

	VkPhysicalDeviceVulkan13Features v13{};
	v13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	v13.dynamicRendering = supported_v13.dynamicRendering;
	v13.synchronization2 = VK_TRUE;

	VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extended{};
	extended.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
	extended.pNext = &v13;
	extended.extendedDynamicState = VK_TRUE;

	VkPhysicalDeviceFeatures2 enabled_features{};
	enabled_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	enabled_features.pNext = &extended;

	VkDeviceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pNext = &enabled_features;
	create_info.pQueueCreateInfos = qcis;
	create_info.queueCreateInfoCount = unique_families_count;
	create_info.enabledExtensionCount = device_extensions_count;
	create_info.ppEnabledExtensionNames = device_extensions;
#if DEBUG
	create_info.enabledLayerCount = validation_layers_count;
	create_info.ppEnabledLayerNames = validation_layers;
#else
	create_info.enabledLayerCount = 0;
#endif

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
	SwapchainSupportDetails details;
	details.capabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
	details.capabilities.pNext = NULL;
	vkGetPhysicalDeviceSurfaceCapabilities2KHR(device, &surface, &details.capabilities);

	vkGetPhysicalDeviceSurfaceFormats2KHR(device, &surface, &details.formats_count, NULL);
	if (details.formats_count > 0) {
		details.formats = arena.alloc<VkSurfaceFormat2KHR>(details.formats_count);
		for (u32 i = 0; i < details.formats_count; i++) {
			details.formats[i].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
			details.formats[i].pNext = NULL;
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
		u32(width),
		u32(height),
	};

	actual_extent.width = CLAMP(c.minImageExtent.width, c.maxImageExtent.width, actual_extent.width);
	actual_extent.height = CLAMP(c.minImageExtent.height, c.maxImageExtent.height, actual_extent.height);
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

	VkSwapchainCreateInfoKHR create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = surface.surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.surfaceFormat.format;
	create_info.imageColorSpace = surface_format.surfaceFormat.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

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

	create_info.preTransform = c.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = VK_NULL_HANDLE;

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
}

void App::create_image_views() {
	for (u32 i = 0; i < image_count; i++) {
		VkImageViewCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = swapchain_images[i];
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = swapchain_format;
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;

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

	VkPipelineShaderStageCreateInfo vssi{};
	vssi.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vssi.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vssi.module = vert_shader_module;
	vssi.pName = "main";

	VkPipelineShaderStageCreateInfo fssi{};
	fssi.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fssi.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fssi.module = frag_shader_module;
	fssi.pName = "main";

	VkPipelineShaderStageCreateInfo shader_stages[] = { vssi, fssi };

	VkPipelineVertexInputStateCreateInfo vii{};
	vii.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vii.vertexBindingDescriptionCount = 0;
	vii.pVertexBindingDescriptions = NULL;
	vii.vertexAttributeDescriptionCount = 0;
	vii.pVertexAttributeDescriptions = NULL;

	VkPipelineInputAssemblyStateCreateInfo input_assembly{};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
		VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
		VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
	};

	VkPipelineDynamicStateCreateInfo dynamic_state{};
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = sizeof(dynamic_states) / sizeof(VkDynamicState);
	dynamic_state.pDynamicStates = dynamic_states;

	VkPipelineViewportStateCreateInfo viewport_state{};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

	VkPipelineRasterizationStateCreateInfo raster{};
	raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster.depthClampEnable = VK_FALSE;
	raster.rasterizerDiscardEnable = VK_FALSE;
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.lineWidth = 1;
	raster.cullMode = VK_CULL_MODE_BACK_BIT;
	raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
	raster.depthBiasEnable = VK_FALSE;
	raster.depthBiasConstantFactor = 1;
	raster.depthBiasClamp = 0;
	raster.depthBiasSlopeFactor = 0;

	VkPipelineMultisampleStateCreateInfo multisample{};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.sampleShadingEnable = VK_FALSE;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample.minSampleShading = 1;
	multisample.pSampleMask = NULL;
	multisample.alphaToCoverageEnable = VK_FALSE;
	multisample.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState color_blend_attach{};
	color_blend_attach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
		                              | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attach.blendEnable = VK_FALSE;
	color_blend_attach.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attach.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attach.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attach.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo color_blend{};
	color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend.logicOpEnable = VK_FALSE;
	color_blend.logicOp = VK_LOGIC_OP_COPY;
	color_blend.attachmentCount = 1;
	color_blend.pAttachments = &color_blend_attach;
	color_blend.blendConstants[0] = 0;
	color_blend.blendConstants[1] = 0;
	color_blend.blendConstants[2] = 0;
	color_blend.blendConstants[3] = 0;

	VkPipelineLayoutCreateInfo pipeline_layout_info{};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 0;
	pipeline_layout_info.pSetLayouts = NULL;
	pipeline_layout_info.pushConstantRangeCount = 0;
	pipeline_layout_info.pPushConstantRanges = NULL;

	if (vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &pipeline_layout) != VK_SUCCESS) {
		Panic("Failed to create pipeline layout!");
	}

	VkPipelineRenderingCreateInfo render_info{};
	render_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	render_info.colorAttachmentCount = 1;
	render_info.pColorAttachmentFormats = &swapchain_format;
	render_info.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	render_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	VkGraphicsPipelineCreateInfo pipeline_info{};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.pNext = &render_info;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = shader_stages;
	pipeline_info.pVertexInputState = &vii;
	pipeline_info.pInputAssemblyState = &input_assembly;
	pipeline_info.pRasterizationState = &raster;
	pipeline_info.pMultisampleState = &multisample;
	pipeline_info.pDepthStencilState = NULL;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pColorBlendState = &color_blend;
	pipeline_info.pDynamicState = &dynamic_state;
	pipeline_info.layout = pipeline_layout;
	pipeline_info.renderPass = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &graphics_pipeline) != VK_SUCCESS) {
		Panic("Failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(device, vert_shader_module, NULL);
	vkDestroyShaderModule(device, frag_shader_module, NULL);
}

VkShaderModule App::create_shader_module(std::string_view code) {
	VkShaderModuleCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = code.size();
	create_info.pCode = (u32*)code.data();

	VkShaderModule shader_module;
	if (vkCreateShaderModule(device, &create_info, NULL, &shader_module) != VK_SUCCESS) {
		Panic("Failed to create shader module!");
	}

	return shader_module;
}

void App::create_command_pool() {
	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = indices.graphics_family;

	if (vkCreateCommandPool(device, &pool_info, NULL, &command_pool) != VK_SUCCESS) {
		Panic("Failed to create command pool!");
	}
}

void App::create_command_buffer() {
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(device, &alloc_info, &command_buffer) != VK_SUCCESS) {
		Panic("Failed to allocate command buffers!");
	}
}

void App::record_command_buffer(VkCommandBuffer buffer, u32 image_index) {
	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = 0;
	begin_info.pInheritanceInfo = NULL;

	if (vkBeginCommandBuffer(buffer, &begin_info) != VK_SUCCESS) {
		Panic("Failed to begin recording command buffer!");
	}

	VkRenderingAttachmentInfo color_attachment{};
	color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	color_attachment.imageView = swapchain_image_views[image_index];
	color_attachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.clearValue = { .color = { .float32 = { 0, 0, 0, 1 } } };

	VkRenderingInfo render_info{};
	render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	render_info.renderArea = {{0, 0}, swapchain_extent};
	render_info.layerCount = 1;
	render_info.colorAttachmentCount = 1;
	render_info.pColorAttachments = &color_attachment;

	VkViewport viewport{
		0, 0,
		f32(swapchain_extent.width),
		f32(swapchain_extent.height),
		0, 1
	};

	VkRect2D scissor{{0, 0}, swapchain_extent};

	VkImageMemoryBarrier2 barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	barrier.srcAccessMask = VK_ACCESS_2_NONE;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	barrier.image = swapchain_images[image_index];
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkDependencyInfo dep_info{};
	dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dep_info.imageMemoryBarrierCount = 1;
	dep_info.pImageMemoryBarriers = &barrier;

	vkCmdPipelineBarrier2(buffer, &dep_info);
	vkCmdBeginRendering(buffer, &render_info);
	vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
	vkCmdSetViewportWithCount(buffer, 1, &viewport);
	vkCmdSetScissorWithCount(buffer, 1, &scissor);
	vkCmdSetPrimitiveTopology(buffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	vkCmdDraw(buffer, 3, 1, 0, 0);
	vkCmdEndRendering(buffer);

	barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_NONE;
	barrier.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	vkCmdPipelineBarrier2(buffer, &dep_info);

	if (vkEndCommandBuffer(buffer) != VK_SUCCESS) {
		Panic("Failed to record command buffer!");
	}
}

void App::draw_frame() {
	vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &in_flight_fence);

	u32 image_index;
	vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, img_available_sem, VK_NULL_HANDLE, &image_index);

	vkResetCommandBuffer(command_buffer, 0);
	record_command_buffer(command_buffer, image_index);

	VkSemaphoreSubmitInfo wait_info{};
	wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	wait_info.semaphore = img_available_sem;
	wait_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSemaphoreSubmitInfo signal_info{};
	signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signal_info.semaphore = render_finished_sem;
	signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

	VkCommandBufferSubmitInfo cmd_info{};
	cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmd_info.commandBuffer = command_buffer;

	VkSubmitInfo2 submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit_info.waitSemaphoreInfoCount = 1;
	submit_info.pWaitSemaphoreInfos = &wait_info;
	submit_info.commandBufferInfoCount = 1;
	submit_info.pCommandBufferInfos = &cmd_info;
	submit_info.signalSemaphoreInfoCount = 1;
	submit_info.pSignalSemaphoreInfos = &signal_info;

	if (vkQueueSubmit2(graphics_queue, 1, &submit_info, in_flight_fence) != VK_SUCCESS) {
		Panic("Failed to submit queue!");
	}

	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &render_finished_sem;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swapchain;
	present_info.pImageIndices = &image_index;

	vkQueuePresentKHR(present_queue, &present_info);
}

void App::create_sync_objects() {
	VkSemaphoreCreateInfo sem_info{};
	sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fence_info{};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(device, &sem_info, NULL, &img_available_sem) != VK_SUCCESS ||
		vkCreateSemaphore(device, &sem_info, NULL, &render_finished_sem) != VK_SUCCESS ||
		vkCreateFence(device, &fence_info, NULL, &in_flight_fence) != VK_SUCCESS) {
		Panic("Failed to create synchronization objects!");
	}
}
