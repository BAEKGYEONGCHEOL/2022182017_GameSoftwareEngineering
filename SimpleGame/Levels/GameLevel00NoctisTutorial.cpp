#include "stdafx.h"
#include "../Game.h"

#include <chrono>
#include <cmath>

namespace
{
float Length(float x, float y)
{
	return std::sqrt(x * x + y * y);
}
} // namespace
void Game::UpdateOnFoot(float deltaSeconds)
{
	float screenX = 0.0f;
	float screenY = 0.0f;
	if (m_Keys['a'] || m_Keys['A'])
		screenX -= 1.0f;
	if (m_Keys['d'] || m_Keys['D'])
		screenX += 1.0f;
	if (m_Keys['w'] || m_Keys['W'])
		screenY += 1.0f;
	if (m_Keys['s'] || m_Keys['S'])
		screenY -= 1.0f;

	float inputLength = Length(screenX, screenY);
	m_PlayerMoving = inputLength > 0.0f;
	if (inputLength > 0.0f)
	{
		screenX /= inputLength;
		screenY /= inputLength;
		WorldPoint direction = {screenX - screenY, -screenX - screenY};
		float worldLength = Length(direction.x, direction.y);
		direction.x /= worldLength;
		direction.y /= worldLength;
		m_LastMoveDirection = direction;

		if ((m_KeyPressed[' '] || m_DodgeTimer > 0.0f) && m_DodgeCooldown <= 0.0f)
		{
			m_DodgeTimer = 0.18f;
			m_DodgeCooldown = 0.75f;
		}
		float speed = m_DodgeTimer > 0.0f ? 9.5f : 4.2f;
		float nextX = m_Player.x + direction.x * speed * deltaSeconds;
		float nextY = m_Player.y + direction.y * speed * deltaSeconds;
		if (CanMoveTo(nextX, m_Player.y))
			m_Player.x = nextX;
		if (CanMoveTo(m_Player.x, nextY))
			m_Player.y = nextY;
	}

	if (m_Quest == FindCore && m_Player.x > 5.0f && m_Player.y > 5.0f)
	{
		for (size_t i = 0; i < m_Enemies.size(); ++i)
			m_Enemies[i].active = m_Enemies[i].health > 0;
	}

	if (m_MousePressed)
		HandleAttack();
	const std::chrono::steady_clock::time_point enemyStart = std::chrono::steady_clock::now();
	UpdateEnemies(deltaSeconds);
	const std::chrono::steady_clock::time_point survivorStart = std::chrono::steady_clock::now();
	UpdateLevelSurvivors(deltaSeconds);
	const std::chrono::steady_clock::time_point projectileStart = std::chrono::steady_clock::now();
	UpdateProjectiles(deltaSeconds);
	const std::chrono::steady_clock::time_point projectileEnd = std::chrono::steady_clock::now();
	m_EnemyAiTimeMs = std::chrono::duration<double, std::milli>(survivorStart - enemyStart).count();
	m_SurvivorAiTimeMs =
		std::chrono::duration<double, std::milli>(projectileStart - survivorStart).count();
	m_ProjectileCollisionTimeMs =
		std::chrono::duration<double, std::milli>(projectileEnd - projectileStart).count();
	if (m_KeyPressed['e'] || m_KeyPressed['E'])
		HandleInteraction();
	if ((m_KeyPressed['r'] || m_KeyPressed['R']) && m_Quest != InspectTerminal &&
		m_ReloadTimer <= 0.0f)
	{
		m_ReloadTimer = 0.8f;
		SetMessage(L"펄스 카빈을 충전하고 있습니다...", 1.0f);
	}
}

bool Game::CanMoveTo(float x, float y) const
{
	if (!IsTutorialFloor(x, y))
		return false;
	const float radius = 0.28f;
	for (size_t i = 0; i < m_Obstacles.size(); ++i)
	{
		const Obstacle& obstacle = m_Obstacles[i];
		if (x + radius > obstacle.x - obstacle.halfWidth &&
			x - radius < obstacle.x + obstacle.halfWidth &&
			y + radius > obstacle.y - obstacle.halfHeight &&
			y - radius < obstacle.y + obstacle.halfHeight)
			return false;
	}
	return true;
}

bool Game::IsTutorialFloor(float x, float y) const
{
	const float roomHalfSize = 8.0f;
	const float corridorHalfWidth = 1.75f;
	const float centers[] = {-13.0f, 13.0f};
	for (int xIndex = 0; xIndex < 2; ++xIndex)
	{
		for (int yIndex = 0; yIndex < 2; ++yIndex)
		{
			if (std::fabs(x - centers[xIndex]) <= roomHalfSize &&
				std::fabs(y - centers[yIndex]) <= roomHalfSize)
				return true;
		}
	}

	const bool horizontalCorridor = std::fabs(x) <= 5.0f &&
		(std::fabs(y + 13.0f) <= corridorHalfWidth ||
			std::fabs(y - 13.0f) <= corridorHalfWidth);
	const bool verticalCorridor = std::fabs(y) <= 5.0f &&
		(std::fabs(x + 13.0f) <= corridorHalfWidth ||
			std::fabs(x - 13.0f) <= corridorHalfWidth);
	const bool dockingBay =
		std::fabs(x + 28.0f) <= 4.5f && std::fabs(y + 28.0f) <= 4.5f;
	const bool dockingHorizontal =
		y >= -29.75f && y <= -26.25f && x >= -28.0f && x <= -20.0f;
	const bool dockingVertical =
		x >= -21.75f && x <= -18.25f && y >= -28.0f && y <= -13.0f;
	return horizontalCorridor || verticalCorridor || dockingBay || dockingHorizontal || dockingVertical;
}
