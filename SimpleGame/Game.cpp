#include "stdafx.h"
#include "Game.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <queue>
#include <random>
#include <sstream>
#include "Dependencies\\freeglut.h"

namespace
{
const float Pi = 3.1415926535f;
const float TileWidth = 64.0f;
const float TileHeight = 32.0f;
const WorldPoint TerminalPosition = {-9.0f, -8.0f};
const WorldPoint RecordPosition = {-4.0f, -4.5f};
const WorldPoint SignalPosition = {0.0f, 3.0f};
const WorldPoint CorePosition = {8.5f, 8.5f};
const WorldPoint CockpitPosition = {-9.0f, 8.5f};
const WorldPoint DestinationPosition = {0.0f, 650.0f};
const WorldPoint FirstLevelWeaponPosition = {-1.5f, 0.0f};

float Clamp(float value, float minimum, float maximum)
{
	return value < minimum ? minimum : (value > maximum ? maximum : value);
}

float Length(float x, float y)
{
	return std::sqrt(x * x + y * y);
}

float DistanceToSegment(
	const WorldPoint& point, const WorldPoint& segmentStart, const WorldPoint& segmentEnd)
{
	float segmentX = segmentEnd.x - segmentStart.x;
	float segmentY = segmentEnd.y - segmentStart.y;
	float lengthSquared = segmentX * segmentX + segmentY * segmentY;
	if (lengthSquared <= 0.0001f)
		return Length(point.x - segmentStart.x, point.y - segmentStart.y);

	float projection =
		((point.x - segmentStart.x) * segmentX + (point.y - segmentStart.y) * segmentY) /
		lengthSquared;
	projection = Clamp(projection, 0.0f, 1.0f);
	float closestX = segmentStart.x + segmentX * projection;
	float closestY = segmentStart.y + segmentY * projection;
	return Length(point.x - closestX, point.y - closestY);
}

ScreenPoint Rotate(float x, float y, float angle, float centerX, float centerY)
{
	const float cosine = std::cos(angle);
	const float sine = std::sin(angle);
	ScreenPoint point = {centerX + x * cosine - y * sine, centerY + x * sine + y * cosine};
	return point;
}

void DrawRotatedRect(PrototypeRenderer* renderer,
	float x,
	float y,
	float width,
	float height,
	float angle,
	float r,
	float g,
	float b,
	float alpha)
{
	ScreenPoint p0 = Rotate(-width * 0.5f, -height * 0.5f, angle, x, y);
	ScreenPoint p1 = Rotate(width * 0.5f, -height * 0.5f, angle, x, y);
	ScreenPoint p2 = Rotate(width * 0.5f, height * 0.5f, angle, x, y);
	ScreenPoint p3 = Rotate(-width * 0.5f, height * 0.5f, angle, x, y);
	renderer->DrawQuad(p0, p1, p2, p3, r, g, b, alpha);
}
} // namespace

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
	m_Player = {-9.5f, -9.0f};
	m_LastMoveDirection = {1.0f, 0.0f};
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
	m_ExperienceOrbs.clear();
	m_LevelWeaponCollected = false;
	m_AmmoInMagazine = 12;
	m_SpareMagazines = 3;
	m_ShotsFired = 0;
	m_PlayerLevel = 1;
	m_Experience = 0;
	m_ExperienceToNextLevel = 100;
	m_StatPoints = 0;
	m_Firepower = 1;
	m_Agility = 1;
	m_Vitality = 1;
	m_PlayerMaxHealth = 100.0f;
	m_LevelUpFlash = 0.0f;
	m_FirstLevelSize = 45;
	m_LevelSeed = static_cast<unsigned int>(std::time(NULL));
	m_FirstLevelTiles.clear();
	if (!m_ModelLibrary.IsLoaded())
		m_ModelLibrary.Load();
	m_ShipPosition = {0.0f, 0.0f};
	m_PreviousShipPosition = m_ShipPosition;
	m_ShipVelocity = {0.0f, 0.0f};
	m_ShipAngle = Pi * 0.5f;
	m_ShipHealth = 100.0f;
	m_ShipImpactFlash = 0.0f;
	m_TransitionTimer = 0.0f;
	m_CurrentShip = 0;
	m_TravelDestinationShip = 1;
	m_Message = L"WASD: 이동    E: 상호작용";
	m_MessageTimer = 6.0f;

	m_Obstacles.clear();
	m_Obstacles.push_back({-7.1f, -7.2f, 0.65f, 0.55f, 42.0f, 0.16f, 0.25f, 0.30f, MedicalPod});
	m_Obstacles.push_back({-4.6f, -6.4f, 0.75f, 0.50f, 48.0f, 0.20f, 0.25f, 0.28f, ControlConsole});
	m_Obstacles.push_back({-1.8f, -3.6f, 0.55f, 0.85f, 52.0f, 0.19f, 0.23f, 0.28f, CoolantPipe});
	m_Obstacles.push_back({1.4f, -1.6f, 0.80f, 0.55f, 38.0f, 0.24f, 0.20f, 0.25f, CargoCrate});
	m_Obstacles.push_back({4.1f, 0.2f, 0.65f, 0.85f, 54.0f, 0.13f, 0.23f, 0.29f, PowerRelay});
	m_Obstacles.push_back({6.1f, 3.1f, 0.80f, 0.60f, 44.0f, 0.19f, 0.25f, 0.29f, CargoCrate});
	m_Obstacles.push_back({7.0f, 6.8f, 0.60f, 0.55f, 38.0f, 0.25f, 0.18f, 0.27f, PowerRelay});
	m_Obstacles.push_back({3.2f, 7.1f, 0.60f, 0.80f, 50.0f, 0.14f, 0.21f, 0.27f, EngineModule});
	m_Obstacles.push_back({-0.7f, 6.2f, 0.75f, 0.50f, 44.0f, 0.18f, 0.24f, 0.29f, ControlConsole});
	m_Obstacles.push_back({-4.2f, 5.8f, 0.55f, 0.85f, 49.0f, 0.14f, 0.23f, 0.30f, CoolantPipe});
	m_Obstacles.push_back({-7.1f, 4.0f, 0.80f, 0.55f, 45.0f, 0.17f, 0.25f, 0.31f, CargoCrate});
	m_Obstacles.push_back({-2.6f, 1.0f, 0.65f, 0.65f, 40.0f, 0.21f, 0.22f, 0.27f, MedicalPod});

	m_Enemies.clear();
	m_Enemies.push_back({{9.2f, 7.2f}, 2, false});
	m_Enemies.push_back({{7.3f, 9.4f}, 2, false});
	m_Enemies.push_back({{9.7f, 9.2f}, 2, false});
	m_Enemies.push_back({{6.8f, 7.8f}, 2, false});

	m_NeutralNpcs.clear();
	m_NeutralNpcs.push_back({{-6.5f, -3.0f}, true, 0.0f, -1, false});
	m_NeutralNpcs.push_back({{2.8f, 2.4f}, true, 1.7f, -1, false});
	m_NeutralNpcs.push_back({{-7.2f, 6.1f}, false, 0.8f, 0, false});
	m_NeutralNpcs.push_back({{4.8f, -5.8f}, false, 2.4f, 1, false});

	m_Asteroids.clear();
	m_Asteroids.push_back({{-135.0f, 125.0f}, {31.0f, -8.0f}, 25.0f, 0.2f, 0.45f});
	m_Asteroids.push_back({{150.0f, 235.0f}, {-38.0f, -3.0f}, 31.0f, 1.1f, -0.32f});
	m_Asteroids.push_back({{-175.0f, 355.0f}, {43.0f, -11.0f}, 22.0f, 2.2f, 0.60f});
	m_Asteroids.push_back({{145.0f, 455.0f}, {-34.0f, -7.0f}, 28.0f, 0.7f, -0.48f});
	m_Asteroids.push_back({{-120.0f, 565.0f}, {29.0f, -13.0f}, 20.0f, 1.8f, 0.72f});
}

void Game::Resize(int width, int height)
{
	m_Renderer->Resize(width, height);
}

void Game::KeyDown(unsigned char key)
{
	if (!m_Keys[key])
		m_KeyPressed[key] = true;
	m_Keys[key] = true;
}

void Game::KeyUp(unsigned char key)
{
	m_Keys[key] = false;
}

void Game::SpecialDown(int key)
{
	if (key >= 0 && key < 512)
		m_SpecialKeys[key] = true;
}

void Game::SpecialUp(int key)
{
	if (key >= 0 && key < 512)
		m_SpecialKeys[key] = false;
}

void Game::MouseMove(int x, int y)
{
	m_MouseX = x;
	m_MouseY = y;
}

void Game::MouseButton(int button, int state)
{
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
		m_MousePressed = true;
}

void Game::Update(float deltaSeconds)
{
	deltaSeconds = Clamp(deltaSeconds, 0.0f, 0.05f);
	if (m_KeyPressed[27])
		m_Paused = !m_Paused;
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
		m_LevelUpFlash = std::max(0.0f, m_LevelUpFlash - deltaSeconds);
		m_MessageTimer = std::max(0.0f, m_MessageTimer - deltaSeconds);
		if (m_ReloadTimer > 0.0f)
		{
			m_ReloadTimer -= deltaSeconds;
			if (m_ReloadTimer <= 0.0f)
			{
				if (m_Mode == FirstLevel)
				{
					if (m_SpareMagazines > 0 && m_AmmoInMagazine < 12)
					{
						m_AmmoInMagazine = 12;
						--m_SpareMagazines;
						SetMessage(L"탄창 교체 완료.", 1.2f);
					}
				}
				else
				{
					m_WeaponEnergy = 100.0f;
					SetMessage(L"펄스 카빈 충전 완료.", 1.2f);
				}
			}
		}

		if (m_Mode == OnFoot)
			UpdateOnFoot(deltaSeconds);
		else if (m_Mode == ShipControl)
			UpdateShip(deltaSeconds);
		else if (m_Mode == Transition)
		{
			m_TransitionTimer += deltaSeconds;
			if (m_TransitionTimer >= 1.4f)
			{
				m_CurrentShip = m_TravelDestinationShip;
				m_ShipPosition = {0.0f, 0.0f};
				m_PreviousShipPosition = m_ShipPosition;
				m_ShipVelocity = {0.0f, 0.0f};

				if (m_CurrentShip == 1 && m_Quest == FlyToSignal)
				{
					m_Mode = FirstLevel;
					m_Quest = CollectLevelWeapon;
					GenerateFirstLevel();
					SetMessage(L"첫 레벨: 훈련용 펄스 소총을 회수하십시오. [E]", 4.0f);
				}
				else
				{
					m_Mode = OnFoot;
					m_Player = {-9.0f, 8.0f};
					SetMessage(m_CurrentShip == 1 ? L"난민 수송선 에레보스에 도킹했습니다."
												  : L"연구선 노크티스로 귀환했습니다.",
						3.0f);
				}
			}
		}
		else if (m_Mode == FirstLevel)
			UpdateFirstLevel(deltaSeconds);
	}

	std::memset(m_KeyPressed, 0, sizeof(m_KeyPressed));
	m_MousePressed = false;
}

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
	UpdateEnemies(deltaSeconds);
	UpdateProjectiles(deltaSeconds);
	if (m_KeyPressed['e'] || m_KeyPressed['E'])
		HandleInteraction();
	if ((m_KeyPressed['r'] || m_KeyPressed['R']) && m_Quest != InspectTerminal &&
		m_ReloadTimer <= 0.0f)
	{
		m_ReloadTimer = 0.8f;
		SetMessage(L"펄스 카빈을 충전하고 있습니다...", 1.0f);
	}
}

void Game::UpdateFirstLevel(float deltaSeconds)
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
			m_DodgeCooldown = std::max(0.42f, 0.75f - static_cast<float>(m_Agility - 1) * 0.06f);
		}

		float baseSpeed = 4.2f + static_cast<float>(m_Agility - 1) * 0.22f;
		float speed = m_DodgeTimer > 0.0f ? baseSpeed * 2.25f : baseSpeed;
		float nextX = m_Player.x + direction.x * speed * deltaSeconds;
		float nextY = m_Player.y + direction.y * speed * deltaSeconds;

		if (IsFirstLevelWalkable(nextX, m_Player.y))
			m_Player.x = nextX;
		if (IsFirstLevelWalkable(m_Player.x, nextY))
			m_Player.y = nextY;
	}

	if ((m_KeyPressed['e'] || m_KeyPressed['E']) && !m_LevelWeaponCollected &&
		IsNear(FirstLevelWeaponPosition, 1.2f))
	{
		m_LevelWeaponCollected = true;
		m_Quest = EarnExperience;
		SetMessage(L"훈련용 펄스 소총 획득: 12발 탄창, 예비 탄창 3개. 마우스로 사격합니다.", 4.0f);
	}
	else if (m_KeyPressed['e'] || m_KeyPressed['E'])
	{
		bool interactionHandled = false;
		for (size_t index = 0; index < m_MagazinePickups.size(); ++index)
		{
			MagazinePickup& magazine = m_MagazinePickups[index];
			if (magazine.collected || !IsNear(magazine.position, 1.1f))
				continue;

			magazine.collected = true;
			++m_SpareMagazines;
			interactionHandled = true;
			SetMessage(L"예비 탄창을 획득했습니다. 탄창 보유 수 +1", 2.0f);
			break;
		}
		for (size_t index = 0; index < m_StoryDocuments.size(); ++index)
		{
			if (interactionHandled)
				break;
			StoryDocument& document = m_StoryDocuments[index];
			if (!IsNear(document.position, 1.1f))
				continue;

			document.read = true;
			if (document.documentId == 0)
			{
				SetMessage(
					L"작전 기록: 지휘부는 민간인 대피가 끝나기 전에 이 구역을 봉쇄했다.", 4.5f);
			}
			else if (document.documentId == 1)
			{
				SetMessage(
					L"의무관 기록: 공허종 정찰체 하나가 무기를 버린 부상자를 지나쳤다.", 4.5f);
			}
			else
			{
				SetMessage(
					L"찢어진 명령서: '모든 외계 신호는 기만이다.' 발령 시각은 첫 공격보다 이르다.",
					4.5f);
			}
			interactionHandled = true;
			break;
		}

		for (size_t index = 0; index < m_LevelSurvivors.size() && !interactionHandled; ++index)
		{
			LevelSurvivor& survivor = m_LevelSurvivors[index];
			if (!IsNear(survivor.position, 1.3f))
				continue;

			if (survivor.survivorId == 0)
			{
				SetMessage(survivor.talked ? L"경비대원 미라: 뒤에서도 인간의 총성이 들렸어요. "
											 L"누가 누구를 쐈는지 모르겠습니다."
										   : L"경비대원 미라: 격벽을 닫으라는 명령을 따랐어요. "
											 L"아직 통로에 사람들이 있었는데...",
					5.0f);
			}
			else if (survivor.survivorId == 1)
			{
				SetMessage(survivor.talked ? L"정비기사 준: 동력실 우회 통로는 살아 있습니다. "
											 L"공허종도 그 길을 쓰는 것 같아요."
										   : L"정비기사 준: 놈이 문을 부쉈지만 비무장 작업자들은 "
											 L"지나쳤습니다. 이유는 모르겠어요.",
					5.0f);
			}
			else if (survivor.survivorId == 2)
			{
				SetMessage(survivor.talked ? L"난민 한: 구조선이 오면 군인보다 먼저 아이들을 태워 "
											 L"주세요. 약속해 주세요."
										   : L"난민 한: 방송은 이 방이 안전하다고 했습니다. 문이 "
											 L"잠긴 뒤에야 거짓말인 걸 알았어요.",
					5.0f);
			}
			else
			{
				SetMessage(survivor.talked ? L"의무관 일리안: 인간도 공허종도 같은 통로에서 "
											 L"죽었습니다. 전쟁은 구분하지 않더군요."
										   : L"의무관 일리안: 탄환보다 감염 공포가 더 많은 사람을 "
											 L"죽였어요. 서로를 먼저 의심했죠.",
					5.0f);
			}
			survivor.talked = true;
		}
	}

	if (m_MousePressed)
		HandleAttack();

	UpdateEnemies(deltaSeconds);
	UpdateProjectiles(deltaSeconds);
	ApplyStatInput();

	if ((m_KeyPressed['r'] || m_KeyPressed['R']) && m_LevelWeaponCollected && m_ReloadTimer <= 0.0f)
	{
		if (m_AmmoInMagazine >= 12)
			SetMessage(L"현재 탄창이 이미 가득 차 있습니다.", 1.4f);
		else if (m_SpareMagazines <= 0)
			SetMessage(L"예비 탄창이 없습니다. 구역을 탐색해 탄창을 확보하십시오.", 2.0f);
		else
		{
			m_ReloadTimer = std::max(0.45f, 1.05f - static_cast<float>(m_Agility - 1) * 0.08f);
			SetMessage(L"새 탄창을 장전하고 있습니다...", m_ReloadTimer);
		}
	}

	for (size_t index = 0; index < m_ExperienceOrbs.size();)
	{
		m_ExperienceOrbs[index].life -= deltaSeconds;
		if (m_ExperienceOrbs[index].life <= 0.0f)
			m_ExperienceOrbs.erase(m_ExperienceOrbs.begin() + index);
		else
			++index;
	}
}

void Game::GenerateFirstLevel()
{
	struct Room
	{
		int centerX;
		int centerY;
		int halfWidth;
		int halfHeight;
	};

	m_FirstLevelTiles.assign(m_FirstLevelSize * m_FirstLevelSize, 0);
	int halfSize = m_FirstLevelSize / 2;
	std::mt19937 random(m_LevelSeed);
	std::uniform_int_distribution<int> roomJitter(-2, 2);
	std::uniform_int_distribution<int> roomRadius(3, 5);

	const WorldPoint roomAnchors[] = {
		{0.0f, 0.0f},
		{-12.0f, 0.0f},
		{12.0f, 0.0f},
		{-12.0f, -13.0f},
		{12.0f, -13.0f},
		{-12.0f, 13.0f},
		{12.0f, 13.0f},
	};

	std::vector<Room> rooms;
	for (int roomIndex = 0; roomIndex < 7; ++roomIndex)
	{
		int jitterX = roomIndex == 0 ? 0 : roomJitter(random);
		int jitterY = roomIndex == 0 ? 0 : roomJitter(random);
		Room room = {
			halfSize + static_cast<int>(roomAnchors[roomIndex].x) + jitterX,
			halfSize + static_cast<int>(roomAnchors[roomIndex].y) + jitterY,
			roomRadius(random),
			roomRadius(random),
		};
		rooms.push_back(room);

		for (int y = room.centerY - room.halfHeight; y <= room.centerY + room.halfHeight; ++y)
		{
			for (int x = room.centerX - room.halfWidth; x <= room.centerX + room.halfWidth; ++x)
			{
				if (x > 0 && x < m_FirstLevelSize - 1 && y > 0 && y < m_FirstLevelSize - 1)
					m_FirstLevelTiles[y * m_FirstLevelSize + x] = 2;
			}
		}
	}

	auto carveTile = [this](int x, int y)
	{
		for (int offsetY = -1; offsetY <= 1; ++offsetY)
		{
			for (int offsetX = -1; offsetX <= 1; ++offsetX)
			{
				int tileX = x + offsetX;
				int tileY = y + offsetY;
				if (tileX > 0 && tileX < m_FirstLevelSize - 1 && tileY > 0 &&
					tileY < m_FirstLevelSize - 1 &&
					m_FirstLevelTiles[tileY * m_FirstLevelSize + tileX] == 0)
				{
					m_FirstLevelTiles[tileY * m_FirstLevelSize + tileX] = 1;
				}
			}
		}
	};

	std::vector<WorldPoint> corridorPatrols;
	auto connectRooms = [&random, &rooms, &carveTile, &corridorPatrols, halfSize](
							int firstRoom, int secondRoom)
	{
		int x = rooms[firstRoom].centerX;
		int y = rooms[firstRoom].centerY;
		int targetX = rooms[secondRoom].centerX;
		int targetY = rooms[secondRoom].centerY;
		bool horizontalFirst = (random() & 1u) == 0u;
		std::vector<WorldPoint> corridorPath;

		auto carveHorizontal = [&]()
		{
			while (x != targetX)
			{
				carveTile(x, y);
				corridorPath.push_back(
					{static_cast<float>(x - halfSize), static_cast<float>(y - halfSize)});
				x += x < targetX ? 1 : -1;
			}
		};

		auto carveVertical = [&]()
		{
			while (y != targetY)
			{
				carveTile(x, y);
				corridorPath.push_back(
					{static_cast<float>(x - halfSize), static_cast<float>(y - halfSize)});
				y += y < targetY ? 1 : -1;
			}
		};

		if (horizontalFirst)
		{
			carveHorizontal();
			carveVertical();
		}
		else
		{
			carveVertical();
			carveHorizontal();
		}
		carveTile(targetX, targetY);
		if (!corridorPath.empty())
			corridorPatrols.push_back(corridorPath[corridorPath.size() / 2]);
	};

	const int connections[][2] = {
		{0, 1},
		{0, 2},
		{1, 3},
		{1, 5},
		{2, 4},
		{2, 6},
		{5, 6},
	};
	for (int connectionIndex = 0; connectionIndex < 7; ++connectionIndex)
		connectRooms(connections[connectionIndex][0], connections[connectionIndex][1]);

	auto toWorld = [halfSize](const Room& room, float offsetX, float offsetY)
	{
		return WorldPoint{static_cast<float>(room.centerX - halfSize) + offsetX,
			static_cast<float>(room.centerY - halfSize) + offsetY};
	};

	m_Player = toWorld(rooms[0], 0.0f, 0.0f);
	m_LevelCorpses.clear();
	m_LevelCorpses.push_back(toWorld(rooms[1], 1.5f, -1.0f));
	m_LevelCorpses.push_back(toWorld(rooms[2], -1.5f, 1.0f));
	m_LevelCorpses.push_back(toWorld(rooms[3], 1.0f, 1.5f));
	m_LevelCorpses.push_back(toWorld(rooms[4], -1.0f, -1.5f));
	m_LevelCorpses.push_back(toWorld(rooms[6], 1.5f, 1.0f));
	m_StoryDocuments.clear();
	m_StoryDocuments.push_back({toWorld(rooms[1], 2.0f, -0.5f), 0, false});
	m_StoryDocuments.push_back({toWorld(rooms[3], -1.5f, 1.0f), 1, false});
	m_StoryDocuments.push_back({toWorld(rooms[6], -1.5f, 1.5f), 2, false});
	m_LevelSurvivors.clear();
	m_LevelSurvivors.push_back({toWorld(rooms[3], 0.0f, -1.5f), 0, false});
	m_LevelSurvivors.push_back({toWorld(rooms[4], 1.5f, 0.0f), 1, false});
	m_LevelSurvivors.push_back({toWorld(rooms[5], -1.5f, 0.0f), 2, false});
	m_LevelSurvivors.push_back({toWorld(rooms[6], 0.0f, -1.5f), 3, false});
	m_MagazinePickups.clear();
	m_MagazinePickups.push_back({toWorld(rooms[1], -2.0f, 1.5f), false});
	m_MagazinePickups.push_back({toWorld(rooms[2], 2.0f, -1.5f), false});
	m_MagazinePickups.push_back({toWorld(rooms[3], 2.0f, -1.5f), false});
	m_MagazinePickups.push_back({toWorld(rooms[5], 1.5f, 1.5f), false});
	m_MagazinePickups.push_back({toWorld(rooms[6], -2.0f, -1.5f), false});

	m_Enemies.clear();
	const WorldPoint enemyOffsets[] = {{-2.0f, -1.5f}, {2.0f, 1.0f}, {0.0f, 2.0f}};
	for (int roomIndex = 1; roomIndex < 7; ++roomIndex)
	{
		int enemyCount = roomIndex == 5 ? 2 : 3;
		for (int enemyIndex = 0; enemyIndex < enemyCount; ++enemyIndex)
		{
			WorldPoint position =
				toWorld(rooms[roomIndex], enemyOffsets[enemyIndex].x, enemyOffsets[enemyIndex].y);
			m_Enemies.push_back({position, 3, true});
		}
	}
	for (size_t patrolIndex = 0; patrolIndex < corridorPatrols.size(); patrolIndex += 2)
		m_Enemies.push_back({corridorPatrols[patrolIndex], 3, true});
}

bool Game::IsFirstLevelConnected(const std::vector<int>& tiles) const
{
	int startIndex = -1;
	int walkableCount = 0;
	for (int index = 0; index < static_cast<int>(tiles.size()); ++index)
	{
		if (tiles[index] > 0)
		{
			++walkableCount;
			if (startIndex < 0)
				startIndex = index;
		}
	}

	if (startIndex < 0)
		return false;

	std::vector<bool> visited(tiles.size(), false);
	std::queue<int> openTiles;
	openTiles.push(startIndex);
	visited[startIndex] = true;
	int visitedCount = 0;
	const int directions[] = {-1, 1, -m_FirstLevelSize, m_FirstLevelSize};

	while (!openTiles.empty())
	{
		int current = openTiles.front();
		openTiles.pop();
		++visitedCount;
		int currentX = current % m_FirstLevelSize;

		for (int directionIndex = 0; directionIndex < 4; ++directionIndex)
		{
			int next = current + directions[directionIndex];
			if (next < 0 || next >= static_cast<int>(tiles.size()))
				continue;
			if ((directionIndex == 0 && currentX == 0) ||
				(directionIndex == 1 && currentX == m_FirstLevelSize - 1))
				continue;
			if (tiles[next] > 0 && !visited[next])
			{
				visited[next] = true;
				openTiles.push(next);
			}
		}
	}

	return visitedCount == walkableCount;
}

bool Game::IsFirstLevelWalkable(float x, float y) const
{
	int halfSize = m_FirstLevelSize / 2;
	int tileX = static_cast<int>(std::floor(x + 0.5f)) + halfSize;
	int tileY = static_cast<int>(std::floor(y + 0.5f)) + halfSize;

	if (tileX < 0 || tileX >= m_FirstLevelSize || tileY < 0 || tileY >= m_FirstLevelSize)
		return false;

	return m_FirstLevelTiles[tileY * m_FirstLevelSize + tileX] > 0;
}

void Game::AwardExperience(int amount, const WorldPoint& position)
{
	m_Experience += amount;
	m_ExperienceOrbs.push_back({position, 1.2f});

	std::wostringstream message;
	message << L"공허종 정찰체 제거: 경험치 +" << amount;
	SetMessage(message.str(), 1.6f);

	while (m_Experience >= m_ExperienceToNextLevel)
	{
		m_Experience -= m_ExperienceToNextLevel;
		++m_PlayerLevel;
		m_ExperienceToNextLevel = 100 + (m_PlayerLevel - 1) * 50;
		m_StatPoints += 3;
		m_LevelUpFlash = 3.2f;
		m_Quest = AllocateStats;
		SetMessage(L"레벨 상승! 1: 화력, 2: 기동, 3: 생존에 스탯 포인트를 배분하십시오.", 5.0f);
	}
}

void Game::ApplyStatInput()
{
	if (m_StatPoints <= 0)
		return;

	if (m_KeyPressed['1'])
	{
		++m_Firepower;
		--m_StatPoints;
		SetMessage(L"화력 증가: 탄환 피해량이 상승합니다.", 1.5f);
	}
	else if (m_KeyPressed['2'])
	{
		++m_Agility;
		--m_StatPoints;
		SetMessage(L"기동 증가: 이동, 연사, 재장전, 회피 성능이 상승합니다.", 1.5f);
	}
	else if (m_KeyPressed['3'])
	{
		++m_Vitality;
		--m_StatPoints;
		m_PlayerMaxHealth += 15.0f;
		m_PlayerHealth = std::min(m_PlayerMaxHealth, m_PlayerHealth + 15.0f);
		SetMessage(L"생존 증가: 최대 전투복 내구도가 상승합니다.", 1.5f);
	}

	if (m_StatPoints == 0 && m_Quest == AllocateStats)
	{
		m_Quest = FirstLevelComplete;
		SetMessage(L"첫 레벨 완료: 획득한 경험치로 전투 능력을 조정했습니다.", 4.0f);
	}
}

void Game::UpdateEnemies(float deltaSeconds)
{
	for (size_t i = 0; i < m_Enemies.size(); ++i)
	{
		Enemy& enemy = m_Enemies[i];
		if (!enemy.active || enemy.health <= 0)
			continue;
		float dx = m_Player.x - enemy.position.x;
		float dy = m_Player.y - enemy.position.y;
		float distance = Length(dx, dy);
		if (m_Mode == FirstLevel && distance > 7.0f)
			continue;
		if (distance > 0.65f)
		{
			float nextX = enemy.position.x + dx / distance * 1.15f * deltaSeconds;
			float nextY = enemy.position.y + dy / distance * 1.15f * deltaSeconds;
			if (m_Mode != FirstLevel || IsFirstLevelWalkable(nextX, enemy.position.y))
				enemy.position.x = nextX;
			if (m_Mode != FirstLevel || IsFirstLevelWalkable(enemy.position.x, nextY))
				enemy.position.y = nextY;
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
		if (m_Mode == FirstLevel)
		{
			m_Player = {0.0f, 0.0f};
			m_PlayerHealth = m_PlayerMaxHealth;
			SetMessage(L"훈련 구역의 의료 비콘이 시작 지점에서 전투복을 복구했습니다.", 2.5f);
		}
		else
		{
			m_Player = {5.0f, 5.0f};
			m_PlayerHealth = 100.0f;
			for (size_t i = 0; i < m_Enemies.size(); ++i)
			{
				m_Enemies[i].health = 2;
				m_Enemies[i].active = true;
			}
			SetMessage(L"의료 백업이 마지막 지점을 복구했습니다.", 2.5f);
		}
	}
}

void Game::HandleAttack()
{
	if (m_Quest == InspectTerminal || m_AttackCooldown > 0.0f || m_ReloadTimer > 0.0f ||
		(m_Mode == FirstLevel && !m_LevelWeaponCollected))
		return;

	if (m_Mode == FirstLevel && m_AmmoInMagazine <= 0)
	{
		SetMessage(m_SpareMagazines > 0
					   ? L"탄창이 비었습니다. R 키로 재장전하십시오."
					   : L"탄창과 예비 탄창이 모두 비었습니다. 탄창을 탐색하십시오.",
			1.8f);
		return;
	}

	if (m_Mode != FirstLevel && m_WeaponEnergy < 10.0f)
	{
		SetMessage(L"무기 에너지가 부족합니다. R 키로 충전하십시오.", 1.5f);
		return;
	}
	if (m_Mode == FirstLevel)
		--m_AmmoInMagazine;
	else
		m_WeaponEnergy -= 10.0f;

	m_AttackCooldown = std::max(0.09f, 0.22f - static_cast<float>(m_Agility - 1) * 0.018f);
	m_AttackFlash = 0.10f;
	float aimX = static_cast<float>(m_MouseX - m_Renderer->Width() / 2);
	float aimY = static_cast<float>(m_Renderer->Height() / 2 - m_MouseY + 30);
	float aimLength = Length(aimX, aimY);
	if (aimLength < 1.0f)
	{
		aimX = 1.0f;
		aimY = 0.0f;
		aimLength = 1.0f;
	}
	aimX /= aimLength;
	aimY /= aimLength;

	WorldPoint worldDirection = {aimX / 64.0f - aimY / 32.0f, -aimX / 64.0f - aimY / 32.0f};
	float worldLength = Length(worldDirection.x, worldDirection.y);
	if (worldLength > 0.0f)
	{
		worldDirection.x /= worldLength;
		worldDirection.y /= worldLength;
	}
	float bulletSpeed = 11.0f + static_cast<float>(m_Agility - 1) * 0.8f;
	int bulletDamage = 1 + (m_Firepower - 1) / 2;
	++m_ShotsFired;
	bool chargedBullet = m_Mode == FirstLevel && m_ShotsFired % 4 == 0;
	if (chargedBullet)
		++bulletDamage;
	m_Projectiles.push_back({m_Player,
		m_Player,
		{worldDirection.x * bulletSpeed, worldDirection.y * bulletSpeed},
		1.4f,
		0.0f,
		14.0f,
		bulletDamage,
		chargedBullet});
}

void Game::UpdateProjectiles(float deltaSeconds)
{
	for (size_t projectileIndex = 0; projectileIndex < m_Projectiles.size();)
	{
		Projectile& projectile = m_Projectiles[projectileIndex];
		projectile.previousPosition = projectile.position;
		float movementX = projectile.velocity.x * deltaSeconds;
		float movementY = projectile.velocity.y * deltaSeconds;
		projectile.position.x += movementX;
		projectile.position.y += movementY;
		projectile.distanceTravelled += Length(movementX, movementY);
		projectile.life -= deltaSeconds;
		bool remove =
			projectile.life <= 0.0f || projectile.distanceTravelled >= projectile.maximumDistance;

		if (m_Mode == FirstLevel &&
			!IsFirstLevelWalkable(projectile.position.x, projectile.position.y))
			remove = true;

		for (size_t enemyIndex = 0; enemyIndex < m_Enemies.size() && !remove; ++enemyIndex)
		{
			Enemy& enemy = m_Enemies[enemyIndex];
			if (enemy.active && enemy.health > 0 &&
				Length(projectile.position.x - enemy.position.x,
					projectile.position.y - enemy.position.y) < 0.55f)
			{
				enemy.health -= projectile.damage;
				remove = true;
				if (enemy.health <= 0)
				{
					enemy.active = false;
					if (m_Mode == FirstLevel)
						AwardExperience(25, enemy.position);
				}
				if (m_Mode != FirstLevel && AllEnemiesDefeated())
					SetMessage(L"구역이 확보되었습니다. 항법 코어를 회수하십시오.", 2.5f);
			}
		}
		if (remove)
			m_Projectiles.erase(m_Projectiles.begin() + projectileIndex);
		else
			++projectileIndex;
	}
}

void Game::HandleInteraction()
{
	for (size_t i = 0; i < m_NeutralNpcs.size(); ++i)
	{
		NeutralNpc& npc = m_NeutralNpcs[i];
		if (npc.dead || !IsNear(npc.position, 1.25f))
			continue;
		if (npc.dialogueId == 0)
		{
			SetMessage(npc.talked ? L"통신장교 서윤: 공허종의 신호에도 구조 요청이 섞여 있었어요. "
									L"지휘부는 그 부분을 지웠습니다."
								  : L"통신장교 서윤: 우리가 먼저 그 문을 열었어요... 그런데 모두 "
									L"적이 먼저 공격했다고 믿고 있어요.",
				5.0f);
		}
		else
		{
			SetMessage(npc.talked ? L"정비사 라울: 살아남으면 진실을 전해 주세요. 복수만으로는 이 "
									L"함선을 다시 움직일 수 없습니다."
								  : L"정비사 라울: 냉각관이 터질 때 동료들을 두고 도망쳤습니다... "
									L"아직도 그 소리가 들립니다.",
				5.0f);
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
		SetMessage(
			m_TravelDestinationShip == 0 ? L"항로: 연구선 노크티스" : L"항로: 난민 수송선 에레보스",
			2.5f);
	}
}

void Game::UpdateShip(float deltaSeconds)
{
	m_PreviousShipPosition = m_ShipPosition;
	if (m_SpecialKeys[GLUT_KEY_LEFT])
		m_ShipAngle += 2.1f * deltaSeconds;
	if (m_SpecialKeys[GLUT_KEY_RIGHT])
		m_ShipAngle -= 2.1f * deltaSeconds;
	float thrust = 0.0f;
	if (m_SpecialKeys[GLUT_KEY_UP])
		thrust += 95.0f;
	if (m_SpecialKeys[GLUT_KEY_DOWN])
		thrust -= 48.0f;
	if (m_Keys[' '])
		thrust *= 1.8f;
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

	const WorldPoint debris[] = {
		{-90.0f, 170.0f}, {85.0f, 290.0f}, {-70.0f, 410.0f}, {55.0f, 520.0f}};
	for (int i = 0; i < 4; ++i)
	{
		float distance = DistanceToSegment(debris[i], m_PreviousShipPosition, m_ShipPosition);
		if (distance < 34.0f && m_DamageCooldown <= 0.0f)
		{
			m_ShipVelocity.x *= -0.4f;
			m_ShipVelocity.y *= -0.4f;
			m_ShipHealth = std::max(20.0f, m_ShipHealth - 15.0f);
			m_DamageCooldown = 0.8f;
			m_ShipImpactFlash = 0.35f;
			SetMessage(L"선체가 충돌했습니다. 항로를 수정하십시오.", 1.5f);
		}
	}
	float destinationDistance =
		Length(m_ShipPosition.x - DestinationPosition.x, m_ShipPosition.y - DestinationPosition.y);
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
		WorldPoint previousAsteroidPosition = asteroid.position;
		asteroid.position.x += asteroid.velocity.x * deltaSeconds;
		asteroid.position.y += asteroid.velocity.y * deltaSeconds;
		asteroid.rotation += asteroid.rotationSpeed * deltaSeconds;

		if (asteroid.position.x < -210.0f || asteroid.position.x > 210.0f)
			asteroid.velocity.x *= -1.0f;
		bool wrapped = false;
		if (asteroid.position.y < 45.0f)
		{
			asteroid.position.y += 560.0f;
			wrapped = true;
		}
		if (asteroid.position.y > 625.0f)
		{
			asteroid.position.y -= 560.0f;
			wrapped = true;
		}
		if (wrapped)
			previousAsteroidPosition = asteroid.position;

		float dx = m_ShipPosition.x - asteroid.position.x;
		float dy = m_ShipPosition.y - asteroid.position.y;
		float finalDistance = Length(dx, dy);
		WorldPoint relativeStart = {m_PreviousShipPosition.x - previousAsteroidPosition.x,
			m_PreviousShipPosition.y - previousAsteroidPosition.y};
		WorldPoint relativeEnd = {dx, dy};
		const WorldPoint relativeOrigin = {0.0f, 0.0f};
		float sweptDistance = DistanceToSegment(relativeOrigin, relativeStart, relativeEnd);
		if (sweptDistance < asteroid.radius + 22.0f && m_DamageCooldown <= 0.0f)
		{
			float normalX = finalDistance > 0.01f ? dx / finalDistance : 1.0f;
			float normalY = finalDistance > 0.01f ? dy / finalDistance : 0.0f;
			float separation = std::max(3.0f, asteroid.radius + 22.0f - finalDistance);
			m_ShipPosition.x += normalX * separation;
			m_ShipPosition.y += normalY * separation;
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
		m_ShipPosition = {0.0f, std::max(0.0f, m_ShipPosition.y - 90.0f)};
		m_PreviousShipPosition = m_ShipPosition;
		m_ShipVelocity = {0.0f, 0.0f};
		SetMessage(L"비상 자동 항법이 작동했습니다. 선체 내구도 35%로 복구합니다.", 3.0f);
	}
}

bool Game::CanMoveTo(float x, float y) const
{
	if (x < -11.7f || x > 11.7f || y < -11.7f || y > 11.7f)
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

bool Game::IsNear(const WorldPoint& point, float distance) const
{
	return Length(m_Player.x - point.x, m_Player.y - point.y) <= distance;
}

bool Game::AllEnemiesDefeated() const
{
	for (size_t i = 0; i < m_Enemies.size(); ++i)
		if (m_Enemies[i].health > 0)
			return false;
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
	ScreenPoint point = {(x - y) * TileWidth * 0.5f - cameraX,
		-(x + y) * TileHeight * 0.5f - cameraY - 30.0f + height};
	return point;
}

std::wstring Game::ObjectiveText() const
{
	if (m_Mode == FirstLevel)
	{
		if (m_Quest == CollectLevelWeapon)
			return L"첫 레벨: 훈련용 펄스 소총을 회수하십시오 [E]";
		if (m_Quest == EarnExperience)
			return L"첫 레벨: 공허종 정찰체를 제거해 레벨 2에 도달하십시오";
		if (m_Quest == AllocateStats)
			return L"첫 레벨: 스탯 배분  [1] 화력  [2] 기동  [3] 생존";
		return L"첫 레벨 완료: 경험치 획득과 능력 조정 훈련 완료";
	}

	if ((m_Mode == ShipControl || m_Mode == Transition) && m_Quest == QuestComplete)
		return m_TravelDestinationShip == 0 ? L"목표: 연구선 노크티스에 도킹하십시오"
											: L"목표: 난민 수송선 에레보스에 도킹하십시오";
	if (m_Quest == InspectTerminal)
		return L"목표: 의료실 단말기를 조사하십시오 [E]";
	if (m_Quest == FindCore && !AllEnemiesDefeated())
		return L"목표: 기관실로 이동하여 침입체를 제거하십시오";
	if (m_Quest == FindCore)
		return L"목표: 항법 코어를 회수하십시오 [E]";
	if (m_Quest == ReturnToCockpit)
		return L"목표: 조종석으로 돌아가 항법 코어를 설치하십시오 [E]";
	if (m_Quest == FlyToSignal)
		return L"목표: 노크티스를 조종하여 구조 신호로 이동하십시오";
	return m_CurrentShip == 1 ? L"현재 위치: 난민 수송선 에레보스 | 조종석에서 노크티스로 귀환 가능"
							  : L"현재 위치: 연구선 노크티스 | 조종석에서 에레보스로 이동 가능";
}

std::wstring Game::InteractionText() const
{
	if (m_Mode == ShipControl)
	{
		float distance = Length(
			m_ShipPosition.x - DestinationPosition.x, m_ShipPosition.y - DestinationPosition.y);
		if (distance >= 85.0f)
			return L"";
		if (m_Quest == FlyToSignal)
			return L"[E] 구조 신호에 진입";
		return m_TravelDestinationShip == 0 ? L"[E] 노크티스에 도킹" : L"[E] 에레보스에 도킹";
	}
	if (m_Mode == FirstLevel && !m_LevelWeaponCollected && IsNear(FirstLevelWeaponPosition, 1.2f))
		return L"[E] 훈련용 펄스 소총 회수";
	if (m_Mode == FirstLevel)
	{
		for (size_t index = 0; index < m_MagazinePickups.size(); ++index)
		{
			if (!m_MagazinePickups[index].collected &&
				IsNear(m_MagazinePickups[index].position, 1.1f))
				return L"[E] 예비 탄창 획득";
		}
		for (size_t index = 0; index < m_StoryDocuments.size(); ++index)
		{
			if (IsNear(m_StoryDocuments[index].position, 1.1f))
				return m_StoryDocuments[index].read ? L"[E] 문서 다시 읽기" : L"[E] 현장 문서 조사";
		}
		for (size_t index = 0; index < m_LevelSurvivors.size(); ++index)
		{
			if (IsNear(m_LevelSurvivors[index].position, 1.3f))
				return L"[E] 생존자와 대화";
		}
	}
	if (m_Mode != OnFoot)
		return L"";
	for (size_t i = 0; i < m_NeutralNpcs.size(); ++i)
	{
		const NeutralNpc& npc = m_NeutralNpcs[i];
		if (!npc.dead && IsNear(npc.position, 1.25f))
			return npc.dialogueId == 0 ? L"[E] 통신장교 서윤과 대화" : L"[E] 정비사 라울과 대화";
	}
	if (m_Quest == InspectTerminal && IsNear(TerminalPosition, 1.15f))
		return L"[E] 단말기 조사";
	if (m_CurrentShip == 0 && !m_RecordRead && IsNear(RecordPosition, 1.05f))
		return L"[E] 손상된 기록 읽기";
	if (m_CurrentShip == 0 && m_CoreRecovered && !m_SignalChecked && IsNear(SignalPosition, 1.05f))
		return L"[E] 구조 신호 조사";
	if (m_Quest == FindCore && AllEnemiesDefeated() && IsNear(CorePosition, 1.2f))
		return L"[E] 항법 코어 회수";
	if (m_Quest == ReturnToCockpit && IsNear(CockpitPosition, 1.25f))
		return L"[E] 코어 설치 및 우주선 조종";
	if (m_Quest == QuestComplete && IsNear(CockpitPosition, 1.25f))
		return m_CurrentShip == 1 ? L"[E] 노크티스로 귀환" : L"[E] 에레보스로 이동";
	return L"";
}

void Game::Render()
{
	if (m_Mode == ShipControl || m_Mode == Transition)
		RenderSpace();
	else if (m_Mode == FirstLevel)
		RenderFirstLevel();
	else if (m_Mode == Complete)
		RenderDestination();
	else
		RenderInterior();
	RenderInterface();
	m_Renderer->Present(m_TotalTime);
}

Actor* Game::CreateSceneActor(const std::string& name,
	float x,
	float y,
	float height,
	int layer,
	float sortOrder,
	const Actor::RenderFunction& renderFunction,
	Actor* parent)
{
	Actor* actor = m_SceneGraph.CreateActor(name, parent);
	ActorTransform& transform = actor->LocalTransform();
	transform.x = x;
	transform.y = y;
	transform.height = height;
	actor->SetLayer(layer);
	actor->SetSortOrder(sortOrder);
	actor->SetRenderFunction(renderFunction);
	return actor;
}

void Game::RenderInterior()
{
	m_SceneGraph.Clear();
	m_Renderer->BeginFrame(
		m_CurrentShip == 0 ? 0.012f : 0.018f, 0.020f, m_CurrentShip == 0 ? 0.035f : 0.045f, 1.0f);
	int width = m_Renderer->Width();
	int height = m_Renderer->Height();

	for (int x = -12; x <= 12; ++x)
	{
		for (int y = -12; y <= 12; ++y)
		{
			ScreenPoint p = WorldToScreen(static_cast<float>(x), static_cast<float>(y));
			float checker = ((x + y) & 1) ? 0.015f : 0.0f;
			float restored = m_CoreRecovered ? 0.035f : 0.0f;
			m_Renderer->DrawDiamond(p.x,
				p.y,
				TileWidth - 2.0f,
				TileHeight - 1.0f,
				0.075f + checker + (m_CurrentShip == 1 ? 0.025f : 0.0f),
				0.105f + restored,
				0.13f + restored + (m_CurrentShip == 1 ? 0.035f : 0.0f),
				1.0f);
		}
	}

	for (int i = -12; i <= 12; ++i)
	{
		Obstacle north = {
			static_cast<float>(i), -12.2f, 0.48f, 0.25f, 58.0f, 0.12f, 0.18f, 0.22f, HullWall};
		Obstacle south = {
			static_cast<float>(i), 12.2f, 0.48f, 0.25f, 58.0f, 0.10f, 0.16f, 0.21f, HullWall};
		Obstacle west = {
			-12.2f, static_cast<float>(i), 0.25f, 0.48f, 58.0f, 0.11f, 0.17f, 0.22f, HullWall};
		Obstacle east = {
			12.2f, static_cast<float>(i), 0.25f, 0.48f, 58.0f, 0.11f, 0.16f, 0.21f, HullWall};
		const Obstacle walls[] = {north, south, west, east};
		for (int wallIndex = 0; wallIndex < 4; ++wallIndex)
		{
			const Obstacle wall = walls[wallIndex];
			CreateSceneActor("HullWall",
				wall.x,
				wall.y,
				0.0f,
				1,
				wall.x + wall.y,
				[this, wall](const ActorTransform& transform)
				{
					Obstacle placedWall = wall;
					placedWall.x = transform.x;
					placedWall.y = transform.y;
					DrawWorldBlock(placedWall);
				});
		}
	}

	ScreenPoint infestationA = WorldToScreen(9.0f, 9.3f, 2.0f);
	ScreenPoint infestationB = WorldToScreen(6.1f, 7.7f, 1.0f);
	m_Renderer->DrawDiamond(
		infestationA.x, infestationA.y, 165.0f, 58.0f, 0.28f, 0.04f, 0.38f, 0.78f);
	m_Renderer->DrawDiamond(
		infestationB.x, infestationB.y, 125.0f, 42.0f, 0.10f, 0.28f, 0.33f, 0.62f);

	for (size_t i = 0; i < m_Obstacles.size(); ++i)
	{
		const Obstacle obstacle = m_Obstacles[i];
		CreateSceneActor("InteriorFixture",
			obstacle.x,
			obstacle.y,
			0.0f,
			1,
			obstacle.x + obstacle.y,
			[this, obstacle](const ActorTransform& transform)
			{
				Obstacle placedObstacle = obstacle;
				placedObstacle.x = transform.x;
				placedObstacle.y = transform.y;
				DrawWorldBlock(placedObstacle);
			});
	}
	CreateSceneActor("Player",
		m_Player.x,
		m_Player.y,
		0.0f,
		1,
		m_Player.x + m_Player.y,
		[this](const ActorTransform& transform)
		{
			WorldPoint position = {transform.x, transform.y};
			DrawCharacter(position, 0.20f, 0.72f, 0.86f, false);
		});
	for (size_t i = 0; i < m_Enemies.size(); ++i)
	{
		if (m_Enemies[i].health > 0 && m_Enemies[i].active)
		{
			const WorldPoint position = m_Enemies[i].position;
			CreateSceneActor("Enemy",
				position.x,
				position.y,
				0.0f,
				1,
				position.x + position.y,
				[this](const ActorTransform& transform)
				{
					WorldPoint placedPosition = {transform.x, transform.y};
					DrawCharacter(placedPosition, 0.62f, 0.08f, 0.72f, true);
				});
		}
	}
	for (size_t i = 0; i < m_NeutralNpcs.size(); ++i)
	{
		const NeutralNpc npc = m_NeutralNpcs[i];
		CreateSceneActor("NeutralNpc",
			npc.position.x,
			npc.position.y,
			0.0f,
			1,
			npc.position.x + npc.position.y,
			[this, npc](const ActorTransform& transform)
			{
				NeutralNpc placedNpc = npc;
				placedNpc.position = {transform.x, transform.y};
				DrawNeutralNpc(placedNpc);
			});
	}
	for (size_t i = 0; i < m_Projectiles.size(); ++i)
	{
		const WorldPoint position = m_Projectiles[i].position;
		CreateSceneActor("Projectile",
			position.x,
			position.y,
			29.0f,
			2,
			position.x + position.y,
			[this](const ActorTransform& transform)
			{
				ScreenPoint shot = WorldToScreen(transform.x, transform.y, transform.height);
				m_Renderer->DrawSoftShadow(shot.x, shot.y - 25.0f, 24.0f, 7.0f, 0.18f);
				m_Renderer->DrawDiamond(shot.x, shot.y, 25.0f, 8.0f, 0.18f, 0.92f, 1.0f, 1.0f);
			});
	}
	m_SceneGraph.Render();

	if (m_CurrentShip == 0)
	{
		ScreenPoint terminal = WorldToScreen(TerminalPosition.x, TerminalPosition.y, 22.0f);
		m_Renderer->DrawRect(terminal.x, terminal.y, 22.0f, 34.0f, 0.10f, 0.75f, 0.92f, 0.9f);
		ScreenPoint record = WorldToScreen(RecordPosition.x, RecordPosition.y, 5.0f);
		m_Renderer->DrawDiamond(
			record.x, record.y, 24.0f, 12.0f, m_RecordRead ? 0.18f : 0.75f, 0.58f, 0.18f, 0.95f);
		ScreenPoint core = WorldToScreen(CorePosition.x, CorePosition.y, 18.0f);
		if (!m_CoreRecovered)
			m_Renderer->DrawDiamond(core.x,
				core.y,
				34.0f,
				25.0f,
				0.10f,
				0.85f,
				0.94f,
				AllEnemiesDefeated() ? 1.0f : 0.45f);
	}
	ScreenPoint cockpit = WorldToScreen(CockpitPosition.x, CockpitPosition.y, 24.0f);
	m_Renderer->DrawRect(cockpit.x,
		cockpit.y,
		55.0f,
		36.0f,
		m_CoreRecovered ? 0.10f : 0.22f,
		m_CoreRecovered ? 0.72f : 0.16f,
		m_CoreRecovered ? 0.82f : 0.18f,
		0.9f);
	// Pilot station: raised chair, wraparound console, holographic gauges and forward canopy.
	m_Renderer->DrawSoftShadow(cockpit.x, cockpit.y - 7.0f, 105.0f, 24.0f, 0.85f);
	m_Renderer->DrawDiamond(cockpit.x, cockpit.y + 7.0f, 118.0f, 38.0f, 0.055f, 0.11f, 0.15f, 1.0f);
	m_Renderer->DrawRect(cockpit.x, cockpit.y + 34.0f, 35.0f, 48.0f, 0.07f, 0.12f, 0.16f, 1.0f);
	m_Renderer->DrawDiamond(
		cockpit.x - 42.0f, cockpit.y + 28.0f, 40.0f, 18.0f, 0.08f, 0.58f, 0.68f, 0.82f);
	m_Renderer->DrawDiamond(
		cockpit.x + 42.0f, cockpit.y + 28.0f, 40.0f, 18.0f, 0.08f, 0.58f, 0.68f, 0.82f);
	m_Renderer->DrawRect(cockpit.x, cockpit.y + 61.0f, 98.0f, 7.0f, 0.20f, 0.68f, 0.78f, 0.60f);
	for (int gauge = -2; gauge <= 2; ++gauge)
		m_Renderer->DrawRect(cockpit.x + gauge * 17.0f,
			cockpit.y + 18.0f,
			8.0f,
			4.0f,
			gauge == 0 ? 0.92f : 0.18f,
			gauge == 0 ? 0.28f : 0.82f,
			0.86f,
			0.92f);
	if (m_CurrentShip == 0)
	{
		ScreenPoint signal = WorldToScreen(SignalPosition.x, SignalPosition.y, 12.0f);
		if (m_CoreRecovered && !m_SignalChecked)
			m_Renderer->DrawDiamond(signal.x, signal.y, 30.0f, 18.0f, 0.52f, 0.12f, 0.62f, 0.85f);
	}

	float pulse = 0.62f + 0.22f * std::sin(m_TotalTime * 4.0f);
	ScreenPoint warningLight = WorldToScreen(-10.5f, -10.5f, 55.0f);
	m_Renderer->DrawDiamond(
		warningLight.x, warningLight.y, 42.0f, 18.0f, 0.85f, 0.04f, 0.05f, pulse);
	m_Renderer->DrawString(-width * 0.5f + 22.0f,
		height * 0.5f - 58.0f,
		m_CurrentShip == 0 ? L"비상 전력" : L"에레보스 생체 신호 감지",
		m_CurrentShip == 0 ? 0.90f : 0.72f,
		0.30f,
		m_CurrentShip == 0 ? 0.25f : 0.82f,
		0.88f);
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
		m_Renderer->DrawRect(foot.x,
			foot.y + obstacle.visualHeight * 0.5f,
			width * 0.72f,
			obstacle.visualHeight,
			obstacle.r * 0.70f,
			obstacle.g * 0.70f,
			obstacle.b * 0.76f,
			1.0f);
		m_Renderer->DrawDiamond(foot.x,
			topY,
			width,
			(obstacle.halfWidth + obstacle.halfHeight) * TileHeight,
			obstacle.r,
			obstacle.g,
			obstacle.b,
			1.0f);
		m_Renderer->DrawRect(foot.x,
			foot.y + obstacle.visualHeight * 0.52f,
			width * 0.62f,
			4.0f,
			0.20f,
			0.42f,
			0.48f,
			0.45f);
		return;
	}

	if (obstacle.type == CargoCrate)
	{
		m_Renderer->DrawRect(foot.x,
			foot.y + obstacle.visualHeight * 0.44f,
			width * 0.76f,
			obstacle.visualHeight * 0.88f,
			0.16f,
			0.19f,
			0.20f,
			1.0f);
		m_Renderer->DrawDiamond(foot.x, topY, width, 27.0f, 0.29f, 0.25f, 0.20f, 1.0f);
		m_Renderer->DrawRect(foot.x,
			foot.y + obstacle.visualHeight * 0.48f,
			6.0f,
			obstacle.visualHeight * 0.78f,
			0.62f,
			0.39f,
			0.12f,
			0.92f);
		m_Renderer->DrawRect(foot.x, topY, width * 0.62f, 3.0f, 0.72f, 0.45f, 0.14f, 0.86f);
		return;
	}

	if (obstacle.type == ControlConsole)
	{
		m_Renderer->DrawRect(foot.x,
			foot.y + obstacle.visualHeight * 0.34f,
			width * 0.54f,
			obstacle.visualHeight * 0.67f,
			0.07f,
			0.12f,
			0.15f,
			1.0f);
		m_Renderer->DrawDiamond(
			foot.x, topY - 3.0f, width * 0.88f, 26.0f, 0.08f, 0.42f, 0.50f, 1.0f);
		m_Renderer->DrawRect(foot.x, topY + 1.0f, width * 0.53f, 9.0f, 0.12f, 0.72f, 0.82f, 0.82f);
		for (int light = -2; light <= 2; ++light)
			m_Renderer->DrawRect(foot.x + light * 9.0f,
				topY + 2.0f,
				4.0f,
				3.0f,
				light == 1 ? 0.92f : 0.16f,
				light == 1 ? 0.25f : 0.78f,
				0.68f,
				1.0f);
		return;
	}

	if (obstacle.type == MedicalPod)
	{
		m_Renderer->DrawRect(foot.x,
			foot.y + obstacle.visualHeight * 0.46f,
			width * 0.68f,
			obstacle.visualHeight * 0.86f,
			0.12f,
			0.18f,
			0.20f,
			1.0f);
		m_Renderer->DrawDiamond(
			foot.x, topY - 2.0f, width * 0.82f, 30.0f, 0.25f, 0.48f, 0.50f, 0.90f);
		m_Renderer->DrawRect(foot.x, topY - 1.0f, 17.0f, 17.0f, 0.78f, 0.82f, 0.77f, 0.95f);
		m_Renderer->DrawRect(foot.x, topY - 1.0f, 5.0f, 15.0f, 0.15f, 0.62f, 0.58f, 1.0f);
		m_Renderer->DrawRect(foot.x, topY - 1.0f, 15.0f, 5.0f, 0.15f, 0.62f, 0.58f, 1.0f);
		return;
	}

	if (obstacle.type == CoolantPipe)
	{
		for (int pipe = -1; pipe <= 1; pipe += 2)
		{
			m_Renderer->DrawRect(foot.x + pipe * width * 0.18f,
				foot.y + obstacle.visualHeight * 0.48f,
				11.0f,
				obstacle.visualHeight * 0.94f,
				0.11f,
				0.26f,
				0.31f,
				1.0f);
			m_Renderer->DrawRect(foot.x + pipe * width * 0.18f,
				topY - 8.0f,
				17.0f,
				6.0f,
				0.26f,
				0.55f,
				0.61f,
				0.95f);
		}
		m_Renderer->DrawRect(
			foot.x, foot.y + 14.0f, width * 0.68f, 8.0f, 0.08f, 0.18f, 0.21f, 1.0f);
		return;
	}

	if (obstacle.type == PowerRelay)
	{
		float pulse = 0.68f + std::sin(m_TotalTime * 6.0f + obstacle.x) * 0.20f;
		m_Renderer->DrawRect(foot.x,
			foot.y + obstacle.visualHeight * 0.48f,
			width * 0.58f,
			obstacle.visualHeight * 0.96f,
			0.08f,
			0.12f,
			0.17f,
			1.0f);
		m_Renderer->DrawDiamond(foot.x, topY, width * 0.76f, 24.0f, 0.16f, 0.28f, 0.34f, 1.0f);
		m_Renderer->DrawRect(foot.x,
			foot.y + obstacle.visualHeight * 0.53f,
			8.0f,
			obstacle.visualHeight * 0.62f,
			0.20f,
			0.78f,
			0.92f,
			pulse);
		m_Renderer->DrawDiamond(foot.x, topY + 4.0f, 14.0f, 10.0f, 0.32f, 0.88f, 1.0f, pulse);
		return;
	}

	// Engine modules use a broad armored housing and a pulsing central coolant core.
	float enginePulse = 0.55f + std::sin(m_TotalTime * 4.0f) * 0.18f;
	m_Renderer->DrawRect(foot.x,
		foot.y + obstacle.visualHeight * 0.43f,
		width * 0.82f,
		obstacle.visualHeight * 0.86f,
		0.07f,
		0.12f,
		0.15f,
		1.0f);
	m_Renderer->DrawDiamond(foot.x, topY, width, 31.0f, 0.13f, 0.20f, 0.24f, 1.0f);
	m_Renderer->DrawDiamond(
		foot.x, topY + 2.0f, width * 0.48f, 18.0f, 0.18f, 0.68f, 0.78f, enginePulse);
	m_Renderer->DrawRect(foot.x - width * 0.31f,
		foot.y + obstacle.visualHeight * 0.46f,
		7.0f,
		obstacle.visualHeight * 0.64f,
		0.30f,
		0.35f,
		0.36f,
		1.0f);
	m_Renderer->DrawRect(foot.x + width * 0.31f,
		foot.y + obstacle.visualHeight * 0.46f,
		7.0f,
		obstacle.visualHeight * 0.64f,
		0.30f,
		0.35f,
		0.36f,
		1.0f);
}

void Game::DrawCharacter(const WorldPoint& point, float r, float g, float b, bool enemy)
{
	ScreenPoint foot = WorldToScreen(point.x, point.y);
	float phase = std::sin(m_TotalTime * (enemy ? 7.0f : 10.0f) + point.x * 0.7f);
	if (!enemy && !m_PlayerMoving)
		phase = std::sin(m_TotalTime * 2.2f) * 0.12f;
	float bob = std::fabs(phase) * (enemy || m_PlayerMoving ? 2.5f : 1.0f);
	m_Renderer->DrawSoftShadow(
		foot.x + 5.0f, foot.y, enemy ? 38.0f : 32.0f, enemy ? 15.0f : 12.0f, 1.0f);
	if (enemy)
	{
		DrawRotatedRect(m_Renderer,
			foot.x - 13.0f,
			foot.y + 14.0f,
			9.0f,
			28.0f,
			phase * 0.28f,
			r * 0.65f,
			g,
			b * 0.75f,
			1.0f);
		DrawRotatedRect(m_Renderer,
			foot.x + 13.0f,
			foot.y + 14.0f,
			9.0f,
			28.0f,
			-phase * 0.28f,
			r * 0.65f,
			g,
			b * 0.75f,
			1.0f);
		m_Renderer->DrawDiamond(foot.x, foot.y + 30.0f + bob, 38.0f, 50.0f, r, g, b, 1.0f);
		m_Renderer->DrawDiamond(
			foot.x, foot.y + 55.0f + bob, 25.0f, 19.0f, r * 0.82f, g * 0.65f, b, 1.0f);
		m_Renderer->DrawRect(
			foot.x - 5.0f, foot.y + 57.0f + bob, 5.0f, 5.0f, 0.20f, 0.95f, 0.82f, 1.0f);
		m_Renderer->DrawRect(
			foot.x + 5.0f, foot.y + 57.0f + bob, 5.0f, 5.0f, 0.20f, 0.95f, 0.82f, 1.0f);
	}
	else
	{
		float stride = m_PlayerMoving ? phase : 0.0f;
		// Boots and articulated pressure-suit legs.
		DrawRotatedRect(m_Renderer,
			foot.x - 7.0f + stride * 2.0f,
			foot.y + 10.0f,
			8.0f,
			23.0f,
			stride * 0.22f,
			0.09f,
			0.24f,
			0.31f,
			1.0f);
		DrawRotatedRect(m_Renderer,
			foot.x + 7.0f - stride * 2.0f,
			foot.y + 10.0f,
			8.0f,
			23.0f,
			-stride * 0.22f,
			0.09f,
			0.24f,
			0.31f,
			1.0f);
		m_Renderer->DrawRect(
			foot.x - 8.0f + stride * 3.0f, foot.y + 1.0f, 13.0f, 7.0f, 0.05f, 0.12f, 0.17f, 1.0f);
		m_Renderer->DrawRect(
			foot.x + 8.0f - stride * 3.0f, foot.y + 1.0f, 13.0f, 7.0f, 0.05f, 0.12f, 0.17f, 1.0f);
		// Backpack, torso shell and chest life-support light.
		m_Renderer->DrawRect(
			foot.x - 4.0f, foot.y + 36.0f + bob, 31.0f, 37.0f, 0.04f, 0.11f, 0.16f, 1.0f);
		m_Renderer->DrawRect(foot.x, foot.y + 34.0f + bob, 25.0f, 35.0f, r, g, b, 1.0f);
		m_Renderer->DrawRect(foot.x, foot.y + 36.0f + bob, 17.0f, 27.0f, 0.08f, 0.23f, 0.30f, 1.0f);
		m_Renderer->DrawDiamond(
			foot.x, foot.y + 39.0f + bob, 8.0f, 6.0f, 0.20f, 0.95f, 0.88f, 1.0f);
		m_Renderer->DrawDiamond(
			foot.x - 16.0f, foot.y + 48.0f + bob, 13.0f, 11.0f, r * 0.85f, g * 0.85f, b, 1.0f);
		m_Renderer->DrawDiamond(
			foot.x + 16.0f, foot.y + 48.0f + bob, 13.0f, 11.0f, r * 0.85f, g * 0.85f, b, 1.0f);
		DrawRotatedRect(m_Renderer,
			foot.x - 17.0f,
			foot.y + 36.0f + bob,
			7.0f,
			25.0f,
			-stride * 0.30f,
			r * 0.75f,
			g * 0.78f,
			b * 0.82f,
			1.0f);
		DrawRotatedRect(m_Renderer,
			foot.x + 17.0f,
			foot.y + 36.0f + bob,
			7.0f,
			25.0f,
			stride * 0.30f,
			r * 0.75f,
			g * 0.78f,
			b * 0.82f,
			1.0f);
		// Helmet shell, dark visor and oxygen valve.
		m_Renderer->DrawDiamond(
			foot.x, foot.y + 61.0f + bob, 27.0f, 23.0f, 0.72f, 0.78f, 0.80f, 1.0f);
		m_Renderer->DrawRect(foot.x, foot.y + 62.0f + bob, 17.0f, 8.0f, 0.025f, 0.11f, 0.17f, 1.0f);
		m_Renderer->DrawRect(foot.x, foot.y + 63.0f + bob, 11.0f, 3.0f, 0.15f, 0.82f, 0.92f, 1.0f);
		m_Renderer->DrawDiamond(
			foot.x - 15.0f, foot.y + 58.0f + bob, 7.0f, 7.0f, 0.20f, 0.75f, 0.82f, 1.0f);
		float aimAngle = std::atan2(static_cast<float>(m_Renderer->Height() / 2 - m_MouseY + 30),
			static_cast<float>(m_MouseX - m_Renderer->Width() / 2));
		DrawRotatedRect(m_Renderer,
			foot.x + std::cos(aimAngle) * 18.0f,
			foot.y + 36.0f + bob + std::sin(aimAngle) * 8.0f,
			30.0f,
			6.0f,
			aimAngle,
			0.08f,
			0.16f,
			0.20f,
			1.0f);
		DrawRotatedRect(m_Renderer,
			foot.x + std::cos(aimAngle) * 29.0f,
			foot.y + 36.0f + bob + std::sin(aimAngle) * 13.0f,
			8.0f,
			3.0f,
			aimAngle,
			0.18f,
			0.82f,
			0.92f,
			1.0f);
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
		DrawRotatedRect(
			m_Renderer, foot.x, foot.y + 9.0f, 45.0f, 18.0f, -0.20f, 0.22f, 0.28f, 0.30f, 1.0f);
		m_Renderer->DrawDiamond(
			foot.x - 24.0f, foot.y + 14.0f, 18.0f, 15.0f, 0.48f, 0.52f, 0.50f, 1.0f);
		DrawRotatedRect(m_Renderer,
			foot.x + 24.0f,
			foot.y + 4.0f,
			27.0f,
			6.0f,
			0.28f,
			0.13f,
			0.17f,
			0.18f,
			1.0f);
		m_Renderer->DrawRect(foot.x - 5.0f, foot.y + 10.0f, 7.0f, 4.0f, 0.52f, 0.06f, 0.05f, 0.75f);
		return;
	}

	float tremble = std::sin(m_TotalTime * 28.0f + npc.animationOffset) * 2.2f;
	m_Renderer->DrawSoftShadow(foot.x, foot.y, 30.0f, 11.0f, 0.85f);
	DrawRotatedRect(m_Renderer,
		foot.x - 6.0f + tremble,
		foot.y + 9.0f,
		7.0f,
		20.0f,
		0.12f,
		0.12f,
		0.20f,
		0.23f,
		1.0f);
	DrawRotatedRect(m_Renderer,
		foot.x + 6.0f + tremble,
		foot.y + 9.0f,
		7.0f,
		20.0f,
		-0.12f,
		0.12f,
		0.20f,
		0.23f,
		1.0f);
	m_Renderer->DrawRect(foot.x + tremble, foot.y + 31.0f, 25.0f, 29.0f, 0.24f, 0.32f, 0.34f, 1.0f);
	DrawRotatedRect(m_Renderer,
		foot.x - 12.0f + tremble,
		foot.y + 35.0f,
		6.0f,
		23.0f,
		-0.55f,
		0.18f,
		0.25f,
		0.27f,
		1.0f);
	DrawRotatedRect(m_Renderer,
		foot.x + 12.0f + tremble,
		foot.y + 35.0f,
		6.0f,
		23.0f,
		0.55f,
		0.18f,
		0.25f,
		0.27f,
		1.0f);
	m_Renderer->DrawDiamond(
		foot.x + tremble, foot.y + 52.0f, 22.0f, 19.0f, 0.42f, 0.47f, 0.45f, 1.0f);
	m_Renderer->DrawRect(foot.x + tremble, foot.y + 52.0f, 14.0f, 6.0f, 0.04f, 0.09f, 0.10f, 1.0f);
	m_Renderer->DrawString(foot.x - 42.0f,
		foot.y + 67.0f,
		npc.dialogueId == 0 ? L"통신장교 서윤" : L"정비사 라울",
		0.66f,
		0.78f,
		0.76f,
		0.85f);
}

void Game::RenderSpace()
{
	m_SceneGraph.Clear();
	m_Renderer->BeginFrame(0.004f, 0.008f, 0.025f, 1.0f);
	int width = m_Renderer->Width();
	int height = m_Renderer->Height();
	m_Renderer->DrawSpaceBackground(m_TotalTime, m_ShipPosition.x, m_ShipPosition.y);
	m_Renderer->DrawDiamond(
		width * 0.37f, height * 0.34f, 310.0f, 185.0f, 0.08f, 0.13f, 0.23f, 0.75f);
	m_Renderer->DrawDiamond(
		width * 0.37f, height * 0.34f, 255.0f, 145.0f, 0.16f, 0.24f, 0.39f, 0.32f);

	const WorldPoint debris[] = {
		{-90.0f, 170.0f}, {85.0f, 290.0f}, {-70.0f, 410.0f}, {55.0f, 520.0f}};
	for (int i = 0; i < 4; ++i)
	{
		const int debrisIndex = i;
		CreateSceneActor("SpaceDebris",
			debris[i].x,
			debris[i].y,
			0.0f,
			1,
			debris[i].y,
			[this, debrisIndex](const ActorTransform& transform)
			{
				float x = (transform.x - m_ShipPosition.x) * 0.72f;
				float y = (transform.y - m_ShipPosition.y) * 0.72f;
				m_Renderer->DrawDiamond(x,
					y,
					72.0f + debrisIndex * 8.0f,
					30.0f + debrisIndex * 5.0f,
					0.18f,
					0.21f,
					0.25f,
					1.0f);
				m_Renderer->DrawRect(x + 12.0f, y + 7.0f, 26.0f, 5.0f, 0.65f, 0.12f, 0.10f, 0.65f);
			});
	}
	for (size_t i = 0; i < m_Asteroids.size(); ++i)
	{
		const Asteroid asteroid = m_Asteroids[i];
		Actor* actor = CreateSceneActor("Asteroid",
			asteroid.position.x,
			asteroid.position.y,
			0.0f,
			2,
			asteroid.position.y,
			[this, asteroid](const ActorTransform& transform)
			{
				Asteroid placedAsteroid = asteroid;
				placedAsteroid.position = {transform.x, transform.y};
				placedAsteroid.rotation = transform.rotation;
				DrawAsteroid(placedAsteroid);
			});
		actor->LocalTransform().rotation = asteroid.rotation;
	}

	float destinationX = (DestinationPosition.x - m_ShipPosition.x) * 0.72f;
	float destinationY = (DestinationPosition.y - m_ShipPosition.y) * 0.72f;
	float beaconPulse = 0.55f + std::sin(m_TotalTime * 5.0f) * 0.25f;
	m_Renderer->DrawDiamond(
		destinationX, destinationY, 125.0f, 58.0f, 0.08f, 0.34f, 0.46f, beaconPulse);
	m_Renderer->DrawRect(
		destinationX, destinationY + 12.0f, 70.0f, 20.0f, 0.10f, 0.18f, 0.23f, 1.0f);
	const float destinationScale = m_TravelDestinationShip == 1 ? 2.3f : 1.6f;
	const float destinationRed = m_TravelDestinationShip == 1 ? 0.32f : 0.20f;
	Actor* destinationShip = CreateSceneActor("DestinationShip",
		destinationX,
		destinationY + 14.0f,
		0.0f,
		3,
		destinationY,
		[this, destinationScale, destinationRed](const ActorTransform& transform)
		{
			DrawShip(transform.x,
				transform.y,
				transform.rotation,
				destinationScale * transform.scaleX,
				destinationRed,
				0.48f,
				0.58f);
		});
	destinationShip->LocalTransform().rotation = -Pi * 0.5f;
	float impactShake = m_ShipImpactFlash > 0.0f ? std::sin(m_TotalTime * 95.0f) * 6.0f : 0.0f;
	float shipX = impactShake;
	float shipY = -35.0f + impactShake * 0.35f;
	float shipSpeed = Length(m_ShipVelocity.x, m_ShipVelocity.y);
	Actor* playerShip = CreateSceneActor("PlayerShip",
		shipX,
		shipY,
		0.0f,
		4,
		shipY,
		[this](const ActorTransform& transform)
		{
			DrawShip(transform.x,
				transform.y,
				transform.rotation,
				transform.scaleX,
				0.20f,
				0.67f,
				0.82f);
		});
	playerShip->LocalTransform().rotation = m_ShipAngle;
	if (shipSpeed > 35.0f)
	{
		float flameLength = 34.0f + Clamp(shipSpeed / 180.0f, 0.0f, 1.0f) * 28.0f;
		float flicker = std::sin(m_TotalTime * 38.0f) * 4.0f;
		Actor* exhaust = CreateSceneActor(
			"EngineExhaust",
			-(38.0f + flameLength * 0.5f),
			0.0f,
			0.0f,
			3,
			shipY - 0.1f,
			[this, flameLength, flicker](const ActorTransform& transform)
			{
				DrawRotatedRect(m_Renderer,
					transform.x,
					transform.y,
					flameLength + flicker,
					17.0f,
					transform.rotation,
					0.08f,
					0.45f,
					1.0f,
					0.42f);
				DrawRotatedRect(m_Renderer,
					transform.x + std::cos(transform.rotation) * 7.0f,
					transform.y + std::sin(transform.rotation) * 7.0f,
					flameLength * 0.62f,
					7.0f,
					transform.rotation,
					0.72f,
					0.94f,
					1.0f,
					0.82f);
			},
			playerShip);
		exhaust->SetSortOrder(shipY - 0.1f);
	}
	m_SceneGraph.Render();

	if (m_Mode == Transition)
	{
		float alpha = Clamp(m_TransitionTimer / 1.4f, 0.0f, 1.0f);
		m_Renderer->DrawRect(0.0f,
			0.0f,
			static_cast<float>(width),
			static_cast<float>(height),
			0.0f,
			0.0f,
			0.0f,
			alpha);
	}
	if (m_ShipImpactFlash > 0.0f)
	{
		float flashAlpha = Clamp(m_ShipImpactFlash / 0.45f, 0.0f, 1.0f) * 0.34f;
		m_Renderer->DrawRect(0.0f,
			0.0f,
			static_cast<float>(width),
			static_cast<float>(height),
			0.78f,
			0.08f,
			0.035f,
			flashAlpha);
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
	m_Renderer->DrawDiamond(x - radius * 0.18f,
		y + radius * 0.16f,
		radius * 0.55f,
		radius * 0.34f,
		0.10f,
		0.09f,
		0.085f,
		0.82f);
	m_Renderer->DrawDiamond(x + radius * 0.24f,
		y - radius * 0.14f,
		radius * 0.31f,
		radius * 0.22f,
		0.36f,
		0.30f,
		0.25f,
		0.78f);
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
	m_Renderer->DrawQuad(
		shoulderTop, rearTop, centerRear, centerRear, r * 0.62f, g * 0.64f, b * 0.68f, 1.0f);
	m_Renderer->DrawQuad(
		centerRear, rearBottom, shoulderBottom, centerRear, r * 0.52f, g * 0.58f, b * 0.63f, 1.0f);
	// Swept wings and paired engine pods make the silhouette readable while turning.
	DrawRotatedRect(m_Renderer,
		x,
		y,
		48.0f * scale,
		9.0f * scale,
		angle,
		r * 0.58f,
		g * 0.62f,
		b * 0.68f,
		1.0f);
	ScreenPoint engineA = Rotate(-24.0f * scale, 17.0f * scale, angle, x, y);
	ScreenPoint engineB = Rotate(-24.0f * scale, -17.0f * scale, angle, x, y);
	DrawRotatedRect(m_Renderer,
		engineA.x,
		engineA.y,
		25.0f * scale,
		8.0f * scale,
		angle,
		0.08f,
		0.18f,
		0.23f,
		1.0f);
	DrawRotatedRect(m_Renderer,
		engineB.x,
		engineB.y,
		25.0f * scale,
		8.0f * scale,
		angle,
		0.08f,
		0.18f,
		0.23f,
		1.0f);
	ScreenPoint canopy = Rotate(12.0f * scale, 0.0f, angle, x, y);
	m_Renderer->DrawDiamond(
		canopy.x, canopy.y, 20.0f * scale, 11.0f * scale, 0.55f, 0.84f, 0.92f, 0.95f);
	ScreenPoint armor = Rotate(-7.0f * scale, 0.0f, angle, x, y);
	m_Renderer->DrawDiamond(
		armor.x, armor.y, 17.0f * scale, 8.0f * scale, r * 1.15f, g * 1.05f, b, 0.95f);
	ScreenPoint exhaustA = Rotate(-38.0f * scale, 17.0f * scale, angle, x, y);
	ScreenPoint exhaustB = Rotate(-38.0f * scale, -17.0f * scale, angle, x, y);
	m_Renderer->DrawDiamond(
		exhaustA.x, exhaustA.y, 8.0f * scale, 6.0f * scale, 0.20f, 0.78f, 1.0f, 0.82f);
	m_Renderer->DrawDiamond(
		exhaustB.x, exhaustB.y, 8.0f * scale, 6.0f * scale, 0.20f, 0.78f, 1.0f, 0.82f);
}

void Game::RenderFirstLevel()
{
	m_SceneGraph.Clear();
	m_Renderer->BeginFrame(0.008f, 0.014f, 0.025f, 1.0f);
	int halfSize = m_FirstLevelSize / 2;
	int screenWidth = m_Renderer->Width();
	int screenHeight = m_Renderer->Height();

	for (int tileY = 0; tileY < m_FirstLevelSize; ++tileY)
	{
		for (int tileX = 0; tileX < m_FirstLevelSize; ++tileX)
		{
			float worldX = static_cast<float>(tileX - halfSize);
			float worldY = static_cast<float>(tileY - halfSize);
			ScreenPoint screen = WorldToScreen(worldX, worldY);
			if (std::fabs(screen.x) > screenWidth * 0.5f + 110.0f ||
				std::fabs(screen.y) > screenHeight * 0.5f + 110.0f)
				continue;

			int tile = m_FirstLevelTiles[tileY * m_FirstLevelSize + tileX];

			if (tile > 0)
			{
				float variation =
					static_cast<float>((tileX * 13 + tileY * 7 + m_LevelSeed) % 5) * 0.006f;
				float roomLight = tile == 2 ? 0.018f : 0.0f;
				m_Renderer->DrawDiamond(screen.x,
					screen.y,
					TileWidth - 2.0f,
					TileHeight - 1.0f,
					0.050f + variation + roomLight,
					0.078f + variation + roomLight,
					0.098f + variation + roomLight,
					1.0f);
				if (tile == 1 && ((tileX + tileY) % 4 == 0))
					m_Renderer->DrawDiamond(
						screen.x, screen.y + 1.0f, 21.0f, 7.0f, 0.10f, 0.42f, 0.46f, 0.42f);
			}
			else if (tileX > 0 && tileY > 0 && tileX < m_FirstLevelSize - 1 &&
					 tileY < m_FirstLevelSize - 1)
			{
				bool besideFloor = m_FirstLevelTiles[tileY * m_FirstLevelSize + tileX - 1] > 0 ||
								   m_FirstLevelTiles[tileY * m_FirstLevelSize + tileX + 1] > 0 ||
								   m_FirstLevelTiles[(tileY - 1) * m_FirstLevelSize + tileX] > 0 ||
								   m_FirstLevelTiles[(tileY + 1) * m_FirstLevelSize + tileX] > 0;
				if (besideFloor)
				{
					CreateSceneActor("CorridorWall",
						worldX,
						worldY,
						0.0f,
						1,
						worldX + worldY,
						[this](const ActorTransform& transform)
						{
							ScreenPoint wall =
								WorldToScreen(transform.x, transform.y, transform.height);
							m_Renderer->DrawSoftShadow(wall.x, wall.y, 52.0f, 16.0f, 0.76f);
							m_ModelLibrary.Draw(m_Renderer, "corridor_wall", wall.x, wall.y, 0.92f);
						});
				}
			}
		}
	}

	if (!m_LevelWeaponCollected)
	{
		CreateSceneActor("PulseRiflePickup",
			FirstLevelWeaponPosition.x,
			FirstLevelWeaponPosition.y,
			11.0f,
			2,
			FirstLevelWeaponPosition.x + FirstLevelWeaponPosition.y,
			[this](const ActorTransform& transform)
			{
				ScreenPoint weapon = WorldToScreen(transform.x, transform.y, transform.height);
				float pulse = 1.0f + std::sin(m_TotalTime * 4.0f) * 0.08f;
				m_Renderer->DrawSoftShadow(weapon.x, weapon.y - 10.0f, 60.0f, 16.0f, 0.65f);
				m_ModelLibrary.Draw(m_Renderer, "pulse_rifle_pickup", weapon.x, weapon.y, pulse);
			});
	}

	for (size_t index = 0; index < m_LevelCorpses.size(); ++index)
	{
		const WorldPoint position = m_LevelCorpses[index];
		CreateSceneActor("FallenSoldier",
			position.x,
			position.y,
			3.0f,
			1,
			position.x + position.y,
			[this](const ActorTransform& transform)
			{
				ScreenPoint corpse = WorldToScreen(transform.x, transform.y, transform.height);
				m_Renderer->DrawSoftShadow(corpse.x, corpse.y - 3.0f, 58.0f, 13.0f, 0.82f);
				m_ModelLibrary.Draw(m_Renderer, "fallen_soldier", corpse.x, corpse.y, 1.0f);
			});
	}

	for (size_t index = 0; index < m_StoryDocuments.size(); ++index)
	{
		const StoryDocument& document = m_StoryDocuments[index];
		const float animationOffset = static_cast<float>(index);
		CreateSceneActor("StoryDocument",
			document.position.x,
			document.position.y,
			7.0f,
			2,
			document.position.x + document.position.y,
			[this, document, animationOffset](const ActorTransform& transform)
			{
				float pulse = document.read
								  ? 1.0f
								  : 1.0f + std::sin(m_TotalTime * 5.0f + animationOffset) * 0.12f;
				ScreenPoint paper = WorldToScreen(transform.x, transform.y, transform.height);
				m_ModelLibrary.Draw(m_Renderer,
					document.read ? "field_document_read" : "field_document",
					paper.x,
					paper.y,
					pulse);
			});
	}

	for (size_t index = 0; index < m_MagazinePickups.size(); ++index)
	{
		const MagazinePickup& magazine = m_MagazinePickups[index];
		if (magazine.collected)
			continue;

		const float animationOffset = static_cast<float>(index);
		CreateSceneActor("AmmoMagazine",
			magazine.position.x,
			magazine.position.y,
			8.0f,
			2,
			magazine.position.x + magazine.position.y,
			[this, animationOffset](const ActorTransform& transform)
			{
				float pulse = 1.0f + std::sin(m_TotalTime * 4.5f + animationOffset) * 0.08f;
				ScreenPoint screen = WorldToScreen(transform.x, transform.y, transform.height);
				m_Renderer->DrawSoftShadow(screen.x, screen.y - 7.0f, 32.0f, 10.0f, 0.70f);
				m_ModelLibrary.Draw(m_Renderer, "ammo_magazine", screen.x, screen.y, pulse);
			});
	}

	for (size_t index = 0; index < m_LevelSurvivors.size(); ++index)
	{
		const LevelSurvivor& survivor = m_LevelSurvivors[index];
		const float animationOffset = static_cast<float>(index);
		CreateSceneActor("LevelSurvivor",
			survivor.position.x,
			survivor.position.y,
			0.0f,
			3,
			survivor.position.x + survivor.position.y,
			[this, animationOffset](const ActorTransform& transform)
			{
				ScreenPoint screen = WorldToScreen(transform.x, transform.y, transform.height);
				float tremble = std::sin(m_TotalTime * 24.0f + animationOffset) * 1.7f;
				m_Renderer->DrawSoftShadow(screen.x, screen.y, 34.0f, 12.0f, 0.82f);
				m_ModelLibrary.Draw(
					m_Renderer, "wounded_survivor", screen.x + tremble, screen.y, 1.0f);
				m_Renderer->DrawString(
					screen.x - 27.0f, screen.y + 61.0f, L"생존자", 0.62f, 0.82f, 0.76f, 0.90f);
			});
	}

	for (size_t index = 0; index < m_Enemies.size(); ++index)
	{
		if (!m_Enemies[index].active || m_Enemies[index].health <= 0)
			continue;

		const WorldPoint position = m_Enemies[index].position;
		CreateSceneActor("VoidScout",
			position.x,
			position.y,
			0.0f,
			3,
			position.x + position.y,
			[this](const ActorTransform& transform)
			{
				ScreenPoint enemy = WorldToScreen(transform.x, transform.y, transform.height);
				m_Renderer->DrawSoftShadow(enemy.x, enemy.y, 39.0f, 14.0f, 0.88f);
				m_ModelLibrary.Draw(m_Renderer, "void_scout", enemy.x, enemy.y, 1.0f);
			});
	}

	CreateSceneActor("Player",
		m_Player.x,
		m_Player.y,
		0.0f,
		3,
		m_Player.x + m_Player.y,
		[this](const ActorTransform& transform)
		{
			WorldPoint position = {transform.x, transform.y};
			DrawCharacter(position, 0.20f, 0.72f, 0.86f, false);
		});

	for (size_t index = 0; index < m_Projectiles.size(); ++index)
	{
		const Projectile projectile = m_Projectiles[index];
		CreateSceneActor("Projectile",
			projectile.position.x,
			projectile.position.y,
			28.0f,
			4,
			projectile.position.x + projectile.position.y,
			[this, projectile](const ActorTransform& transform)
			{
				ScreenPoint bullet = WorldToScreen(transform.x, transform.y, transform.height);
				ScreenPoint previous = WorldToScreen(
					projectile.previousPosition.x, projectile.previousPosition.y, transform.height);
				float tracerWidth =
					std::max(10.0f, Length(bullet.x - previous.x, bullet.y - previous.y) * 1.7f);
				m_Renderer->DrawDiamond((bullet.x + previous.x) * 0.5f,
					(bullet.y + previous.y) * 0.5f,
					tracerWidth,
					5.0f,
					projectile.charged ? 1.0f : 0.10f,
					projectile.charged ? 0.34f : 0.62f,
					projectile.charged ? 0.08f : 0.94f,
					projectile.charged ? 0.70f : 0.42f);
				m_ModelLibrary.Draw(m_Renderer,
					projectile.charged ? "charged_bullet" : "pulse_bullet",
					bullet.x,
					bullet.y,
					1.0f);
			});
	}

	for (size_t index = 0; index < m_ExperienceOrbs.size(); ++index)
	{
		const ExperienceOrb orb = m_ExperienceOrbs[index];
		const float animationOffset = static_cast<float>(index);
		CreateSceneActor("ExperienceOrb",
			orb.position.x,
			orb.position.y,
			28.0f,
			4,
			orb.position.x + orb.position.y,
			[this, animationOffset](const ActorTransform& transform)
			{
				float bob = std::sin(m_TotalTime * 7.0f + animationOffset) * 8.0f;
				ScreenPoint screen =
					WorldToScreen(transform.x, transform.y, transform.height + bob);
				m_ModelLibrary.Draw(m_Renderer, "xp_orb", screen.x, screen.y, 1.0f);
				m_Renderer->DrawString(
					screen.x - 19.0f, screen.y + 18.0f, L"+25 XP", 0.32f, 1.0f, 0.72f, 0.94f);
			});
	}

	m_SceneGraph.Render();
}

void Game::RenderDestination()
{
	m_SceneGraph.Clear();
	m_Renderer->BeginFrame(0.006f, 0.012f, 0.025f, 1.0f);
	int width = m_Renderer->Width();
	int height = m_Renderer->Height();
	m_Renderer->DrawDiamond(0.0f, -20.0f, width * 0.86f, height * 0.45f, 0.07f, 0.10f, 0.14f, 1.0f);
	m_Renderer->DrawRect(-115.0f, 15.0f, 180.0f, 70.0f, 0.08f, 0.28f, 0.34f, 1.0f);
	m_Renderer->DrawRect(90.0f, 28.0f, 155.0f, 62.0f, 0.24f, 0.05f, 0.30f, 0.95f);
	m_Renderer->DrawRect(-115.0f, 62.0f, 8.0f, 42.0f, 0.18f, 0.85f, 0.90f, 0.75f);
	m_Renderer->DrawDiamond(105.0f, 62.0f, 95.0f, 32.0f, 0.36f, 0.08f, 0.48f, 0.75f);
	Actor* ship = CreateSceneActor("DockedPlayerShip",
		0.0f,
		-125.0f,
		0.0f,
		1,
		0.0f,
		[this](const ActorTransform& transform)
		{
			DrawShip(transform.x,
				transform.y,
				transform.rotation,
				1.3f * transform.scaleX,
				0.22f,
				0.68f,
				0.82f);
		});
	ship->LocalTransform().rotation = Pi * 0.5f;
	m_SceneGraph.Render();
}

void Game::RenderInterface()
{
	int width = m_Renderer->Width();
	int height = m_Renderer->Height();
	m_Renderer->DrawRect(0.0f,
		height * 0.5f - 27.0f,
		static_cast<float>(width),
		54.0f,
		0.015f,
		0.025f,
		0.045f,
		0.88f);
	m_Renderer->DrawString(
		-width * 0.5f + 20.0f, height * 0.5f - 33.0f, ObjectiveText(), 0.62f, 0.88f, 0.94f, 1.0f);

	if (m_Mode == OnFoot || m_Mode == FirstLevel)
	{
		std::wostringstream health;
		health << L"전투복 " << static_cast<int>(m_PlayerHealth) << L" / "
			   << static_cast<int>(m_PlayerMaxHealth);
		float healthRatio = Clamp(m_PlayerHealth / m_PlayerMaxHealth, 0.0f, 1.0f);
		m_Renderer->DrawRect(-width * 0.5f + 93.0f,
			-height * 0.5f + 27.0f,
			170.0f,
			35.0f,
			0.025f,
			0.04f,
			0.06f,
			0.9f);
		m_Renderer->DrawRect(-width * 0.5f + 18.0f + healthRatio * 72.0f,
			-height * 0.5f + 18.0f,
			healthRatio * 144.0f,
			6.0f,
			0.15f,
			0.68f,
			0.76f,
			1.0f);
		m_Renderer->DrawString(
			-width * 0.5f + 24.0f, -height * 0.5f + 25.0f, health.str(), 0.72f, 0.86f, 0.88f, 1.0f);
		if (m_Quest != InspectTerminal)
		{
			std::wostringstream weapon;
			if (m_Mode == FirstLevel)
				weapon << L"펄스 소총  " << m_AmmoInMagazine << L" / 12   예비 탄창 "
					   << m_SpareMagazines << L"개";
			else
				weapon << L"펄스 카빈 " << static_cast<int>(m_WeaponEnergy) << L"%";
			m_Renderer->DrawRect(-width * 0.5f + 150.0f,
				-height * 0.5f + 58.0f,
				284.0f,
				25.0f,
				0.025f,
				0.04f,
				0.06f,
				0.90f);
			m_Renderer->DrawString(-width * 0.5f + 20.0f,
				-height * 0.5f + 54.0f,
				m_ReloadTimer > 0.0f
					? (m_Mode == FirstLevel ? L"펄스 소총 재장전 중..." : L"펄스 카빈 충전 중...")
					: weapon.str(),
				0.34f,
				0.86f,
				0.92f,
				1.0f);
		}

		if (m_Mode == FirstLevel)
		{
			std::wostringstream level;
			level << L"레벨 " << m_PlayerLevel << L"    경험치 " << m_Experience << L" / "
				  << m_ExperienceToNextLevel;
			std::wostringstream stats;
			stats << L"화력 " << m_Firepower << L"  기동 " << m_Agility << L"  생존 " << m_Vitality
				  << L"  포인트 " << m_StatPoints;
			float experienceRatio = Clamp(
				static_cast<float>(m_Experience) / static_cast<float>(m_ExperienceToNextLevel),
				0.0f,
				1.0f);
			m_Renderer->DrawRect(width * 0.5f - 174.0f,
				-height * 0.5f + 52.0f,
				326.0f,
				82.0f,
				0.025f,
				0.04f,
				0.06f,
				0.90f);
			m_Renderer->DrawString(width * 0.5f - 325.0f,
				-height * 0.5f + 70.0f,
				level.str(),
				0.38f,
				0.90f,
				0.76f,
				1.0f);
			m_Renderer->DrawString(width * 0.5f - 325.0f,
				-height * 0.5f + 42.0f,
				stats.str(),
				0.68f,
				0.78f,
				0.86f,
				1.0f);
			m_Renderer->DrawRect(width * 0.5f - 174.0f,
				-height * 0.5f + 20.0f,
				302.0f,
				8.0f,
				0.035f,
				0.08f,
				0.09f,
				1.0f);
			m_Renderer->DrawRect(width * 0.5f - 325.0f + experienceRatio * 151.0f,
				-height * 0.5f + 20.0f,
				experienceRatio * 302.0f,
				6.0f,
				0.18f,
				0.88f,
				0.62f,
				1.0f);
			if ((m_ShotsFired + 1) % 4 == 0 && m_LevelWeaponCollected)
				m_Renderer->DrawString(-width * 0.5f + 20.0f,
					-height * 0.5f + 82.0f,
					L"다음 탄환: 주황색 강화 펄스탄 (+1 피해)",
					1.0f,
					0.55f,
					0.20f,
					1.0f);
		}
	}
	else if (m_Mode == ShipControl || m_Mode == Transition)
	{
		std::wostringstream ship;
		ship << (m_CurrentShip == 0 ? L"노크티스 선체 " : L"에레보스 선체 ")
			 << static_cast<int>(m_ShipHealth) << L"%";
		m_Renderer->DrawRect(-width * 0.5f + 105.0f,
			-height * 0.5f + 27.0f,
			194.0f,
			35.0f,
			0.025f,
			0.04f,
			0.06f,
			0.90f);
		m_Renderer->DrawRect(-width * 0.5f + 18.0f + m_ShipHealth * 0.82f,
			-height * 0.5f + 18.0f,
			m_ShipHealth * 1.64f,
			6.0f,
			m_ShipHealth < 40.0f ? 0.86f : 0.16f,
			m_ShipHealth < 40.0f ? 0.12f : 0.68f,
			0.72f,
			1.0f);
		m_Renderer->DrawString(
			-width * 0.5f + 22.0f, -height * 0.5f + 25.0f, ship.str(), 0.58f, 0.82f, 0.92f, 1.0f);
		if (m_ShipImpactFlash > 0.0f)
			m_Renderer->DrawString(
				-90.0f, height * 0.5f - 67.0f, L"경고: 선체 충격 감지", 1.0f, 0.28f, 0.16f, 1.0f);
	}

	if (m_Mode == FirstLevel && m_LevelUpFlash > 0.0f)
	{
		float pulse = 0.78f + std::sin(m_TotalTime * 8.0f) * 0.18f;
		m_Renderer->DrawRect(0.0f, 75.0f, 460.0f, 118.0f, 0.025f, 0.08f, 0.075f, 0.94f);
		m_Renderer->DrawString(-84.0f, 103.0f, L"레벨 상승", 0.30f, 1.0f, 0.72f, pulse);
		m_Renderer->DrawString(-178.0f,
			70.0f,
			L"스탯 포인트 +3    [1] 화력  [2] 기동  [3] 생존",
			0.78f,
			0.92f,
			0.86f,
			1.0f);
		m_Renderer->DrawString(-133.0f,
			42.0f,
			L"포인트를 모두 사용하면 첫 레벨이 완료됩니다.",
			0.52f,
			0.72f,
			0.70f,
			1.0f);
	}

	std::wstring interaction = InteractionText();
	if (!interaction.empty())
	{
		m_Renderer->DrawRect(
			0.0f, -height * 0.5f + 72.0f, 310.0f, 34.0f, 0.02f, 0.05f, 0.07f, 0.92f);
		m_Renderer->DrawString(
			-145.0f, -height * 0.5f + 68.0f, interaction, 0.78f, 0.94f, 0.96f, 1.0f);
	}
	if (m_MessageTimer > 0.0f)
	{
		m_Renderer->DrawRect(
			0.0f, -height * 0.5f + 28.0f, 590.0f, 32.0f, 0.025f, 0.025f, 0.045f, 0.92f);
		m_Renderer->DrawString(
			-280.0f, -height * 0.5f + 24.0f, m_Message, 0.76f, 0.78f, 0.88f, 1.0f);
	}
	if (m_Keys['\t'] && (m_Mode == OnFoot || m_Mode == FirstLevel))
		m_Renderer->DrawString(-220.0f,
			height * 0.5f - 58.0f,
			L"WASD 이동 | E 상호작용 | 마우스 공격 | Space 회피",
			0.60f,
			0.75f,
			0.80f,
			1.0f);
	if (m_Paused)
	{
		m_Renderer->DrawRect(0.0f, 0.0f, 300.0f, 105.0f, 0.01f, 0.015f, 0.025f, 0.95f);
		m_Renderer->DrawString(-38.0f, 12.0f, L"일시 정지", 0.76f, 0.90f, 0.94f, 1.0f);
		m_Renderer->DrawString(
			-88.0f, -18.0f, L"계속하려면 ESC를 누르십시오", 0.48f, 0.63f, 0.68f, 1.0f);
	}
	if (m_Mode == Complete)
	{
		m_Renderer->DrawRect(0.0f, height * 0.27f, 470.0f, 105.0f, 0.01f, 0.02f, 0.035f, 0.94f);
		m_Renderer->DrawString(-115.0f,
			height * 0.27f + 20.0f,
			L"퀘스트 완료: 마지막 항로",
			0.32f,
			0.90f,
			0.92f,
			1.0f);
		m_Renderer->DrawString(-175.0f,
			height * 0.27f - 8.0f,
			L"새 목표: 구조 신호의 근원을 조사하십시오",
			0.70f,
			0.72f,
			0.82f,
			1.0f);
		m_Renderer->DrawString(
			-72.0f, height * 0.27f - 34.0f, L"R 키로 다시 시작", 0.48f, 0.65f, 0.70f, 1.0f);
	}
}
