cmake_minimum_required(VERSION 3.31)
project(VulkanResearch)

set(CMAKE_CXX_STANDARD 20)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/W4>
                    $<$<CXX_COMPILER_ID:MSVC>:/WX>
                    $<$<CXX_COMPILER_ID:GNU>:-Wall>
                    $<$<CXX_COMPILER_ID:GNU>:-Wextra>
                    $<$<CXX_COMPILER_ID:GNU>:-Werror>
                    $<$<CXX_COMPILER_ID:Clang>:-Wall>
                    $<$<CXX_COMPILER_ID:Clang>:-Wextra>
                    $<$<CXX_COMPILER_ID:Clang>:-Werror>)

include(FetchContent)
FetchContent_Declare(
		glfw
		URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz)
set(GLFW_BUILD_DOCS OFF)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
		vkBootstrap
		URL https://github.com/charles-lunarg/vk-bootstrap/archive/refs/tags/v1.3.302.tar.gz)
FetchContent_MakeAvailable(vkBootstrap)

find_package(Vulkan REQUIRED)

add_subdirectory(vulkan-classes)
add_subdirectory(app)

set(SOURCE
    main.cpp)

add_executable(${PROJECT_NAME}
               ${SOURCE})

target_link_libraries(${PROJECT_NAME} PRIVATE App)