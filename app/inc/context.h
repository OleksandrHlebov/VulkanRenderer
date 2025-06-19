#ifndef CONTEXT_H
#define CONTEXT_H
#include "VkBootstrap.h"
#include "vulkan/vulkan.h"

#include "GLFW/glfw3.h"

struct Context
{
	GLFWwindow*                Window{};
	vkb::Instance              Instance;
	vkb::InstanceDispatchTable InstanceDispatchTable;
	VkSurfaceKHR               Surface{};
	vkb::Device                Device;
	vkb::DispatchTable         DispatchTable;
	vkb::Swapchain             Swapchain;

	std::vector<VkImage>     SwapchainImages;
	std::vector<VkImageView> SwapchainImageViews;

	VkQueue GraphicsQueue{};
	VkQueue PresentQueue{};
};

#endif //CONTEXT_H
