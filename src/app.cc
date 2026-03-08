static constexpr u32 SCREEN_FACTOR = 80;
static constexpr u32 WIDTH = 16 * SCREEN_FACTOR;
static constexpr u32 HEIGHT = 9 * SCREEN_FACTOR;

#if DEBUG
static const char* TITLE = "Window! (Debug)";
#else
static const char* TITLE = "Window!";
#endif

static constexpr u32 INVALID_QUEUE = 0xFFFFFFFF;

struct App {
	App();
	~App();

	void init_glfw();
	void create_instance();
	void create_surface();
	void pick_device();
	void create_device();

	void run();

	GLFWwindow* window;
	VkInstance instance;
	VkPhysicalDeviceSurfaceInfo2KHR surface_info;
	VkDebugUtilsMessengerEXT messenger;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device;

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
		u32 count = 0;
	} families;

	Arena arena{GB(1)};
};

static void error_callback(s32 code, const char* desc) {
	Panic("GLFW ERROR (%d): %s", code, desc);
}

static VkBool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
							   VkDebugUtilsMessageTypeFlagsEXT type,
							   const VkDebugUtilsMessengerCallbackDataEXT* data,
							   void* user_data)
{
	switch (severity) {
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		Log("(%d) %s:\n%s", data->messageIdNumber, data->pMessageIdName, data->pMessage);
		break;
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

App::App() {
	init_glfw();
	create_instance();
	create_surface();
	pick_device();
	create_device();
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
	const char** final_extensions = arena.alloc<const char*>(extensions_count + 1);
	for (u32 i = 0; i < extensions_count; i++) {
		final_extensions[i] = extensions[i];
	}

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

	final_extensions[extensions_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	extensions_count += 1;

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
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	func(instance, &dbg_info, NULL, &messenger);
#endif
}

void App::create_surface() {
	surface_info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
	surface_info.pNext = NULL;

	if (glfwCreateWindowSurface(instance, window, NULL, &surface_info.surface) != VK_SUCCESS) {
		Panic("Failed to create window surface!");
	}
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

			if (graphics && present) {
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

	const char* extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	VkDeviceCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = NULL,
		.queueCreateInfoCount = queue_info_count,
		.pQueueCreateInfos = queue_infos,
		.enabledExtensionCount = 1,
		.ppEnabledExtensionNames = extensions,
	};

	if (vkCreateDevice(physical_device, &create_info, NULL, &device) != VK_SUCCESS) {
		Panic("Failed to create logical device!");
	}

	vkGetDeviceQueue(device, families.graphics, 0, &queues.graphics);
	vkGetDeviceQueue(device, families.present, 0, &queues.present);
	vkGetDeviceQueue(device, families.compute, 0, &queues.compute);
	vkGetDeviceQueue(device, families.transfer, 0, &queues.transfer);
}

App::~App() {
	vkDestroyDevice(device, NULL);
	vkDestroySurfaceKHR(instance, surface_info.surface, NULL);
#if DEBUG
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	func(instance, messenger, NULL);
#endif
	vkDestroyInstance(instance, NULL);
	glfwTerminate();
}

void App::run() {
	arena.reset();

	while (false && !glfwWindowShouldClose(window)) {
		glfwPollEvents();
		arena.reset();
	}
}
