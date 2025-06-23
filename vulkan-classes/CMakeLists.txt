cmake_minimum_required(VERSION 3.31)
project(VulkanClasses)

set(CMAKE_CXX_STANDARD 20)

set(INCLUDE
    inc/vma_usage.h
    inc/image.h
    inc/datatypes.h
    inc/Buffer.h
    inc/image_view.h
    inc/pipeline_layout.h)

set(SOURCE
    src/main.cpp
    src/vma_usage.cpp
    src/image.cpp
    src/buffer.cpp
    src/image_view.cpp
    src/pipeline_layout.cpp)

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
                           ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(${PROJECT_NAME} PUBLIC
                      Vulkan::Vulkan
                      glfw
                      vk-bootstrap::vk-bootstrap
                      GPUOpen::VulkanMemoryAllocator)

