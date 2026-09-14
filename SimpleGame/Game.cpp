#include "stdafx.h"
#include "Game.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include "Dependencies\\freeglut.h"

namespace
{
	const float Pi = 3.1415926535f;
	const float TileWidth = 64.0f;
	const float TileHeight = 32.0f;
	const WorldPoint TerminalPosition = { -9.0f, -8.0f };
	const WorldPoint RecordPosition = { -4.0f, -4.5f };
	const WorldPoint SignalPosition = { 0.0f, 3.0f };
	const WorldPoint CorePosition = { 8.5f, 8.5f };
	const WorldPoint CockpitPosition = { -9.0f, 8.5f };
	const WorldPoint DestinationPosition = { 0.0f, 650.0f };

	float Clamp(float value, float minimum, float maximum)
	{
		return value < minimum ? minimum : (value > maximum ? maximum : value);
	}

	float Length(float x, float y)
	{
		return std::sqrt(x * x + y * y);
	}

	ScreenPoint Rotate(float x, float y, float angle, float centerX, float centerY)
	{
		const float cosine = std::cos(angle);
		const float sine = std::sin(angle);
		ScreenPoint point = { centerX + x * cosine - y * sine, centerY + x * sine + y * cosine };
		return point;
	}

	void DrawRotatedRect(PrototypeRenderer* renderer, float x, float y, float width, float height,
		float angle, float r, float g, float b, float alpha)
	{
		ScreenPoint p0 = Rotate(-width * 0.5f, -height * 0.5f, angle, x, y);
		ScreenPoint p1 = Rotate(width * 0.5f, -height * 0.5f, angle, x, y);
		ScreenPoint p2 = Rotate(width * 0.5f, height * 0.5f, angle, x, y);
		ScreenPoint p3 = Rotate(-width * 0.5f, height * 0.5f, angle, x, y);
		renderer->DrawQuad(p0, p1, p2, p3, r, g, b, alpha);
	}
}

Game::Game(PrototypeRenderer* renderer)
	: m_Renderer(renderer)
{
	Reset();
}

void Game::Reset()
{
	m_Mode = OnFoot;
	m_Quest = InspectTerminal;
	m_Paused = false;
	m_CoreRecovered = false;
	m_RecordRead = false;
	m_SignalChecked = false;
	m_MedicalDroneUsed = false;
	std::memset(m_Keys, 0, sizeof(m_Keys));
	std::memset(m_KeyPressed, 0, sizeof(m_KeyPressed));
	std::memset(m_SpecialKeys, 0, sizeof(m_SpecialKeys));
	m_MousePressed = false;
	m_MouseX = m_Renderer->Width() / 2;
	m_MouseY = m_Renderer->Height() / 2;
	m_Player = { -9.5f, -9.0f };
	m_LastMoveDirection = { 1.0f, 0.0f };
	m_PlayerHealth = 100.0f;
	m_AttackCooldown = 0.0f;
	m_AttackFlash = 0.0f;
	m_DodgeCooldown = 0.0f;
	m_DodgeTimer = 0.0f;
	m_DamageCooldown = 0.0f;
	m_TotalTime = 0.0f;
	m_PlayerMoving = false;
	m_WeaponEnergy = 100.0f;
	m_ReloadTimer = 0.0f;
	m_Projectiles.clear();
	m_ShipPosition = { 0.0f, 0.0f };
	m_ShipVelocity = { 0.0f, 0.0f };
	m_ShipAngle = Pi * 0.5f;
	m_ShipHealth = 100.0f;
	m_ShipImpactFlash = 0.0f;
	m_TransitionTimer = 0.0f;
	m_CurrentShip = 0;
	m_TravelDestinationShip = 1;
	m_Message = L"WASD: 이동    E: 상호작용";
	m_MessageTimer = 6.0f;

	m_Obstacles.clear();
	m_Obstacles.push_back({ -7.1f, -7.2f, 0.65f, 0.55f, 42.0f, 0.16f, 0.25f, 0.30f, MedicalPod });
	m_Obstacles.push_back({ -4.6f, -6.4f, 0.75f, 0.50f, 48.0f, 0.20f, 0.25f, 0.28f, ControlConsole });
	m_Obstacles.push_back({ -1.8f, -3.6f, 0.55f, 0.85f, 52.0f, 0.19f, 0.23f, 0.28f, CoolantPipe });
	m_Obstacles.push_back({ 1.4f, -1.6f, 0.80f, 0.55f, 38.0f, 0.24f, 0.20f, 0.25f, CargoCrate });
	m_Obstacles.push_back({ 4.1f, 0.2f, 0.65f, 0.85f, 54.0f, 0.13f, 0.23f, 0.29f, PowerRelay });
	m_Obstacles.push_back({ 6.1f, 3.1f, 0.80f, 0.60f, 44.0f, 0.19f, 0.25f, 0.29f, CargoCrate });
	m_Obstacles.push_back({ 7.0f, 6.8f, 0.60f, 0.55f, 38.0f, 0.25f, 0.18f, 0.27f, PowerRelay });
	m_Obstacles.push_back({ 3.2f, 7.1f, 0.60f, 0.80f, 50.0f, 0.14f, 0.21f, 0.27f, EngineModule });
	m_Obstacles.push_back({ -0.7f, 6.2f, 0.75f, 0.50f, 44.0f, 0.18f, 0.24f, 0.29f, ControlConsole });
	m_Obstacles.push_back({ -4.2f, 5.8f, 0.55f, 0.85f, 49.0f, 0.14f, 0.23f, 0.30f, CoolantPipe });
	m_Obstacles.push_back({ -7.1f, 4.0f, 0.80f, 0.55f, 45.0f, 0.17f, 0.25f, 0.31f, CargoCrate });
	m_Obstacles.push_back({ -2.6f, 1.0f, 0.65f, 0.65f, 40.0f, 0.21f, 0.22f, 0.27f, MedicalPod });

	m_Enemies.clear();
	m_Enemies.push_back({ { 9.2f, 7.2f }, 2, false });
	m_Enemies.push_back({ { 7.3f, 9.4f }, 2, false });
	m_Enemies.push_back({ { 9.7f, 9.2f }, 2, false });
	m_Enemies.push_back({ { 6.8f, 7.8f }, 2, false });

	m_NeutralNpcs.clear();
	m_NeutralNpcs.push_back({ { -6.5f, -3.0f }, true, 0.0f, -1, false });
	m_NeutralNpcs.push_back({ { 2.8f, 2.4f }, true, 1.7f, -1, false });
	m_NeutralNpcs.push_back({ { -7.2f, 6.1f }, false, 0.8f, 0, false });
	m_NeutralNpcs.push_back({ { 4.8f, -5.8f }, false, 2.4f, 1, false });

	m_Asteroids.clear();
	m_Asteroids.push_back({ { -135.0f, 125.0f }, { 31.0f, -8.0f }, 25.0f, 0.2f, 0.45f });
	m_Asteroids.push_back({ { 150.0f, 235.0f }, { -38.0f, -3.0f }, 31.0f, 1.1f, -0.32f });
	m_Asteroids.push_back({ { -175.0f, 355.0f }, { 43.0f, -11.0f }, 22.0f, 2.2f, 0.60f });
	m_Asteroids.push_back({ { 145.0f, 455.0f }, { -34.0f, -7.0f }, 28.0f, 0.7f, -0.48f });
	m_Asteroids.push_back({ { -120.0f, 565.0f }, { 29.0f, -13.0f }, 20.0f, 1.8f, 0.72f });
}

void Game::Resize(int width, int height)
{
	m_Renderer->Resize(width, height);
}

void Game::KeyDown(unsigned char key)
{
	if (!m_Keys[key]) m_KeyPressed[key] = true;
	m_Keys[key] = true;
}

void Game::KeyUp(unsigned char key)
{
	m_Keys[key] = false;
}

void Game::SpecialDown(int key)
{
	if (key >= 0 && key < 512) m_SpecialKeys[key] = true;
}

void Game::SpecialUp(int key)
{
	if (key >= 0 && key < 512) m_SpecialKeys[key] = false;
}

void Game::MouseMove(int x, int y)
{
	m_MouseX = x;
	m_MouseY = y;
}

void Game::MouseButton(int button, int state)
{
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) m_MousePressed = true;
}

void Game::Update(float deltaSeconds)
{
	deltaSeconds = Clamp(deltaSeconds, 0.0f, 0.05f);
	if (m_KeyPressed[27]) m_Paused = !m_Paused;
	if (m_Mode == Complete && (m_KeyPressed['r'] || m_KeyPressed['R']))
	{
		Reset();
		return;
	}

	if (!m_Paused)
	{
		m_TotalTime += deltaSeconds;
		m_AttackCooldown = std::max(0.0f, m_AttackCooldown - deltaSeconds);
		m_AttackFlash = std::max(0.0f, m_AttackFlash - deltaSeconds);
		m_DodgeCooldown = std::max(0.0f, m_DodgeCooldown - deltaSeconds);
		m_DodgeTimer = std::max(0.0f, m_DodgeTimer - deltaSeconds);
		m_DamageCooldown = std::max(0.0f, m_DamageCooldown - deltaSeconds);
		m_ShipImpactFlash = std::max(0.0f, m_ShipImpactFlash - deltaSeconds);
		m_MessageTimer = std::max(0.0f, m_MessageTimer - deltaSeconds);
		if (m_ReloadTimer > 0.0f)
		{
			m_ReloadTimer -= deltaSeconds;
			if (m_ReloadTimer <= 0.0f)
			{
				m_WeaponEnergy = 100.0f;
				SetMessage(L"펄스 카빈 충전 완료.", 1.2f);
			}
		}

		if (m_Mode == OnFoot) UpdateOnFoot(deltaSeconds);
		else if (m_Mode == ShipControl) UpdateShip(deltaSeconds);
		else if (m_Mode == Transition)
		{
			m_TransitionTimer += deltaSeconds;
			if (m_TransitionTimer >= 1.4f)
			{
				m_CurrentShip = m_TravelDestinationShip;
				m_Mode = OnFoot;
				m_Player = { -9.0f, 8.0f };
				m_ShipPosition = { 0.0f, 0.0f };
				m_ShipVelocity = { 0.0f, 0.0f };
				if (m_CurrentShip == 1) m_Quest = QuestComplete;
				SetMessage(m_CurrentShip == 1 ? L"난민 수송선 에레보스에 도킹했습니다." : L"연구선 노크티스로 귀환했습니다.", 3.0f);
			}
		}
	}

	std::memset(m_KeyPressed, 0, sizeof(m_KeyPressed));
	m_MousePressed = false;
}

void Game::UpdateOnFoot(float deltaSeconds)
{
	float screenX = 0.0f;
	float screenY = 0.0f;
	if (m_Keys['a'] || m_Keys['A']) screenX -= 1.0f;
	if (m_Keys['d'] || m_Keys['D']) screenX += 1.0f;
	if (m_Keys['w'] || m_Keys['W']) screenY += 1.0f;
	if (m_Keys['s'] || m_Keys['S']) screenY -= 1.0f;

	float inputLength = Length(screenX, screenY);
	m_PlayerMoving = inputLength > 0.0f;
	if (inputLength > 0.0f)
	{
		screenX /= inputLength;
		screenY /= inputLength;
		WorldPoint direction = { screenX - screenY, -screenX - screenY };
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
		if (CanMoveTo(nextX, m_Player.y)) m_Player.x = nextX;
		if (CanMoveTo(m_Player.x, nextY)) m_Player.y = nextY;
	}

	if (m_Quest == FindCore && m_Player.x > 5.0f && m_Player.y > 5.0f)
	{
		for (size_t i = 0; i < m_Enemies.size(); ++i) m_Enemies[i].active = m_Enemies[i].health > 0;
	}

	if (m_MousePressed) HandleAttack();
	UpdateEnemies(deltaSeconds);
	UpdateProjectiles(deltaSeconds);
	if (m_KeyPressed['e'] || m_KeyPressed['E']) HandleInteraction();
	if ((m_KeyPressed['r'] || m_KeyPressed['R']) && m_Quest != InspectTerminal && m_ReloadTimer <= 0.0f)
	{
		m_ReloadTimer = 0.8f;
		SetMessage(L"펄스 카빈을 충전하고 있습니다...", 1.0f);
	}
}

void Game::UpdateEnemies(float deltaSeconds)
{
	for (size_t i = 0; i < m_Enemies.size(); ++i)
	{
		Enemy& enemy = m_Enemies[i];
		if (!enemy.active || enemy.health <= 0) continue;
		float dx = m_Player.x - enemy.position.x;
		float dy = m_Player.y - enemy.position.y;
		float distance = Length(dx, dy);
		if (distance > 0.65f)
		{
			enemy.position.x += dx / distance * 1.15f * deltaSeconds;
			enemy.position.y += dy / distance * 1.15f * deltaSeconds;
		}
		else if (m_DamageCooldown <= 0.0f && m_DodgeTimer <= 0.0f)
		{
			m_PlayerHealth -= 18.0f;
			m_DamageCooldown = 0.8f;
			SetMessage(L"공허종과 접촉했습니다. 계속 움직이십시오.", 1.4f);
		}
	}

	if (m_PlayerHealth <= 25.0f && !m_MedicalDroneUsed)
	{
		m_MedicalDroneUsed = true;
		m_PlayerHealth = 70.0f;
		SetMessage(L"응급 의료 드론이 생체 신호를 회복했습니다.", 2.2f);
	}
	if (m_PlayerHealth <= 0.0f)
	{
		m_Player = { 5.0f, 5.0f };
		m_PlayerHealth = 100.0f;
		for (size_t i = 0; i < m_Enemies.size(); ++i)
		{
			m_Enemies[i].health = 2;
			m_Enemies[i].active = true;
		}
		SetMessage(L"의료 백업이 마지막 지점을 복구했습니다.", 2.5f);
	}
}

void Game::HandleAttack()
{
	if (m_Quest == InspectTerminal || m_AttackCooldown > 0.0f || m_ReloadTimer > 0.0f) return;
	if (m_WeaponEnergy < 10.0f)
	{
		SetMessage(L"무기 에너지가 부족합니다. R 키로 충전하십시오.", 1.5f);
		return;
	}
	m_WeaponEnergy -= 10.0f;
	m_AttackCooldown = 0.22f;
	m_AttackFlash = 0.10f;
	float aimX = static_cast<float>(m_MouseX - m_Renderer->Width() / 2);
	float aimY = static_cast<float>(m_Renderer->Height() / 2 - m_MouseY + 30);
	float aimLength = Length(aimX, aimY);
	if (aimLength < 1.0f) { aimX = 1.0f; aimY = 0.0f; aimLength = 1.0f; }
	aimX /= aimLength;
	aimY /= aimLength;

	WorldPoint worldDirection = { aimX / 64.0f - aimY / 32.0f, -aimX / 64.0f - aimY / 32.0f };
	float worldLength = Length(worldDirection.x, worldDirection.y);
	if (worldLength > 0.0f)
	{
		worldDirection.x /= worldLength;
		worldDirection.y /= worldLength;
	}
	m_Projectiles.push_back({ m_Player, { worldDirection.x * 11.0f, worldDirection.y * 11.0f }, 1.1f });
}

void Game::UpdateProjectiles(float deltaSeconds)
{
	for (size_t projectileIndex = 0; projectileIndex < m_Projectiles.size();)
	{
		Projectile& projectile = m_Projectiles[projectileIndex];
		projectile.position.x += projectile.velocity.x * deltaSeconds;
		projectile.position.y += projectile.velocity.y * deltaSeconds;
		projectile.life -= deltaSeconds;
		bool remove = projectile.life <= 0.0f;
		for (size_t enemyIndex = 0; enemyIndex < m_Enemies.size() && !remove; ++enemyIndex)
		{
			Enemy& enemy = m_Enemies[enemyIndex];
			if (enemy.active && enemy.health > 0 && Length(projectile.position.x - enemy.position.x, projectile.position.y - enemy.position.y) < 0.55f)
			{
				enemy.health -= 1;
				remove = true;
				if (enemy.health <= 0) enemy.active = false;
				if (AllEnemiesDefeated()) SetMessage(L"구역이 확보되었습니다. 항법 코어를 회수하십시오.", 2.5f);
			}
		}
		if (remove) m_Projectiles.erase(m_Projectiles.begin() + projectileIndex);
		else ++projectileIndex;
	}
}

void Game::HandleInteraction()
{
	for (size_t i = 0; i < m_NeutralNpcs.size(); ++i)
	{
		NeutralNpc& npc = m_NeutralNpcs[i];
		if (npc.dead || !IsNear(npc.position, 1.25f)) continue;
		if (npc.dialogueId == 0)
		{
			SetMessage(npc.talked
				? L"통신장교 서윤: 공허종의 신호에도 구조 요청이 섞여 있었어요. 지휘부는 그 부분을 지웠습니다."
				: L"통신장교 서윤: 우리가 먼저 그 문을 열었어요... 그런데 모두 적이 먼저 공격했다고 믿고 있어요.", 5.0f);
		}
		else
		{
			SetMessage(npc.talked
				? L"정비사 라울: 살아남으면 진실을 전해 주세요. 복수만으로는 이 함선을 다시 움직일 수 없습니다."
				: L"정비사 라울: 냉각관이 터질 때 동료들을 두고 도망쳤습니다... 아직도 그 소리가 들립니다.", 5.0f);
		}
		npc.talked = true;
		return;
	}
	if (m_Quest == InspectTerminal && IsNear(TerminalPosition, 1.15f))
	{
		m_Quest = FindCore;
		SetMessage(L"함선 AI: 기관실에서 항법 코어가 감지됩니다. 무기를 활성화합니다.", 3.2f);
		return;
	}
	if (m_CurrentShip == 0 && !m_RecordRead && IsNear(RecordPosition, 1.05f))
	{
		m_RecordRead = true;
		SetMessage(L"기록: 공식 접촉 보고서와 봉인된 실험 자료가 서로 일치하지 않습니다.", 3.5f);
		return;
	}
	if (m_CurrentShip == 0 && m_CoreRecovered && !m_SignalChecked && IsNear(SignalPosition, 1.05f))
	{
		m_SignalChecked = true;
		SetMessage(L"구조 신호가 사망한 승무원의 목소리를 반복하고 있습니다.", 3.2f);
		return;
	}
	if (m_Quest == FindCore && AllEnemiesDefeated() && IsNear(CorePosition, 1.2f))
	{
		m_CoreRecovered = true;
		m_Quest = ReturnToCockpit;
		SetMessage(L"항법 코어를 회수했습니다. 비상 전력이 복구됩니다.", 3.0f);
		return;
	}
	if (m_Quest == ReturnToCockpit && IsNear(CockpitPosition, 1.25f))
	{
		m_Mode = ShipControl;
		m_Quest = FlyToSignal;
		m_TravelDestinationShip = 1;
		SetMessage(L"방향키: 회전/추진    Space: 가속    E: 신호 진입", 5.0f);
		return;
	}
	if (m_Quest == QuestComplete && IsNear(CockpitPosition, 1.25f))
	{
		m_TravelDestinationShip = 1 - m_CurrentShip;
		m_Mode = ShipControl;
		SetMessage(m_TravelDestinationShip == 0 ? L"항로: 연구선 노크티스" : L"항로: 난민 수송선 에레보스", 2.5f);
	}
}

void Game::UpdateShip(float deltaSeconds)
{
	if (m_SpecialKeys[GLUT_KEY_LEFT]) m_ShipAngle += 2.1f * deltaSeconds;
	if (m_SpecialKeys[GLUT_KEY_RIGHT]) m_ShipAngle -= 2.1f * deltaSeconds;
	float thrust = 0.0f;
	if (m_SpecialKeys[GLUT_KEY_UP]) thrust += 95.0f;
	if (m_SpecialKeys[GLUT_KEY_DOWN]) thrust -= 48.0f;
	if (m_Keys[' ']) thrust *= 1.8f;
	m_ShipVelocity.x += std::cos(m_ShipAngle) * thrust * deltaSeconds;
	m_ShipVelocity.y += std::sin(m_ShipAngle) * thrust * deltaSeconds;
	float speed = Length(m_ShipVelocity.x, m_ShipVelocity.y);
	if (speed > 180.0f)
	{
		m_ShipVelocity.x *= 180.0f / speed;
		m_ShipVelocity.y *= 180.0f / speed;
	}
	m_ShipVelocity.x *= std::pow(0.38f, deltaSeconds);
	m_ShipVelocity.y *= std::pow(0.38f, deltaSeconds);
	m_ShipPosition.x += m_ShipVelocity.x * deltaSeconds;
	m_ShipPosition.y += m_ShipVelocity.y * deltaSeconds;
	UpdateAsteroids(deltaSeconds);

	const WorldPoint debris[] = { { -90.0f, 170.0f }, { 85.0f, 290.0f }, { -70.0f, 410.0f }, { 55.0f, 520.0f } };
	for (int i = 0; i < 4; ++i)
	{
		float distance = Length(m_ShipPosition.x - debris[i].x, m_ShipPosition.y - debris[i].y);
		if (distance < 34.0f && m_DamageCooldown <= 0.0f)
		{
			m_ShipVelocity.x *= -0.4f;
			m_ShipVelocity.y *= -0.4f;
			m_ShipHealth = std::max(20.0f, m_ShipHealth - 15.0f);
			m_DamageCooldown = 0.8f;
			SetMessage(L"선체가 충돌했습니다. 항로를 수정하십시오.", 1.5f);
		}
	}
	float destinationDistance = Length(m_ShipPosition.x - DestinationPosition.x,
		m_ShipPosition.y - DestinationPosition.y);
	if ((m_KeyPressed['e'] || m_KeyPressed['E']) && destinationDistance < 85.0f)
	{
		m_Mode = Transition;
		m_TransitionTimer = 0.0f;
	}
}

void Game::UpdateAsteroids(float deltaSeconds)
{
	for (size_t i = 0; i < m_Asteroids.size(); ++i)
	{
		Asteroid& asteroid = m_Asteroids[i];
		asteroid.position.x += asteroid.velocity.x * deltaSeconds;
		asteroid.position.y += asteroid.velocity.y * deltaSeconds;
		asteroid.rotation += asteroid.rotationSpeed * deltaSeconds;

		if (asteroid.position.x < -210.0f || asteroid.position.x > 210.0f)
			asteroid.velocity.x *= -1.0f;
		if (asteroid.position.y < 45.0f) asteroid.position.y += 560.0f;
		if (asteroid.position.y > 625.0f) asteroid.position.y -= 560.0f;

		float dx = m_ShipPosition.x - asteroid.position.x;
		float dy = m_ShipPosition.y - asteroid.position.y;
		float distance = Length(dx, dy);
		if (distance < asteroid.radius + 22.0f && m_DamageCooldown <= 0.0f)
		{
			float normalX = distance > 0.01f ? dx / distance : 1.0f;
			float normalY = distance > 0.01f ? dy / distance : 0.0f;
			m_ShipPosition.x += normalX * (asteroid.radius + 22.0f - distance);
			m_ShipPosition.y += normalY * (asteroid.radius + 22.0f - distance);
			m_ShipVelocity.x += normalX * 75.0f;
			m_ShipVelocity.y += normalY * 75.0f;
			asteroid.velocity.x -= normalX * 24.0f;
			asteroid.velocity.y -= normalY * 24.0f;
			m_ShipHealth = std::max(0.0f, m_ShipHealth - 12.0f);
			m_DamageCooldown = 1.0f;
			m_ShipImpactFlash = 0.45f;
			SetMessage(L"운석 피격! 선체 손상과 항로 이탈이 발생했습니다.", 2.0f);
		}
	}

	if (m_ShipHealth <= 0.0f)
	{
		m_ShipHealth = 35.0f;
		m_ShipPosition = { 0.0f, std::max(0.0f, m_ShipPosition.y - 90.0f) };
		m_ShipVelocity = { 0.0f, 0.0f };
		SetMessage(L"비상 자동 항법이 작동했습니다. 선체 내구도 35%로 복구합니다.", 3.0f);
	}
}

bool Game::CanMoveTo(float x, float y) const
{
	if (x < -11.7f || x > 11.7f || y < -11.7f || y > 11.7f) return false;
	const float radius = 0.28f;
	for (size_t i = 0; i < m_Obstacles.size(); ++i)
	{
		const Obstacle& obstacle = m_Obstacles[i];
		if (x + radius > obstacle.x - obstacle.halfWidth && x - radius < obstacle.x + obstacle.halfWidth &&
			y + radius > obstacle.y - obstacle.halfHeight && y - radius < obstacle.y + obstacle.halfHeight)
			return false;
	}
	return true;
}

bool Game::IsNear(const WorldPoint& point, float distance) const
{
	return Length(m_Player.x - point.x, m_Player.y - point.y) <= distance;
}

bool Game::AllEnemiesDefeated() const
{
	for (size_t i = 0; i < m_Enemies.size(); ++i)
		if (m_Enemies[i].health > 0) return false;
	return true;
}

void Game::SetMessage(const std::wstring& message, float seconds)
{
	m_Message = message;
	m_MessageTimer = seconds;
}

ScreenPoint Game::WorldToScreen(float x, float y, float height) const
{
	float cameraX = (m_Player.x - m_Player.y) * TileWidth * 0.5f;
	float cameraY = -(m_Player.x + m_Player.y) * TileHeight * 0.5f;
	ScreenPoint point =
	{
		(x - y) * TileWidth * 0.5f - cameraX,
		-(x + y) * TileHeight * 0.5f - cameraY - 30.0f + height
	};
	return point;
}

std::wstring Game::ObjectiveText() const
{
	if ((m_Mode == ShipControl || m_Mode == Transition) && m_Quest == QuestComplete)
		return m_TravelDestinationShip == 0 ? L"목표: 연구선 노크티스에 도킹하십시오" : L"목표: 난민 수송선 에레보스에 도킹하십시오";
	if (m_Quest == InspectTerminal) return L"목표: 의료실 단말기를 조사하십시오 [E]";
	if (m_Quest == FindCore && !AllEnemiesDefeated()) return L"목표: 기관실로 이동하여 침입체를 제거하십시오";
	if (m_Quest == FindCore) return L"목표: 항법 코어를 회수하십시오 [E]";
	if (m_Quest == ReturnToCockpit) return L"목표: 조종석으로 돌아가 항법 코어를 설치하십시오 [E]";
	if (m_Quest == FlyToSignal) return L"목표: 노크티스를 조종하여 구조 신호로 이동하십시오";
	return m_CurrentShip == 1 ? L"현재 위치: 난민 수송선 에레보스 | 조종석에서 노크티스로 귀환 가능" : L"현재 위치: 연구선 노크티스 | 조종석에서 에레보스로 이동 가능";
}

std::wstring Game::InteractionText() const
{
	if (m_Mode == ShipControl)
	{
		float distance = Length(m_ShipPosition.x - DestinationPosition.x, m_ShipPosition.y - DestinationPosition.y);
		if (distance >= 85.0f) return L"";
		if (m_Quest == FlyToSignal) return L"[E] 구조 신호에 진입";
		return m_TravelDestinationShip == 0 ? L"[E] 노크티스에 도킹" : L"[E] 에레보스에 도킹";
	}
	if (m_Mode != OnFoot) return L"";
	for (size_t i = 0; i < m_NeutralNpcs.size(); ++i)
	{
		const NeutralNpc& npc = m_NeutralNpcs[i];
		if (!npc.dead && IsNear(npc.position, 1.25f))
			return npc.dialogueId == 0 ? L"[E] 통신장교 서윤과 대화" : L"[E] 정비사 라울과 대화";
	}
	if (m_Quest == InspectTerminal && IsNear(TerminalPosition, 1.15f)) return L"[E] 단말기 조사";
	if (m_CurrentShip == 0 && !m_RecordRead && IsNear(RecordPosition, 1.05f)) return L"[E] 손상된 기록 읽기";
	if (m_CurrentShip == 0 && m_CoreRecovered && !m_SignalChecked && IsNear(SignalPosition, 1.05f)) return L"[E] 구조 신호 조사";
	if (m_Quest == FindCore && AllEnemiesDefeated() && IsNear(CorePosition, 1.2f)) return L"[E] 항법 코어 회수";
	if (m_Quest == ReturnToCockpit && IsNear(CockpitPosition, 1.25f)) return L"[E] 코어 설치 및 우주선 조종";
	if (m_Quest == QuestComplete && IsNear(CockpitPosition, 1.25f)) return m_CurrentShip == 1 ? L"[E] 노크티스로 귀환" : L"[E] 에레보스로 이동";
	return L"";
}

void Game::Render()
{
	if (m_Mode == ShipControl || m_Mode == Transition) RenderSpace();
	else if (m_Mode == Complete) RenderDestination();
	else RenderInterior();
	RenderInterface();
	m_Renderer->Present(m_TotalTime);
}

void Game::RenderInterior()
{
	m_Renderer->BeginFrame(m_CurrentShip == 0 ? 0.012f : 0.018f, 0.020f, m_CurrentShip == 0 ? 0.035f : 0.045f, 1.0f);
	int width = m_Renderer->Width();
	int height = m_Renderer->Height();
	m_Renderer->DrawRect(0.0f, height * 0.38f, static_cast<float>(width), height * 0.24f, 0.02f, 0.04f, 0.075f, 1.0f);
	m_Renderer->DrawDiamond(width * 0.34f, height * 0.39f, 330.0f, 180.0f, 0.10f, 0.15f, 0.24f, 0.55f);
	m_Renderer->DrawDiamond(width * 0.34f, height * 0.39f, 270.0f, 145.0f, 0.18f, 0.28f, 0.42f, 0.18f);
	// Layered hull ribs, conduits and pressure-door silhouettes establish the ship interior.
	for (int rib = -3; rib <= 3; ++rib)
	{
		float ribX = rib * width * 0.145f;
		m_Renderer->DrawRect(ribX, height * 0.30f, 10.0f, height * 0.25f, 0.11f, 0.17f, 0.22f, 0.92f);
		m_Renderer->DrawDiamond(ribX, height * 0.18f, 58.0f, 20.0f, 0.16f, 0.23f, 0.28f, 0.72f);
	}
	m_Renderer->DrawRect(-width * 0.36f, height * 0.25f, 92.0f, 54.0f, 0.035f, 0.08f, 0.11f, 1.0f);
	m_Renderer->DrawRect(-width * 0.36f, height * 0.25f, 62.0f, 32.0f, 0.10f, 0.50f, 0.61f, 0.32f);
	m_Renderer->DrawRect(width * 0.15f, height * 0.29f, width * 0.28f, 7.0f, 0.07f, 0.34f, 0.40f, 0.72f);

	for (int x = -12; x <= 12; ++x)
	{
		for (int y = -12; y <= 12; ++y)
		{
			ScreenPoint p = WorldToScreen(static_cast<float>(x), static_cast<float>(y));
			float checker = ((x + y) & 1) ? 0.015f : 0.0f;
			float restored = m_CoreRecovered ? 0.035f : 0.0f;
			m_Renderer->DrawDiamond(p.x, p.y, TileWidth - 2.0f, TileHeight - 1.0f,
				0.075f + checker + (m_CurrentShip == 1 ? 0.025f : 0.0f),
				0.105f + restored, 0.13f + restored + (m_CurrentShip == 1 ? 0.035f : 0.0f), 1.0f);
		}
	}

	for (int i = -12; i <= 12; ++i)
	{
		Obstacle north = { static_cast<float>(i), -12.2f, 0.48f, 0.25f, 58.0f, 0.12f, 0.18f, 0.22f, HullWall };
		Obstacle south = { static_cast<float>(i), 12.2f, 0.48f, 0.25f, 58.0f, 0.10f, 0.16f, 0.21f, HullWall };
		Obstacle west = { -12.2f, static_cast<float>(i), 0.25f, 0.48f, 58.0f, 0.11f, 0.17f, 0.22f, HullWall };
		Obstacle east = { 12.2f, static_cast<float>(i), 0.25f, 0.48f, 58.0f, 0.11f, 0.16f, 0.21f, HullWall };
		DrawWorldBlock(north);
		DrawWorldBlock(south);
		DrawWorldBlock(west);
		DrawWorldBlock(east);
	}

	ScreenPoint infestationA = WorldToScreen(9.0f, 9.3f, 2.0f);
	ScreenPoint infestationB = WorldToScreen(6.1f, 7.7f, 1.0f);
	m_Renderer->DrawDiamond(infestationA.x, infestationA.y, 165.0f, 58.0f, 0.28f, 0.04f, 0.38f, 0.78f);
	m_Renderer->DrawDiamond(infestationB.x, infestationB.y, 125.0f, 42.0f, 0.10f, 0.28f, 0.33f, 0.62f);

	struct RenderEntry { float depth; int type; int index; };
	std::vector<RenderEntry> entries;
	for (size_t i = 0; i < m_Obstacles.size(); ++i) entries.push_back({ m_Obstacles[i].x + m_Obstacles[i].y, 0, static_cast<int>(i) });
	entries.push_back({ m_Player.x + m_Player.y, 1, 0 });
	for (size_t i = 0; i < m_Enemies.size(); ++i)
		if (m_Enemies[i].health > 0 && m_Enemies[i].active) entries.push_back({ m_Enemies[i].position.x + m_Enemies[i].position.y, 2, static_cast<int>(i) });
	for (size_t i = 0; i < m_NeutralNpcs.size(); ++i)
		entries.push_back({ m_NeutralNpcs[i].position.x + m_NeutralNpcs[i].position.y, 3, static_cast<int>(i) });
	std::sort(entries.begin(), entries.end(), [](const RenderEntry& a, const RenderEntry& b) { return a.depth < b.depth; });
	for (size_t i = 0; i < entries.size(); ++i)
	{
		if (entries[i].type == 0) DrawWorldBlock(m_Obstacles[entries[i].index]);
		else if (entries[i].type == 1) DrawCharacter(m_Player, 0.20f, 0.72f, 0.86f, false);
		else if (entries[i].type == 2) DrawCharacter(m_Enemies[entries[i].index].position, 0.62f, 0.08f, 0.72f, true);
		else DrawNeutralNpc(m_NeutralNpcs[entries[i].index]);
	}
	for (size_t i = 0; i < m_Projectiles.size(); ++i)
	{
		ScreenPoint shot = WorldToScreen(m_Projectiles[i].position.x, m_Projectiles[i].position.y, 29.0f);
		m_Renderer->DrawSoftShadow(shot.x, shot.y - 25.0f, 24.0f, 7.0f, 0.18f);
		m_Renderer->DrawDiamond(shot.x, shot.y, 25.0f, 8.0f, 0.18f, 0.92f, 1.0f, 1.0f);
	}

	if (m_CurrentShip == 0)
	{
		ScreenPoint terminal = WorldToScreen(TerminalPosition.x, TerminalPosition.y, 22.0f);
		m_Renderer->DrawRect(terminal.x, terminal.y, 22.0f, 34.0f, 0.10f, 0.75f, 0.92f, 0.9f);
		ScreenPoint record = WorldToScreen(RecordPosition.x, RecordPosition.y, 5.0f);
		m_Renderer->DrawDiamond(record.x, record.y, 24.0f, 12.0f, m_RecordRead ? 0.18f : 0.75f, 0.58f, 0.18f, 0.95f);
		ScreenPoint core = WorldToScreen(CorePosition.x, CorePosition.y, 18.0f);
		if (!m_CoreRecovered)
			m_Renderer->DrawDiamond(core.x, core.y, 34.0f, 25.0f, 0.10f, 0.85f, 0.94f, AllEnemiesDefeated() ? 1.0f : 0.45f);
	}
	ScreenPoint cockpit = WorldToScreen(CockpitPosition.x, CockpitPosition.y, 24.0f);
	m_Renderer->DrawRect(cockpit.x, cockpit.y, 55.0f, 36.0f,
		m_CoreRecovered ? 0.10f : 0.22f, m_CoreRecovered ? 0.72f : 0.16f, m_CoreRecovered ? 0.82f : 0.18f, 0.9f);
	// Pilot station: raised chair, wraparound console, holographic gauges and forward canopy.
	m_Renderer->DrawSoftShadow(cockpit.x, cockpit.y - 7.0f, 105.0f, 24.0f, 0.85f);
	m_Renderer->DrawDiamond(cockpit.x, cockpit.y + 7.0f, 118.0f, 38.0f, 0.055f, 0.11f, 0.15f, 1.0f);
	m_Renderer->DrawRect(cockpit.x, cockpit.y + 34.0f, 35.0f, 48.0f, 0.07f, 0.12f, 0.16f, 1.0f);
	m_Renderer->DrawDiamond(cockpit.x - 42.0f, cockpit.y + 28.0f, 40.0f, 18.0f, 0.08f, 0.58f, 0.68f, 0.82f);
	m_Renderer->DrawDiamond(cockpit.x + 42.0f, cockpit.y + 28.0f, 40.0f, 18.0f, 0.08f, 0.58f, 0.68f, 0.82f);
	m_Renderer->DrawRect(cockpit.x, cockpit.y + 61.0f, 98.0f, 7.0f, 0.20f, 0.68f, 0.78f, 0.60f);
	for (int gauge = -2; gauge <= 2; ++gauge)
		m_Renderer->DrawRect(cockpit.x + gauge * 17.0f, cockpit.y + 18.0f, 8.0f, 4.0f,
			gauge == 0 ? 0.92f : 0.18f, gauge == 0 ? 0.28f : 0.82f, 0.86f, 0.92f);
	if (m_CurrentShip == 0)
	{
		ScreenPoint signal = WorldToScreen(SignalPosition.x, SignalPosition.y, 12.0f);
		if (m_CoreRecovered && !m_SignalChecked)
			m_Renderer->DrawDiamond(signal.x, signal.y, 30.0f, 18.0f, 0.52f, 0.12f, 0.62f, 0.85f);
	}

	float pulse = 0.62f + 0.22f * std::sin(m_TotalTime * 4.0f);
	ScreenPoint warningLight = WorldToScreen(-10.5f, -10.5f, 55.0f);
	m_Renderer->DrawDiamond(warningLight.x, warningLight.y, 42.0f, 18.0f, 0.85f, 0.04f, 0.05f, pulse);
	m_Renderer->DrawString(-width * 0.5f + 22.0f, height * 0.5f - 58.0f,
		m_CurrentShip == 0 ? L"비상 전력" : L"에레보스 생체 신호 감지", m_CurrentShip == 0 ? 0.90f : 0.72f, 0.30f, m_CurrentShip == 0 ? 0.25f : 0.82f, 0.88f);
	m_Renderer->DrawRect(width * 0.42f, -height * 0.27f, 12.0f, 42.0f, 0.12f, 0.60f, 0.75f, 0.55f);
}

void Game::DrawWorldBlock(const Obstacle& obstacle)
{
	ScreenPoint foot = WorldToScreen(obstacle.x, obstacle.y);
	float width = (obstacle.halfWidth + obstacle.halfHeight) * TileWidth;
	float topY = foot.y + obstacle.visualHeight;
	m_Renderer->DrawSoftShadow(foot.x + 10.0f, foot.y - 5.0f, width * 0.90f, 18.0f, 0.85f);
	if (obstacle.type == HullWall)
	{
		m_Renderer->DrawRect(foot.x, foot.y + obstacle.visualHeight * 0.5f, width * 0.72f, obstacle.visualHeight,
			obstacle.r * 0.70f, obstacle.g * 0.70f, obstacle.b * 0.76f, 1.0f);
		m_Renderer->DrawDiamond(foot.x, topY, width, (obstacle.halfWidth + obstacle.halfHeight) * TileHeight,
			obstacle.r, obstacle.g, obstacle.b, 1.0f);
		m_Renderer->DrawRect(foot.x, foot.y + obstacle.visualHeight * 0.52f, width * 0.62f, 4.0f,
			0.20f, 0.42f, 0.48f, 0.45f);
		return;
	}

	if (obstacle.type == CargoCrate)
	{
		m_Renderer->DrawRect(foot.x, foot.y + obstacle.visualHeight * 0.44f, width * 0.76f, obstacle.visualHeight * 0.88f,
			0.16f, 0.19f, 0.20f, 1.0f);
		m_Renderer->DrawDiamond(foot.x, topY, width, 27.0f, 0.29f, 0.25f, 0.20f, 1.0f);
		m_Renderer->DrawRect(foot.x, foot.y + obstacle.visualHeight * 0.48f, 6.0f, obstacle.visualHeight * 0.78f,
			0.62f, 0.39f, 0.12f, 0.92f);
		m_Renderer->DrawRect(foot.x, topY, width * 0.62f, 3.0f, 0.72f, 0.45f, 0.14f, 0.86f);
		return;
	}

	if (obstacle.type == ControlConsole)
	{
		m_Renderer->DrawRect(foot.x, foot.y + obstacle.visualHeight * 0.34f, width * 0.54f, obstacle.visualHeight * 0.67f,
			0.07f, 0.12f, 0.15f, 1.0f);
		m_Renderer->DrawDiamond(foot.x, topY - 3.0f, width * 0.88f, 26.0f, 0.08f, 0.42f, 0.50f, 1.0f);
		m_Renderer->DrawRect(foot.x, topY + 1.0f, width * 0.53f, 9.0f, 0.12f, 0.72f, 0.82f, 0.82f);
		for (int light = -2; light <= 2; ++light)
			m_Renderer->DrawRect(foot.x + light * 9.0f, topY + 2.0f, 4.0f, 3.0f,
				light == 1 ? 0.92f : 0.16f, light == 1 ? 0.25f : 0.78f, 0.68f, 1.0f);
		return;
	}

	if (obstacle.type == MedicalPod)
	{
		m_Renderer->DrawRect(foot.x, foot.y + obstacle.visualHeight * 0.46f, width * 0.68f, obstacle.visualHeight * 0.86f,
			0.12f, 0.18f, 0.20f, 1.0f);
		m_Renderer->DrawDiamond(foot.x, topY - 2.0f, width * 0.82f, 30.0f, 0.25f, 0.48f, 0.50f, 0.90f);
		m_Renderer->DrawRect(foot.x, topY - 1.0f, 17.0f, 17.0f, 0.78f, 0.82f, 0.77f, 0.95f);
		m_Renderer->DrawRect(foot.x, topY - 1.0f, 5.0f, 15.0f, 0.15f, 0.62f, 0.58f, 1.0f);
		m_Renderer->DrawRect(foot.x, topY - 1.0f, 15.0f, 5.0f, 0.15f, 0.62f, 0.58f, 1.0f);
		return;
	}

	if (obstacle.type == CoolantPipe)
	{
		for (int pipe = -1; pipe <= 1; pipe += 2)
		{
			m_Renderer->DrawRect(foot.x + pipe * width * 0.18f, foot.y + obstacle.visualHeight * 0.48f,
				11.0f, obstacle.visualHeight * 0.94f, 0.11f, 0.26f, 0.31f, 1.0f);
			m_Renderer->DrawRect(foot.x + pipe * width * 0.18f, topY - 8.0f, 17.0f, 6.0f,
				0.26f, 0.55f, 0.61f, 0.95f);
		}
		m_Renderer->DrawRect(foot.x, foot.y + 14.0f, width * 0.68f, 8.0f, 0.08f, 0.18f, 0.21f, 1.0f);
		return;
	}

	if (obstacle.type == PowerRelay)
	{
		float pulse = 0.68f + std::sin(m_TotalTime * 6.0f + obstacle.x) * 0.20f;
		m_Renderer->DrawRect(foot.x, foot.y + obstacle.visualHeight * 0.48f, width * 0.58f, obstacle.visualHeight * 0.96f,
			0.08f, 0.12f, 0.17f, 1.0f);
		m_Renderer->DrawDiamond(foot.x, topY, width * 0.76f, 24.0f, 0.16f, 0.28f, 0.34f, 1.0f);
		m_Renderer->DrawRect(foot.x, foot.y + obstacle.visualHeight * 0.53f, 8.0f, obstacle.visualHeight * 0.62f,
			0.20f, 0.78f, 0.92f, pulse);
		m_Renderer->DrawDiamond(foot.x, topY + 4.0f, 14.0f, 10.0f, 0.32f, 0.88f, 1.0f, pulse);
		return;
	}

	// Engine modules use a broad armored housing and a pulsing central coolant core.
	float enginePulse = 0.55f + std::sin(m_TotalTime * 4.0f) * 0.18f;
	m_Renderer->DrawRect(foot.x, foot.y + obstacle.visualHeight * 0.43f, width * 0.82f, obstacle.visualHeight * 0.86f,
		0.07f, 0.12f, 0.15f, 1.0f);
	m_Renderer->DrawDiamond(foot.x, topY, width, 31.0f, 0.13f, 0.20f, 0.24f, 1.0f);
	m_Renderer->DrawDiamond(foot.x, topY + 2.0f, width * 0.48f, 18.0f, 0.18f, 0.68f, 0.78f, enginePulse);
	m_Renderer->DrawRect(foot.x - width * 0.31f, foot.y + obstacle.visualHeight * 0.46f, 7.0f, obstacle.visualHeight * 0.64f,
		0.30f, 0.35f, 0.36f, 1.0f);
	m_Renderer->DrawRect(foot.x + width * 0.31f, foot.y + obstacle.visualHeight * 0.46f, 7.0f, obstacle.visualHeight * 0.64f,
		0.30f, 0.35f, 0.36f, 1.0f);
}

void Game::DrawCharacter(const WorldPoint& point, float r, float g, float b, bool enemy)
{
	ScreenPoint foot = WorldToScreen(point.x, point.y);
	float phase = std::sin(m_TotalTime * (enemy ? 7.0f : 10.0f) + point.x * 0.7f);
	if (!enemy && !m_PlayerMoving) phase = std::sin(m_TotalTime * 2.2f) * 0.12f;
	float bob = std::fabs(phase) * (enemy || m_PlayerMoving ? 2.5f : 1.0f);
	m_Renderer->DrawSoftShadow(foot.x + 5.0f, foot.y, enemy ? 38.0f : 32.0f, enemy ? 15.0f : 12.0f, 1.0f);
	if (enemy)
	{
		DrawRotatedRect(m_Renderer, foot.x - 13.0f, foot.y + 14.0f, 9.0f, 28.0f, phase * 0.28f, r * 0.65f, g, b * 0.75f, 1.0f);
		DrawRotatedRect(m_Renderer, foot.x + 13.0f, foot.y + 14.0f, 9.0f, 28.0f, -phase * 0.28f, r * 0.65f, g, b * 0.75f, 1.0f);
		m_Renderer->DrawDiamond(foot.x, foot.y + 30.0f + bob, 38.0f, 50.0f, r, g, b, 1.0f);
		m_Renderer->DrawDiamond(foot.x, foot.y + 55.0f + bob, 25.0f, 19.0f, r * 0.82f, g * 0.65f, b, 1.0f);
		m_Renderer->DrawRect(foot.x - 5.0f, foot.y + 57.0f + bob, 5.0f, 5.0f, 0.20f, 0.95f, 0.82f, 1.0f);
		m_Renderer->DrawRect(foot.x + 5.0f, foot.y + 57.0f + bob, 5.0f, 5.0f, 0.20f, 0.95f, 0.82f, 1.0f);
	}
	else
	{
		float stride = m_PlayerMoving ? phase : 0.0f;
		// Boots and articulated pressure-suit legs.
		DrawRotatedRect(m_Renderer, foot.x - 7.0f + stride * 2.0f, foot.y + 10.0f, 8.0f, 23.0f,
			stride * 0.22f, 0.09f, 0.24f, 0.31f, 1.0f);
		DrawRotatedRect(m_Renderer, foot.x + 7.0f - stride * 2.0f, foot.y + 10.0f, 8.0f, 23.0f,
			-stride * 0.22f, 0.09f, 0.24f, 0.31f, 1.0f);
		m_Renderer->DrawRect(foot.x - 8.0f + stride * 3.0f, foot.y + 1.0f, 13.0f, 7.0f, 0.05f, 0.12f, 0.17f, 1.0f);
		m_Renderer->DrawRect(foot.x + 8.0f - stride * 3.0f, foot.y + 1.0f, 13.0f, 7.0f, 0.05f, 0.12f, 0.17f, 1.0f);
		// Backpack, torso shell and chest life-support light.
		m_Renderer->DrawRect(foot.x - 4.0f, foot.y + 36.0f + bob, 31.0f, 37.0f, 0.04f, 0.11f, 0.16f, 1.0f);
		m_Renderer->DrawRect(foot.x, foot.y + 34.0f + bob, 25.0f, 35.0f, r, g, b, 1.0f);
		m_Renderer->DrawRect(foot.x, foot.y + 36.0f + bob, 17.0f, 27.0f, 0.08f, 0.23f, 0.30f, 1.0f);
		m_Renderer->DrawDiamond(foot.x, foot.y + 39.0f + bob, 8.0f, 6.0f, 0.20f, 0.95f, 0.88f, 1.0f);
		m_Renderer->DrawDiamond(foot.x - 16.0f, foot.y + 48.0f + bob, 13.0f, 11.0f, r * 0.85f, g * 0.85f, b, 1.0f);
		m_Renderer->DrawDiamond(foot.x + 16.0f, foot.y + 48.0f + bob, 13.0f, 11.0f, r * 0.85f, g * 0.85f, b, 1.0f);
		DrawRotatedRect(m_Renderer, foot.x - 17.0f, foot.y + 36.0f + bob, 7.0f, 25.0f,
			-stride * 0.30f, r * 0.75f, g * 0.78f, b * 0.82f, 1.0f);
		DrawRotatedRect(m_Renderer, foot.x + 17.0f, foot.y + 36.0f + bob, 7.0f, 25.0f,
			stride * 0.30f, r * 0.75f, g * 0.78f, b * 0.82f, 1.0f);
		// Helmet shell, dark visor and oxygen valve.
		m_Renderer->DrawDiamond(foot.x, foot.y + 61.0f + bob, 27.0f, 23.0f, 0.72f, 0.78f, 0.80f, 1.0f);
		m_Renderer->DrawRect(foot.x, foot.y + 62.0f + bob, 17.0f, 8.0f, 0.025f, 0.11f, 0.17f, 1.0f);
		m_Renderer->DrawRect(foot.x, foot.y + 63.0f + bob, 11.0f, 3.0f, 0.15f, 0.82f, 0.92f, 1.0f);
		m_Renderer->DrawDiamond(foot.x - 15.0f, foot.y + 58.0f + bob, 7.0f, 7.0f, 0.20f, 0.75f, 0.82f, 1.0f);
		float aimAngle = std::atan2(static_cast<float>(m_Renderer->Height() / 2 - m_MouseY + 30),
			static_cast<float>(m_MouseX - m_Renderer->Width() / 2));
		DrawRotatedRect(m_Renderer, foot.x + std::cos(aimAngle) * 18.0f, foot.y + 36.0f + bob + std::sin(aimAngle) * 8.0f,
			30.0f, 6.0f, aimAngle, 0.08f, 0.16f, 0.20f, 1.0f);
		DrawRotatedRect(m_Renderer, foot.x + std::cos(aimAngle) * 29.0f, foot.y + 36.0f + bob + std::sin(aimAngle) * 13.0f,
			8.0f, 3.0f, aimAngle, 0.18f, 0.82f, 0.92f, 1.0f);
		if (m_AttackFlash > 0.0f)
		{
			float muzzleX = foot.x + std::cos(aimAngle) * 36.0f;
			float muzzleY = foot.y + 36.0f + bob + std::sin(aimAngle) * 17.0f;
			m_Renderer->DrawSoftShadow(muzzleX, muzzleY - 25.0f, 36.0f, 10.0f, 0.28f);
			m_Renderer->DrawDiamond(muzzleX, muzzleY, 36.0f, 12.0f, 0.20f, 0.88f, 1.0f, 0.90f);
		}
	}
}

void Game::DrawNeutralNpc(const NeutralNpc& npc)
{
	ScreenPoint foot = WorldToScreen(npc.position.x, npc.position.y);
	if (npc.dead)
	{
		m_Renderer->DrawSoftShadow(foot.x, foot.y, 55.0f, 15.0f, 0.95f);
		DrawRotatedRect(m_Renderer, foot.x, foot.y + 9.0f, 45.0f, 18.0f, -0.20f,
			0.22f, 0.28f, 0.30f, 1.0f);
		m_Renderer->DrawDiamond(foot.x - 24.0f, foot.y + 14.0f, 18.0f, 15.0f,
			0.48f, 0.52f, 0.50f, 1.0f);
		DrawRotatedRect(m_Renderer, foot.x + 24.0f, foot.y + 4.0f, 27.0f, 6.0f, 0.28f,
			0.13f, 0.17f, 0.18f, 1.0f);
		m_Renderer->DrawRect(foot.x - 5.0f, foot.y + 10.0f, 7.0f, 4.0f, 0.52f, 0.06f, 0.05f, 0.75f);
		return;
	}

	float tremble = std::sin(m_TotalTime * 28.0f + npc.animationOffset) * 2.2f;
	m_Renderer->DrawSoftShadow(foot.x, foot.y, 30.0f, 11.0f, 0.85f);
	DrawRotatedRect(m_Renderer, foot.x - 6.0f + tremble, foot.y + 9.0f, 7.0f, 20.0f, 0.12f,
		0.12f, 0.20f, 0.23f, 1.0f);
	DrawRotatedRect(m_Renderer, foot.x + 6.0f + tremble, foot.y + 9.0f, 7.0f, 20.0f, -0.12f,
		0.12f, 0.20f, 0.23f, 1.0f);
	m_Renderer->DrawRect(foot.x + tremble, foot.y + 31.0f, 25.0f, 29.0f, 0.24f, 0.32f, 0.34f, 1.0f);
	DrawRotatedRect(m_Renderer, foot.x - 12.0f + tremble, foot.y + 35.0f, 6.0f, 23.0f, -0.55f,
		0.18f, 0.25f, 0.27f, 1.0f);
	DrawRotatedRect(m_Renderer, foot.x + 12.0f + tremble, foot.y + 35.0f, 6.0f, 23.0f, 0.55f,
		0.18f, 0.25f, 0.27f, 1.0f);
	m_Renderer->DrawDiamond(foot.x + tremble, foot.y + 52.0f, 22.0f, 19.0f, 0.42f, 0.47f, 0.45f, 1.0f);
	m_Renderer->DrawRect(foot.x + tremble, foot.y + 52.0f, 14.0f, 6.0f, 0.04f, 0.09f, 0.10f, 1.0f);
	m_Renderer->DrawString(foot.x - 42.0f, foot.y + 67.0f,
		npc.dialogueId == 0 ? L"통신장교 서윤" : L"정비사 라울", 0.66f, 0.78f, 0.76f, 0.85f);
}

void Game::RenderSpace()
{
	m_Renderer->BeginFrame(0.004f, 0.008f, 0.025f, 1.0f);
	int width = m_Renderer->Width();
	int height = m_Renderer->Height();
	for (int i = 0; i < 44; ++i)
	{
		float x = static_cast<float>((i * 193) % (width + 160) - width / 2 - 80);
		float y = static_cast<float>((i * 97) % (height + 120) - height / 2 - 60);
		float parallax = std::fmod(m_ShipPosition.y * (0.04f + (i % 3) * 0.025f), static_cast<float>(height + 120));
		y -= parallax;
		if (y < -height * 0.5f - 60.0f) y += height + 120.0f;
		float size = 1.0f + static_cast<float>(i % 3);
		m_Renderer->DrawRect(x, y, size, size, 0.45f, 0.62f, 0.82f, 0.7f);
	}
	m_Renderer->DrawDiamond(width * 0.37f, height * 0.34f, 310.0f, 185.0f, 0.08f, 0.13f, 0.23f, 0.75f);
	m_Renderer->DrawDiamond(width * 0.37f, height * 0.34f, 255.0f, 145.0f, 0.16f, 0.24f, 0.39f, 0.32f);

	const WorldPoint debris[] = { { -90.0f, 170.0f }, { 85.0f, 290.0f }, { -70.0f, 410.0f }, { 55.0f, 520.0f } };
	for (int i = 0; i < 4; ++i)
	{
		float x = (debris[i].x - m_ShipPosition.x) * 0.72f;
		float y = (debris[i].y - m_ShipPosition.y) * 0.72f;
		m_Renderer->DrawDiamond(x, y, 72.0f + i * 8.0f, 30.0f + i * 5.0f, 0.18f, 0.21f, 0.25f, 1.0f);
		m_Renderer->DrawRect(x + 12.0f, y + 7.0f, 26.0f, 5.0f, 0.65f, 0.12f, 0.10f, 0.65f);
	}
	for (size_t i = 0; i < m_Asteroids.size(); ++i) DrawAsteroid(m_Asteroids[i]);

	float destinationX = (DestinationPosition.x - m_ShipPosition.x) * 0.72f;
	float destinationY = (DestinationPosition.y - m_ShipPosition.y) * 0.72f;
	float beaconPulse = 0.55f + std::sin(m_TotalTime * 5.0f) * 0.25f;
	m_Renderer->DrawDiamond(destinationX, destinationY, 125.0f, 58.0f, 0.08f, 0.34f, 0.46f, beaconPulse);
	m_Renderer->DrawRect(destinationX, destinationY + 12.0f, 70.0f, 20.0f, 0.10f, 0.18f, 0.23f, 1.0f);
	DrawShip(destinationX, destinationY + 14.0f, -Pi * 0.5f, m_TravelDestinationShip == 1 ? 2.3f : 1.6f,
		m_TravelDestinationShip == 1 ? 0.32f : 0.20f, 0.48f, 0.58f);
	float impactShake = m_ShipImpactFlash > 0.0f ? std::sin(m_TotalTime * 95.0f) * 6.0f : 0.0f;
	DrawShip(impactShake, -35.0f + impactShake * 0.35f, m_ShipAngle, 1.0f, 0.20f, 0.67f, 0.82f);

	if (Length(m_ShipVelocity.x, m_ShipVelocity.y) > 80.0f)
		m_Renderer->DrawDiamond(-std::cos(m_ShipAngle) * 35.0f,
			-35.0f - std::sin(m_ShipAngle) * 35.0f, 48.0f, 16.0f, 0.18f, 0.70f, 1.0f, 0.55f);

	if (m_Mode == Transition)
	{
		float alpha = Clamp(m_TransitionTimer / 1.4f, 0.0f, 1.0f);
		m_Renderer->DrawRect(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 0.0f, 0.0f, alpha);
	}
	if (m_ShipImpactFlash > 0.0f)
	{
		float flashAlpha = Clamp(m_ShipImpactFlash / 0.45f, 0.0f, 1.0f) * 0.34f;
		m_Renderer->DrawRect(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height),
			0.78f, 0.08f, 0.035f, flashAlpha);
	}
}

void Game::DrawAsteroid(const Asteroid& asteroid)
{
	float x = (asteroid.position.x - m_ShipPosition.x) * 0.72f;
	float y = (asteroid.position.y - m_ShipPosition.y) * 0.72f;
	float radius = asteroid.radius * 1.45f;
	ScreenPoint p0 = Rotate(radius, 0.0f, asteroid.rotation, x, y);
	ScreenPoint p1 = Rotate(radius * 0.25f, radius * 0.78f, asteroid.rotation, x, y);
	ScreenPoint p2 = Rotate(-radius * 0.82f, radius * 0.38f, asteroid.rotation, x, y);
	ScreenPoint p3 = Rotate(-radius * 0.62f, -radius * 0.66f, asteroid.rotation, x, y);
	m_Renderer->DrawSoftShadow(x + 12.0f, y - 12.0f, radius * 1.9f, radius * 0.65f, 0.92f);
	m_Renderer->DrawQuad(p0, p1, p2, p3, 0.23f, 0.20f, 0.18f, 1.0f);
	m_Renderer->DrawDiamond(x - radius * 0.18f, y + radius * 0.16f,
		radius * 0.55f, radius * 0.34f, 0.10f, 0.09f, 0.085f, 0.82f);
	m_Renderer->DrawDiamond(x + radius * 0.24f, y - radius * 0.14f,
		radius * 0.31f, radius * 0.22f, 0.36f, 0.30f, 0.25f, 0.78f);
}

void Game::DrawShip(float x, float y, float angle, float scale, float r, float g, float b)
{
	ScreenPoint nose = Rotate(42.0f * scale, 0.0f, angle, x, y);
	ScreenPoint shoulderTop = Rotate(4.0f * scale, 15.0f * scale, angle, x, y);
	ScreenPoint rearTop = Rotate(-30.0f * scale, 12.0f * scale, angle, x, y);
	ScreenPoint rearBottom = Rotate(-30.0f * scale, -12.0f * scale, angle, x, y);
	ScreenPoint shoulderBottom = Rotate(4.0f * scale, -15.0f * scale, angle, x, y);
	ScreenPoint centerRear = Rotate(-13.0f * scale, 0.0f, angle, x, y);
	m_Renderer->DrawQuad(nose, shoulderTop, centerRear, shoulderBottom, r, g, b, 1.0f);
	m_Renderer->DrawQuad(shoulderTop, rearTop, centerRear, centerRear, r * 0.62f, g * 0.64f, b * 0.68f, 1.0f);
	m_Renderer->DrawQuad(centerRear, rearBottom, shoulderBottom, centerRear, r * 0.52f, g * 0.58f, b * 0.63f, 1.0f);
	// Swept wings and paired engine pods make the silhouette readable while turning.
	DrawRotatedRect(m_Renderer, x, y, 48.0f * scale, 9.0f * scale, angle, r * 0.58f, g * 0.62f, b * 0.68f, 1.0f);
	ScreenPoint engineA = Rotate(-24.0f * scale, 17.0f * scale, angle, x, y);
	ScreenPoint engineB = Rotate(-24.0f * scale, -17.0f * scale, angle, x, y);
	DrawRotatedRect(m_Renderer, engineA.x, engineA.y, 25.0f * scale, 8.0f * scale, angle,
		0.08f, 0.18f, 0.23f, 1.0f);
	DrawRotatedRect(m_Renderer, engineB.x, engineB.y, 25.0f * scale, 8.0f * scale, angle,
		0.08f, 0.18f, 0.23f, 1.0f);
	ScreenPoint canopy = Rotate(12.0f * scale, 0.0f, angle, x, y);
	m_Renderer->DrawDiamond(canopy.x, canopy.y, 20.0f * scale, 11.0f * scale, 0.55f, 0.84f, 0.92f, 0.95f);
	ScreenPoint armor = Rotate(-7.0f * scale, 0.0f, angle, x, y);
	m_Renderer->DrawDiamond(armor.x, armor.y, 17.0f * scale, 8.0f * scale, r * 1.15f, g * 1.05f, b, 0.95f);
	ScreenPoint exhaustA = Rotate(-38.0f * scale, 17.0f * scale, angle, x, y);
	ScreenPoint exhaustB = Rotate(-38.0f * scale, -17.0f * scale, angle, x, y);
	m_Renderer->DrawDiamond(exhaustA.x, exhaustA.y, 8.0f * scale, 6.0f * scale, 0.20f, 0.78f, 1.0f, 0.82f);
	m_Renderer->DrawDiamond(exhaustB.x, exhaustB.y, 8.0f * scale, 6.0f * scale, 0.20f, 0.78f, 1.0f, 0.82f);
}

void Game::RenderDestination()
{
	m_Renderer->BeginFrame(0.006f, 0.012f, 0.025f, 1.0f);
	int width = m_Renderer->Width();
	int height = m_Renderer->Height();
	m_Renderer->DrawDiamond(0.0f, -20.0f, width * 0.86f, height * 0.45f, 0.07f, 0.10f, 0.14f, 1.0f);
	m_Renderer->DrawRect(-115.0f, 15.0f, 180.0f, 70.0f, 0.08f, 0.28f, 0.34f, 1.0f);
	m_Renderer->DrawRect(90.0f, 28.0f, 155.0f, 62.0f, 0.24f, 0.05f, 0.30f, 0.95f);
	m_Renderer->DrawRect(-115.0f, 62.0f, 8.0f, 42.0f, 0.18f, 0.85f, 0.90f, 0.75f);
	m_Renderer->DrawDiamond(105.0f, 62.0f, 95.0f, 32.0f, 0.36f, 0.08f, 0.48f, 0.75f);
	DrawShip(0.0f, -125.0f, Pi * 0.5f, 1.3f, 0.22f, 0.68f, 0.82f);
}

void Game::RenderInterface()
{
	int width = m_Renderer->Width();
	int height = m_Renderer->Height();
	m_Renderer->DrawRect(0.0f, height * 0.5f - 27.0f, static_cast<float>(width), 54.0f, 0.015f, 0.025f, 0.045f, 0.88f);
	m_Renderer->DrawString(-width * 0.5f + 20.0f, height * 0.5f - 33.0f, ObjectiveText(), 0.62f, 0.88f, 0.94f, 1.0f);

	if (m_Mode == OnFoot)
	{
		std::wostringstream health;
		health << L"전투복 " << static_cast<int>(m_PlayerHealth) << L"%";
		m_Renderer->DrawRect(-width * 0.5f + 93.0f, -height * 0.5f + 27.0f, 170.0f, 35.0f, 0.025f, 0.04f, 0.06f, 0.9f);
		m_Renderer->DrawRect(-width * 0.5f + 18.0f + m_PlayerHealth * 0.72f, -height * 0.5f + 18.0f,
			m_PlayerHealth * 1.44f, 6.0f, 0.15f, 0.68f, 0.76f, 1.0f);
		m_Renderer->DrawString(-width * 0.5f + 24.0f, -height * 0.5f + 25.0f, health.str(), 0.72f, 0.86f, 0.88f, 1.0f);
		if (m_Quest != InspectTerminal)
		{
			std::wostringstream weapon;
			weapon << L"펄스 카빈 " << static_cast<int>(m_WeaponEnergy) << L"%";
			m_Renderer->DrawRect(-width * 0.5f + 105.0f, -height * 0.5f + 58.0f, 194.0f, 25.0f, 0.025f, 0.04f, 0.06f, 0.90f);
			m_Renderer->DrawString(-width * 0.5f + 20.0f, -height * 0.5f + 54.0f,
				m_ReloadTimer > 0.0f ? L"펄스 카빈 충전 중..." : weapon.str(), 0.34f, 0.86f, 0.92f, 1.0f);
		}
	}
	else if (m_Mode == ShipControl || m_Mode == Transition)
	{
		std::wostringstream ship;
		ship << (m_CurrentShip == 0 ? L"노크티스 선체 " : L"에레보스 선체 ") << static_cast<int>(m_ShipHealth) << L"%";
		m_Renderer->DrawRect(-width * 0.5f + 105.0f, -height * 0.5f + 27.0f, 194.0f, 35.0f, 0.025f, 0.04f, 0.06f, 0.90f);
		m_Renderer->DrawRect(-width * 0.5f + 18.0f + m_ShipHealth * 0.82f, -height * 0.5f + 18.0f,
			m_ShipHealth * 1.64f, 6.0f, m_ShipHealth < 40.0f ? 0.86f : 0.16f,
			m_ShipHealth < 40.0f ? 0.12f : 0.68f, 0.72f, 1.0f);
		m_Renderer->DrawString(-width * 0.5f + 22.0f, -height * 0.5f + 25.0f, ship.str(), 0.58f, 0.82f, 0.92f, 1.0f);
		if (m_ShipImpactFlash > 0.0f)
			m_Renderer->DrawString(-90.0f, height * 0.5f - 67.0f, L"경고: 선체 충격 감지", 1.0f, 0.28f, 0.16f, 1.0f);
	}

	std::wstring interaction = InteractionText();
	if (!interaction.empty())
	{
		m_Renderer->DrawRect(0.0f, -height * 0.5f + 72.0f, 310.0f, 34.0f, 0.02f, 0.05f, 0.07f, 0.92f);
		m_Renderer->DrawString(-145.0f, -height * 0.5f + 68.0f, interaction, 0.78f, 0.94f, 0.96f, 1.0f);
	}
	if (m_MessageTimer > 0.0f)
	{
		m_Renderer->DrawRect(0.0f, -height * 0.5f + 28.0f, 590.0f, 32.0f, 0.025f, 0.025f, 0.045f, 0.92f);
		m_Renderer->DrawString(-280.0f, -height * 0.5f + 24.0f, m_Message, 0.76f, 0.78f, 0.88f, 1.0f);
	}
	if (m_Keys['\t'] && m_Mode == OnFoot)
		m_Renderer->DrawString(-220.0f, height * 0.5f - 58.0f, L"WASD 이동 | E 상호작용 | 마우스 공격 | Space 회피", 0.60f, 0.75f, 0.80f, 1.0f);
	if (m_Paused)
	{
		m_Renderer->DrawRect(0.0f, 0.0f, 300.0f, 105.0f, 0.01f, 0.015f, 0.025f, 0.95f);
		m_Renderer->DrawString(-38.0f, 12.0f, L"일시 정지", 0.76f, 0.90f, 0.94f, 1.0f);
		m_Renderer->DrawString(-88.0f, -18.0f, L"계속하려면 ESC를 누르십시오", 0.48f, 0.63f, 0.68f, 1.0f);
	}
	if (m_Mode == Complete)
	{
		m_Renderer->DrawRect(0.0f, height * 0.27f, 470.0f, 105.0f, 0.01f, 0.02f, 0.035f, 0.94f);
		m_Renderer->DrawString(-115.0f, height * 0.27f + 20.0f, L"퀘스트 완료: 마지막 항로", 0.32f, 0.90f, 0.92f, 1.0f);
		m_Renderer->DrawString(-175.0f, height * 0.27f - 8.0f, L"새 목표: 구조 신호의 근원을 조사하십시오", 0.70f, 0.72f, 0.82f, 1.0f);
		m_Renderer->DrawString(-72.0f, height * 0.27f - 34.0f, L"R 키로 다시 시작", 0.48f, 0.65f, 0.70f, 1.0f);
	}
}
