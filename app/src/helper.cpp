#include "helper.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

help::ImageData::~ImageData()
{
	stbi_image_free(Pixels);
}

help::ImageData help::LoadImage(std::string_view path)
{
	ImageData data{};

	data.Pixels = stbi_load(path.data(), &data.Width, &data.Height, &data.Channels, STBI_rgb_alpha);
	data.Size   = data.Width * data.Height * 4;

	if (!data.Pixels)
		throw std::runtime_error("failed to load image: " + std::string(path));

	return data;
}
