cmake_minimum_required(VERSION 3.31)
project(VulkanResearch)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)
set(EXTERNAL_LIBS)
FetchContent_Declare(
		glfw
		URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz)
set(GLFW_BUILD_DOCS OFF)
FetchContent_MakeAvailable(glfw)
list(APPEND EXTERNAL_LIBS glfw)

FetchContent_Declare(
		vk-bootstrap
		URL https://github.com/charles-lunarg/vk-bootstrap/archive/refs/tags/v1.3.302.tar.gz)
FetchContent_MakeAvailable(vk-bootstrap)
list(APPEND EXTERNAL_LIBS vk-bootstrap)

FetchContent_Declare(
		vma
		URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.3.0.tar.gz)
set(VMA_BUILD_SAMPLES ON)
FetchContent_MakeAvailable(vma)
#list(APPEND EXTERNAL_LIBS VulkanMemoryAllocator)

FetchContent_Declare(
		glm
		URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz)
FetchContent_MakeAvailable(glm)
list(APPEND EXTERNAL_LIBS glm)

FetchContent_Declare(
		assimp
		URL https://github.com/assimp/assimp/archive/refs/tags/v6.0.2.tar.gz)
set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_BUILD_ASSIMP_VIEW OFF)
FetchContent_MakeAvailable(assimp)
list(APPEND EXTERNAL_LIBS glm)

FetchContent_Declare(
		stb
		GIT_REPOSITORY https://github.com/nothings/stb.git
		GIT_TAG master)
FetchContent_MakeAvailable(stb)

find_package(Vulkan REQUIRED)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/W4>
                    $<$<CXX_COMPILER_ID:MSVC>:/WX>
                    $<$<CXX_COMPILER_ID:GNU>:-Wall>
                    $<$<CXX_COMPILER_ID:GNU>:-Wextra>
                    $<$<CXX_COMPILER_ID:GNU>:-Werror>
                    $<$<CXX_COMPILER_ID:Clang>:-Wall>
                    $<$<CXX_COMPILER_ID:Clang>:-Wextra>
                    $<$<CXX_COMPILER_ID:Clang>:-Werror>)

add_subdirectory(vulkan-classes)
add_subdirectory(app)

set(SOURCE
    main.cpp)

add_executable(${PROJECT_NAME}
               ${SOURCE})

target_link_libraries(${PROJECT_NAME} PRIVATE App)

foreach (target IN LISTS EXTERNAL_LIBS)
	target_compile_options(${target} PRIVATE
	                       $<$<CXX_COMPILER_ID:MSVC>:/W0>
	                       $<$<CXX_COMPILER_ID:GNU>:-w>
	                       $<$<CXX_COMPILER_ID:Clang>:-w>)
endforeach ()