cmake_minimum_required(VERSION 3.31)
project(App)

find_program(GLSLANG glslang)

if (!GLSLANG)
	message(FATAL_ERROR "Shader compiler not found")
endif ()

#https://github.com/charles-lunarg/vk-bootstrap/blob/main/example/CMakeLists.txt
set(COMPILED_SHADERS)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)
macro(Shader_Compile name)
	get_filename_component(SHADER_FILENAME_WE ${name} NAME_WE)
	set(SHADER_SOURCE ${PROJECT_SOURCE_DIR}/shaders/${name})
	set(SHADER_SPIRV_PATH ${CMAKE_BINARY_DIR}/shaders/${SHADER_FILENAME_WE}.spv)

	add_custom_command(OUTPUT ${SHADER_SPIRV_PATH}
	                   COMMAND glslang -V --target-env vulkan1.3 ${SHADER_SOURCE} -o ${SHADER_SPIRV_PATH}
	                   DEPENDS ${SHADER_SOURCE}
	                   COMMENT "Compiled ${SHADER_SPIRV_PATH}")
	list(APPEND COMPILED_SHADERS ${SHADER_SPIRV_PATH})
endmacro()

FetchContent_Declare(
		VulkanClasses
		GIT_REPOSITORY https://github.com/OleksandrHlebov/vulkan-classes.git
		GIT_TAG master)
FetchContent_MakeAvailable(VulkanClasses)

set(SHADER_SOURCES
    "basic_color.frag"
    "basic_transform.vert"
    "alpha_discard.frag"
    "transform_w_normals.vert"
    "transform_to_lightspace.vert"
    "gbuffer_generation.frag"
    "lighting.frag"
    "quad.vert"
    "frag_depth_override.frag")

set(HEADER
    inc/helper.h
    inc/datatypes.h
    inc/camera.h
    inc/world_time.h
    inc/scene.h
    inc/mesh.h
    inc/HDRIRenderTarget.h)

set(SOURCE
    src/app.cpp
    src/helper.cpp
    src/world_time.cpp
    src/scene.cpp
    src/mesh.cpp
    src/HDRIRenderTarget.cpp)

add_library(App STATIC
            ${SOURCE}
            ${HEADER})

target_compile_definitions(${PROJECT_NAME} PRIVATE
                           GLM_FORCE_DEPTH_ZERO_TO_ONE
                           GLM_FORCE_RADIANS
                           GLM_ENABLE_EXPERIMENTAL)

target_link_libraries(${PROJECT_NAME} PUBLIC
                      VulkanClasses
                      glm::glm
                      assimp::assimp)

target_include_directories(${PROJECT_NAME} PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}/inc
                           ${CMAKE_CURRENT_SOURCE_DIR}/src
                           ${stb_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC
                           ${CMAKE_CURRENT_SOURCE_DIR})

foreach (shader IN LISTS SHADER_SOURCES)
	Shader_Compile(${shader})
endforeach ()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data
                   ${CMAKE_BINARY_DIR}/data)

add_custom_target(CompileShaders DEPENDS ${COMPILED_SHADERS})
add_dependencies(${PROJECT_NAME} CompileShaders)

set(CMAKE_CXX_STANDARD 20)