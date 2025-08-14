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

FetchContent_Declare(
		assimp
		URL https://github.com/assimp/assimp/archive/refs/tags/v6.0.2.tar.gz)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF)
set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_BUILD_ASSIMP_VIEW OFF)
FetchContent_MakeAvailable(assimp)
list(APPEND EXTERNAL_LIBS assimp)

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

add_subdirectory(app)

set(SOURCE
    main.cpp)

add_executable(${PROJECT_NAME}
               ${SOURCE})

target_link_libraries(${PROJECT_NAME} PRIVATE App)
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(ASSIMP_DLL "${assimp_BINARY_DIR}/bin/assimp-vc143-mt.dll")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
	                   "${ASSIMP_DLL}"
	                   "${CMAKE_BINARY_DIR}/"
	                   )
endif ()

foreach (target IN LISTS EXTERNAL_LIBS)
	target_compile_options(${target} PRIVATE
	                       $<$<CXX_COMPILER_ID:MSVC>:/W0>
	                       $<$<CXX_COMPILER_ID:GNU>:-w>
	                       $<$<CXX_COMPILER_ID:Clang>:-w>)
endforeach ()