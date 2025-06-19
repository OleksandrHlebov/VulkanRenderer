cmake_minimum_required(VERSION 3.31)
project(VulkanClasses)

set(CMAKE_CXX_STANDARD 20)

set(INCLUDE)

set(SOURCE
    src/main.cpp)

add_library(VulkanClasses STATIC
            ${SOURCE}
            ${INCLUDE})

target_include_directories(${PROJECT_NAME} PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}/inc
                           ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(${PROJECT_NAME} PUBLIC
                           ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PUBLIC
                      Vulkan::Vulkan
                      glfw
                      vk-bootstrap::vk-bootstrap)