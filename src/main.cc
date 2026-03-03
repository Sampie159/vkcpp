#if OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif OS_UNIX
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <vector>
#include <sstream>
#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "defs.cc"

#include "utils.cc"
#include "arena.cc"
#include "shader.cc"
#include "vertex.cc"

#include "app.cc"

static void init_systems() {
	init_arena_globals();
}

int main() {
    init_systems();

	App app{};
	app.run();

	return 0;
}

