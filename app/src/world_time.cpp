#include "world_time.h"

#include <chrono>

namespace
{
	auto  lastUpdateTime{ std::chrono::steady_clock::now() };
	float elapsedSec{ .0f };
}

float WorldTime::GetElapsedSec()
{
	return elapsedSec;
}

void WorldTime::Tick()
{
	auto const currentTime{ std::chrono::steady_clock::now() };
	elapsedSec     = std::chrono::duration<float>(currentTime - lastUpdateTime).count();
	lastUpdateTime = currentTime;
}
