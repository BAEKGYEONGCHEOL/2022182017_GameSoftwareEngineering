#include "stdafx.h"
#include "../Game.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <queue>
#include <random>
#include <sstream>

namespace
{
const WorldPoint FirstLevelWeaponPosition = {-1.5f, 0.0f};

enum SurvivorPattern
{
	SurvivorFrozen,
	SurvivorTrembling,
	SurvivorHiding,
	SurvivorSearching,
	SurvivorMedic,
	SurvivorGuard,
	SurvivorMechanic,
	SurvivorSignalAnalyst,
	SurvivorDeserter,
	SurvivorFanatic
};

float Length(float x, float y)
{
	return std::sqrt(x * x + y * y);
}

std::wstring SurvivorDialogue(const LevelSurvivor& survivor)
{
	if (survivor.confused)
	{
		return survivor.talked
			? L"무장 생존자: 방금 누구를 겨눈 거지... 헬멧 표식이 전부 적으로 보여요."
			: L"무장 생존자: 더 가까이 오지 마! 공허종이 사람 목소리를 흉내 낸다고 했어!";
	}

	switch (survivor.pattern)
	{
		case SurvivorFrozen:
			return survivor.talked ? L"충격 생존자: 문이 닫히는 소리만 계속 들립니다."
								   : L"충격 생존자: 명령을 기다렸는데 아무도 돌아오지 않았습니다.";
		case SurvivorTrembling:
			return survivor.talked ? L"난민: 총을 내리면 떨림이 조금 멎을 것 같아요."
								   : L"난민: 제발 먼저 쏘지 마세요. 누가 사람인지 구분이 안 돼요.";
		case SurvivorHiding:
			return survivor.talked ? L"승무원: 환풍구를 따라가면 봉쇄 구역을 피할 수 있어요."
								   : L"승무원: 전투보다 검문이 무서워 숨어 있었습니다.";
		case SurvivorSearching:
			return survivor.talked ? L"난민 한: 구조선이 오면 아이들부터 태워 주세요."
								   : L"난민 한: 안전 방송을 믿고 가족과 다른 갑판으로 갈라졌습니다.";
		case SurvivorMedic:
			return survivor.talked ? L"의무관 일리안: 시체는 어느 종족이 먼저 쐈는지 말해 주지 않아요."
								   : L"의무관 일리안: 감염 공포가 탄환보다 많은 사람을 죽였습니다.";
		case SurvivorGuard:
			return survivor.talked ? L"경비대원 미라: 이제 명령보다 눈앞의 생존자를 확인하겠습니다."
								   : L"경비대원 미라: 격벽을 닫으라는 명령을 따랐습니다. 사람들이 안에 있었는데...";
		case SurvivorMechanic:
			return survivor.talked ? L"정비기사 준: 동력실 우회 통로는 아직 살아 있습니다."
								   : L"정비기사 준: 공허종은 비무장 작업자를 지나쳤습니다. 이유는 모르겠어요.";
		case SurvivorSignalAnalyst:
			return survivor.talked ? L"통신 분석관: 구조 요청과 공격 명령이 같은 주파수에 섞여 있어요."
								   : L"통신 분석관: 지휘부가 공허종 신호에서 공포 반응을 지웠습니다.";
		case SurvivorDeserter:
			return survivor.talked ? L"탈영병: 살아남으면 제가 버린 사람들의 이름을 기록해 주세요."
								   : L"탈영병: 아군이 민간인을 쏘는 걸 보고 총을 들고 도망쳤습니다.";
		default:
			return survivor.talked ? L"귀환교도: 공유와 강제 결합은 다릅니다. 그 경계를 잊지 마세요."
								   : L"귀환교도: 공허종의 기억 속에서도 두려움이 들렸습니다. 괴물도 두려워합니다.";
	}
}
} // namespace
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
		for (int engineIndex = 0; engineIndex < 4; ++engineIndex)
		{
			if (m_LevelEngineActive[engineIndex] ||
				!IsNear(m_LevelEnginePositions[engineIndex], 1.45f))
				continue;
			m_LevelEngineActive[engineIndex] = true;
			interactionHandled = true;
			int activeCount = 0;
			for (int countIndex = 0; countIndex < 4; ++countIndex)
				if (m_LevelEngineActive[countIndex])
					++activeCount;
			if (activeCount == 4)
				SetMessage(L"네 개의 보조 엔진이 모두 가동되었습니다. 중앙 시스템 제어 장치의 감염막이 약해집니다.", 4.0f);
			else
				SetMessage(L"보조 엔진 가동: " + std::to_wstring(activeCount) + L" / 4", 2.5f);
			break;
		}

		if (!interactionHandled && IsNear(m_LevelControlPosition, 1.5f))
		{
			interactionHandled = true;
			if (!AreLevelEnginesActive())
				SetMessage(L"시스템 제어 장치가 감염막에 잠겨 있습니다. 보조 엔진 네 개를 모두 가동하십시오.", 4.0f);
			else if (!AreLevelHostilesCleared())
				SetMessage(L"항로 안전 확보 실패: 모든 공허종과 공황 분대를 처치해야 합니다.", 4.0f);
			else
			{
				m_ErebusSystemsOnline = true;
				m_Quest = BoardPersonalShipToNoctis;
				SetMessage(L"에레보스 시스템 복구 완료. 도킹 통로를 따라 개인 우주선으로 복귀하십시오.", 4.5f);
				return;
			}
		}
		if (!interactionHandled && m_Quest == BoardPersonalShipToNoctis &&
			IsNear(WorldPoint{-57.0f, -54.0f}, 1.6f))
		{
			m_Quest = ReturnToNoctis;
			BeginPersonalShipTravel(0);
			return;
		}
		for (size_t index = 0; index < m_MagazinePickups.size(); ++index)
		{
			if (interactionHandled)
				break;
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
			if (survivor.health <= 0 || !IsNear(survivor.position, 1.3f))
				continue;
			if (survivor.confused)
			{
				SetDialogue(L"무장 생존자: 다가오지 마! 전부 공허종이야!", survivor.position, 3.0f);
				interactionHandled = true;
				break;
			}

			SetDialogue(SurvivorDialogue(survivor), survivor.position, 5.0f);
			survivor.talked = true;
			interactionHandled = true;
		}
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
	std::uniform_int_distribution<int> roomJitter(-3, 3);
	std::uniform_int_distribution<int> roomRadius(5, 8);

	const WorldPoint roomAnchors[] = {
		{0.0f, 0.0f},
		{-24.0f, 0.0f},
		{24.0f, 0.0f},
		{-48.0f, 0.0f},
		{48.0f, 0.0f},
		{-48.0f, -25.0f},
		{-24.0f, -25.0f},
		{0.0f, -25.0f},
		{24.0f, -25.0f},
		{48.0f, -25.0f},
		{-48.0f, 25.0f},
		{-24.0f, 25.0f},
		{0.0f, 25.0f},
		{24.0f, 25.0f},
		{48.0f, 25.0f},
		{-36.0f, -49.0f},
		{-12.0f, -49.0f},
		{12.0f, -49.0f},
		{36.0f, -49.0f},
		{-24.0f, 49.0f},
		{0.0f, 49.0f},
		{24.0f, 49.0f},
	};
	const int roomCount = sizeof(roomAnchors) / sizeof(roomAnchors[0]);

	std::vector<Room> rooms;
	for (int roomIndex = 0; roomIndex < roomCount; ++roomIndex)
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
		{1, 3}, {2, 4},
		{3, 5}, {1, 6}, {0, 7}, {2, 8}, {4, 9},
		{3, 10}, {1, 11}, {0, 12}, {2, 13}, {4, 14},
		{5, 15}, {6, 16}, {7, 17}, {9, 18},
		{11, 19}, {12, 20}, {13, 21},
		{5, 6}, {6, 7}, {7, 8}, {8, 9},
		{10, 11}, {11, 12}, {12, 13}, {13, 14},
		{15, 16}, {16, 17}, {17, 18},
		{19, 20}, {20, 21},
	};
	const int connectionCount = sizeof(connections) / sizeof(connections[0]);
	for (int connectionIndex = 0; connectionIndex < connectionCount; ++connectionIndex)
		connectRooms(connections[connectionIndex][0], connections[connectionIndex][1]);

	// A fixed docking bay in the south-west corner connects the personal ship to Erebus.
	const int dockX = halfSize - 57;
	const int dockY = halfSize - 57;
	for (int y = dockY - 4; y <= dockY + 4; ++y)
	{
		for (int x = dockX - 4; x <= dockX + 4; ++x)
		{
			if (x > 0 && y > 0 && x < m_FirstLevelSize - 1 && y < m_FirstLevelSize - 1)
				m_FirstLevelTiles[y * m_FirstLevelSize + x] = 2;
		}
	}
	int dockCorridorX = dockX;
	int dockCorridorY = dockY;
	while (dockCorridorX != rooms[15].centerX)
	{
		carveTile(dockCorridorX, dockCorridorY);
		dockCorridorX += dockCorridorX < rooms[15].centerX ? 1 : -1;
	}
	while (dockCorridorY != rooms[15].centerY)
	{
		carveTile(dockCorridorX, dockCorridorY);
		dockCorridorY += dockCorridorY < rooms[15].centerY ? 1 : -1;
	}
	carveTile(rooms[15].centerX, rooms[15].centerY);

	m_FirstLevelFloorVisuals.clear();
	m_FirstLevelWallVisuals.clear();
	for (int tileY = 0; tileY < m_FirstLevelSize; ++tileY)
	{
		for (int tileX = 0; tileX < m_FirstLevelSize; ++tileX)
		{
			const int tile = m_FirstLevelTiles[tileY * m_FirstLevelSize + tileX];
			const WorldPoint position = {
				static_cast<float>(tileX - halfSize), static_cast<float>(tileY - halfSize)};
			if (tile > 0)
			{
				const float variation =
					static_cast<float>((tileX * 13 + tileY * 7 + m_LevelSeed) % 5) * 0.006f;
				m_FirstLevelFloorVisuals.push_back({position, tile, variation});
			}
			else if (tileX > 0 && tileY > 0 && tileX < m_FirstLevelSize - 1 &&
					 tileY < m_FirstLevelSize - 1)
			{
				const bool besideFloor =
					m_FirstLevelTiles[tileY * m_FirstLevelSize + tileX - 1] > 0 ||
					m_FirstLevelTiles[tileY * m_FirstLevelSize + tileX + 1] > 0 ||
					m_FirstLevelTiles[(tileY - 1) * m_FirstLevelSize + tileX] > 0 ||
					m_FirstLevelTiles[(tileY + 1) * m_FirstLevelSize + tileX] > 0;
				if (besideFloor)
					m_FirstLevelWallVisuals.push_back(position);
			}
		}
	}

	auto toWorld = [halfSize](const Room& room, float offsetX, float offsetY)
	{
		return WorldPoint{static_cast<float>(room.centerX - halfSize) + offsetX,
			static_cast<float>(room.centerY - halfSize) + offsetY};
	};
	m_LevelEnginePositions[0] = toWorld(rooms[3], -2.0f, 0.0f);
	m_LevelEnginePositions[1] = toWorld(rooms[4], 2.0f, 0.0f);
	m_LevelEnginePositions[2] = toWorld(rooms[16], 0.0f, -2.0f);
	m_LevelEnginePositions[3] = toWorld(rooms[20], 0.0f, 2.0f);
	for (int engineIndex = 0; engineIndex < 4; ++engineIndex)
		m_LevelEngineActive[engineIndex] = false;
	m_LevelControlPosition = toWorld(rooms[0], 2.5f, 0.0f);

	m_Player = {-52.0f, -57.0f};
	m_LevelCorpses.clear();
	for (int roomIndex = 1; roomIndex < roomCount; roomIndex += 2)
	{
		const float side = (roomIndex & 2) == 0 ? -1.0f : 1.0f;
		m_LevelCorpses.push_back(toWorld(rooms[roomIndex], side * 1.6f, side));
	}
	m_StoryDocuments.clear();
	m_StoryDocuments.push_back({toWorld(rooms[1], 2.0f, -0.5f), 0, false});
	m_StoryDocuments.push_back({toWorld(rooms[10], -1.5f, 1.0f), 1, false});
	m_StoryDocuments.push_back({toWorld(rooms[21], -1.5f, 1.5f), 2, false});
	m_LevelSurvivors.clear();
	const int squadRoomIndices[] = {3, 6, 9, 11};
	const WorldPoint squadOffsets[] = {{-1.3f, -0.9f}, {1.3f, -0.9f}, {0.0f, 1.4f}};
	for (int survivorIndex = 0; survivorIndex < 20; ++survivorIndex)
	{
		const int pattern = survivorIndex % 10;
		const bool squadMember = survivorIndex < 12;
		const int squadIndex = survivorIndex / 3;
		const int roomIndex = squadMember ? squadRoomIndices[squadIndex] : survivorIndex - 6;
		const WorldPoint offset = squadMember ? squadOffsets[survivorIndex % 3]
			: WorldPoint{survivorIndex % 2 == 0 ? -1.4f : 1.4f,
				survivorIndex % 3 == 0 ? -1.2f : 0.8f};
		const float fear = 0.28f + static_cast<float>((survivorIndex * 17) % 68) / 100.0f;
		const bool armed = squadMember;
		const bool confused = squadMember;
		const WorldPoint position = toWorld(rooms[roomIndex], offset.x, offset.y);
		m_LevelSurvivors.push_back(
			{position, survivorIndex, false, pattern, 3, fear, 0.0f, 0.5f, position, armed, confused, {1.0f, 0.0f}, -1});
	}
	AssignSurvivorSquads(m_LevelSurvivors);
	m_MagazinePickups.clear();
	const int magazineRoomIndices[] = {0, 1, 3, 5, 6, 9, 11, 15, 18, 21};
	for (int magazineIndex = 0; magazineIndex < 10; ++magazineIndex)
	{
		const float side = magazineIndex % 2 == 0 ? -1.0f : 1.0f;
		m_MagazinePickups.push_back(
			{toWorld(rooms[magazineRoomIndices[magazineIndex]], side * 2.2f, 1.8f), false});
	}

	m_Enemies.clear();
	const WorldPoint enemyOffsets[] = {{-2.0f, -1.5f}, {2.0f, 1.0f}, {0.0f, 2.0f}};
	for (int roomIndex = 1; roomIndex < roomCount; ++roomIndex)
	{
		int enemyCount = roomIndex % 4 == 0 ? 1 : 2;
		for (int enemyIndex = 0; enemyIndex < enemyCount; ++enemyIndex)
		{
			WorldPoint position =
				toWorld(rooms[roomIndex], enemyOffsets[enemyIndex].x, enemyOffsets[enemyIndex].y);
			m_Enemies.push_back(
				{position, 3, true, 0.0f, position, static_cast<float>(roomIndex * 3 + enemyIndex)});
		}
	}
	for (size_t patrolIndex = 0; patrolIndex < corridorPatrols.size(); patrolIndex += 2)
		m_Enemies.push_back({corridorPatrols[patrolIndex],
			3,
			true,
			0.0f,
			corridorPatrols[patrolIndex],
			static_cast<float>(patrolIndex) * 0.73f});
	RebuildMonitoringMapCache();
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
