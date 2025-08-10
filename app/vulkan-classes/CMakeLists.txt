cmake_minimum_required(VERSION 3.31)
project(VulkanClasses)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
include(FetchContent)

set(EXTERNAL_LIBS)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
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

set(INCLUDE
    inc/vma_usage.h
    inc/deletion_queue.h
    inc/buffer.h
    inc/image.h
    inc/image_view.h
    inc/pipeline_layout.h
    inc/pipeline.h
    inc/descriptor_set_layout.h
    inc/descriptor_pool.h
    inc/shader_stage.h
    inc/command_pool.h
    inc/command_buffer.h)

set(SOURCE
    src/main.cpp
    src/vma_usage.cpp
    src/buffer.cpp
    src/image.cpp
    src/image_view.cpp
    src/pipeline_layout.cpp
    src/pipeline.cpp
    src/descriptor_set_layout.cpp
    src/descriptor_pool.cpp
    inc/descriptor_set.h
    src/descriptor_set.cpp
    src/command_pool.cpp
    src/command_buffer.cpp)

add_library(VulkanClasses STATIC
            ${SOURCE}
            ${INCLUDE})

if (MSVC)
	set_source_files_properties(src/vma_usage.cpp PROPERTIES COMPILE_FLAGS /W0)
else ()
	set_source_files_properties(src/vma_usage.cpp PROPERTIES COMPILE_FLAGS -w)
endif ()

target_include_directories(${PROJECT_NAME} PUBLIC
                           ${CMAKE_CURRENT_SOURCE_DIR}/inc)

target_link_libraries(${PROJECT_NAME} PUBLIC
                      Vulkan::Vulkan
                      glfw
                      vk-bootstrap::vk-bootstrap
                      GPUOpen::VulkanMemoryAllocator)

foreach (target IN LISTS EXTERNAL_LIBS)
	target_compile_options(${target} PRIVATE
	                       $<$<CXX_COMPILER_ID:MSVC>:/W0>
	                       $<$<CXX_COMPILER_ID:GNU>:-w>
	                       $<$<CXX_COMPILER_ID:Clang>:-w>)
endforeach ()

