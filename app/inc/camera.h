#ifndef CAMERA_H
#define CAMERA_H
#include <algorithm>

#include "world_time.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

class Camera final
{
public:
	Camera() = delete;

	Camera(glm::vec3 const& position, float fov, float aspectRatio, float near, float far)
		: m_Position{ position }
		, m_Fov{ fov }
		, m_AspectRatio{ aspectRatio }
		, m_Near{ near }
		, m_Far{ far }
	{
		RecalculateProjection();
		m_TotalYaw   = glm::degrees(atan2(m_Forward.z, m_Forward.x));
		m_TotalPitch = glm::degrees(asin(m_Forward.y));
	}

	~Camera() = default;

	Camera(Camera&&)                 = delete;
	Camera(Camera const&)            = delete;
	Camera& operator=(Camera&&)      = delete;
	Camera& operator=(Camera const&) = delete;

	void Update(GLFWwindow* window)
	{
		double        xPos{};
		double        yPos{};
		static double cache_xPos{ 0.0 };
		static double cache_yPos{ 0.0 };
		glfwGetCursorPos(window, &xPos, &yPos);

		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
		{
			float const boost{ 1.f + static_cast<float>(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) * m_Boost };
			float const travelThisUpdate{ m_Speed * boost * WorldTime::GetElapsedSec() };
			if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
				m_Position += m_Forward * travelThisUpdate;
			if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
				m_Position -= m_Forward * travelThisUpdate;
			if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
				m_Position += WORLD_UP * travelThisUpdate;
			if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
				m_Position -= WORLD_UP * travelThisUpdate;
			if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			{
				glm::vec3 const right{ glm::cross(m_Forward, WORLD_UP) };
				m_Position += right * travelThisUpdate;
			}
			if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			{
				glm::vec3 const right{ glm::cross(m_Forward, WORLD_UP) };
				m_Position -= right * travelThisUpdate;
			}

			if (glm::vec2 const delta{ cache_xPos - xPos, cache_yPos - yPos };
				abs(delta.x) > FLT_EPSILON || abs(delta.y) > FLT_EPSILON)
			{
				m_TotalYaw -= delta.x * m_Sensitivity;
				m_TotalPitch += delta.y * m_Sensitivity;
				m_TotalPitch = std::clamp(m_TotalPitch, -89.f, 89.f);

				m_Forward.x = cos(glm::radians(m_TotalYaw)) * cos(glm::radians(m_TotalPitch));
				m_Forward.y = sin(glm::radians(m_TotalPitch));
				m_Forward.z = sin(glm::radians(m_TotalYaw)) * cos(glm::radians(m_TotalPitch));
			}
		}
		cache_xPos = xPos;
		cache_yPos = yPos;
	}

	[[nodiscard]] glm::mat4 CalculateViewMatrix() const
	{
		return glm::lookAt(m_Position, m_Position + m_Forward, WORLD_UP);
	}

	[[nodiscard]] glm::mat4 const& GetProjection() const
	{
		return m_Projection;
	}

private
:
	void RecalculateProjection()
	{
		m_Projection = glm::perspective(glm::radians(m_Fov), m_AspectRatio, m_Near, m_Far);
		m_Projection[1][1] *= -1;
	}

	static glm::vec3 constexpr WORLD_UP{ 0.0f, 1.0f, 0.0f };
	static glm::vec3 constexpr WORLD_FORWARD{ 0.0f, 0.0f, 1.0f };

	glm::mat4 m_Projection{};
	glm::vec3 m_Position;
	glm::vec3 m_Forward{ WORLD_FORWARD };
	float     m_Fov;
	float     m_AspectRatio;
	float     m_Speed{ 2.f };
	float     m_Boost{ 1.f };
	float     m_Sensitivity{ 0.2f };
	float     m_Near;
	float     m_Far;
	float     m_TotalYaw{ 0 };
	float     m_TotalPitch{ 0 };
};

#endif //CAMERA_H
