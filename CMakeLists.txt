cmake_minimum_required(VERSION 3.31)
project(VulkanResearch)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
set(EXTERNAL_LIBS)

FetchContent_Declare(
		glm
		URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz)
FetchContent_MakeAvailable(glm)
list(APPEND EXTERNAL_LIBS glm)

find_package(Vulkan REQUIRED)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/W4>
                    $<$<CXX_COMPILER_ID:MSVC>:/WX>
                    $<$<CXX_COMPILER_ID:GNU>:-Wall>
                    $<$<CXX_COMPILER_ID:GNU>:-Wextra>
                    $<$<CXX_COMPILER_ID:GNU>:-Werror>
                    $<$<CXX_COMPILER_ID:Clang>:-Wall>
                    $<$<CXX_COMPILER_ID:Clang>:-Wextra>
                    $<$<CXX_COMPILER_ID:Clang>:-Werror>)

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