#include "world_time.h"

#include <chrono>

namespace
{
	auto  lastUpdateTime{ std::chrono::steady_clock::now() };
	float elapsedSec{ .0f };
}

float world_time::GetElapsedSec()
{
	return elapsedSec;
}

void world_time::Tick()
{
	auto const currentTime{ std::chrono::steady_clock::now() };
	elapsedSec     = std::chrono::duration<float>(currentTime - lastUpdateTime).count();
	lastUpdateTime = currentTime;
}
