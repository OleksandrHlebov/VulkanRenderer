cmake_minimum_required(VERSION 3.31)
project(VulkanClasses)

set(CMAKE_CXX_STANDARD 20)

set(INCLUDE
    inc/vma_usage.h
    inc/deletion_queue.h
    inc/datatypes.h
    inc/Buffer.h
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
                           ${CMAKE_CURRENT_SOURCE_DIR}/inc
                           ${CMAKE_CURRENT_SOURCE_DIR}/src
                           ${stb_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PUBLIC
                      Vulkan::Vulkan
                      glfw
                      vk-bootstrap::vk-bootstrap
                      GPUOpen::VulkanMemoryAllocator)

