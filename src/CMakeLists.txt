add_executable(
    playground
    main.cc
)

find_package(Vulkan REQUIRED)

target_link_libraries(
    playground
    EnTT::EnTT
    glm::glm
    glfw
    Vulkan::Vulkan
)

if (WIN32)
    target_compile_definitions(
        playground
        PRIVATE
        OS_WINDOWS=1
    )
elseif (UNIX)
    target_compile_definitions(
        playground
        PRIVATE
        OS_UNIX=1
    )
endif ()
