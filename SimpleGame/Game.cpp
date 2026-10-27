#include "stdafx.h"
#include "Game.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <queue>
#include <random>
#include <sstream>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "Dependencies\\freeglut.h"
#include "LocalMonitoringServer.h"

namespace
{
const float Pi = 3.1415926535f;
const float ModelForwardAxisCorrection = Pi * 0.5f;
const float TileWidth = 64.0f;
const float TileHeight = 32.0f;
const WorldPoint TerminalPosition = {-16.0f, -15.0f};
const WorldPoint TutorialWeaponPosition = {-10.0f, -14.0f};
const WorldPoint TutorialRecordPositions[] = {
	{10.0f, -13.0f}, {-11.0f, 12.0f}, {12.0f, 10.0f}};
const WorldPoint SignalPosition = {-9.0f, 13.0f};
const WorldPoint CorePosition = {16.0f, 15.0f};
const WorldPoint CockpitPosition = {-16.0f, 15.0f};
const WorldPoint TutorialPersonalShipDockPosition = {-28.0f, -28.0f};
const WorldPoint FirstLevelPersonalShipDockPosition = {-57.0f, -57.0f};
const WorldPoint TutorialPersonalShipCockpitPosition = {-28.0f, -25.0f};
const WorldPoint FirstLevelPersonalShipCockpitPosition = {-57.0f, -54.0f};
const WorldPoint NoctisSpacePosition = {0.0f, 0.0f};
const WorldPoint ErebusSpacePosition = {0.0f, 650.0f};
const WorldPoint StationSpacePosition = {420.0f, 340.0f};
const WorldPoint FirstLevelWeaponPosition = {-1.5f, 0.0f};
const WorldPoint SpaceBoundaryCenter = {0.0f, 325.0f};
const float SpaceBoundaryHalfWidth = 620.0f;
const float SpaceBoundaryHalfHeight = 720.0f;

float EstimatedGlyphWidth(wchar_t character)
{
	if (character == L' ' || character == L'\t')
		return 6.0f;
	return character < 128 ? 9.0f : 17.0f;
}

float EstimatedTextWidth(const std::wstring& text)
{
	float width = 0.0f;
	for (size_t characterIndex = 0; characterIndex < text.size(); ++characterIndex)
		width += EstimatedGlyphWidth(text[characterIndex]);
	return width;
}

std::vector<std::wstring> WrapTextToWidth(const std::wstring& text, float maximumWidth)
{
	std::vector<std::wstring> lines;
	std::wstring currentLine;
	float currentWidth = 0.0f;
	for (size_t characterIndex = 0; characterIndex < text.size(); ++characterIndex)
	{
		const wchar_t character = text[characterIndex];
		if (character == L'\n')
		{
			lines.push_back(currentLine);
			currentLine.clear();
			currentWidth = 0.0f;
			continue;
		}

		const float glyphWidth = EstimatedGlyphWidth(character);
		if (!currentLine.empty() && currentWidth + glyphWidth > maximumWidth)
		{
			lines.push_back(currentLine);
			currentLine.clear();
			currentWidth = 0.0f;
			if (character == L' ' || character == L'\t')
				continue;
		}
		currentLine.push_back(character);
		currentWidth += glyphWidth;
	}
	if (!currentLine.empty() || lines.empty())
		lines.push_back(currentLine);
	return lines;
}

std::string ToUtf8(const std::wstring& text)
{
	if (text.empty())
		return std::string();
	const int requiredBytes =
		WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
	if (requiredBytes <= 0)
		return std::string();
	std::string utf8(static_cast<size_t>(requiredBytes), '\0');
	WideCharToMultiByte(CP_UTF8,
		0,
		text.c_str(),
		static_cast<int>(text.size()),
		&utf8[0],
		requiredBytes,
		NULL,
		NULL);
	return utf8;
}

std::string JsonEscape(const std::string& value)
{
	std::ostringstream escaped;
	for (size_t index = 0; index < value.size(); ++index)
	{
		const unsigned char character = static_cast<unsigned char>(value[index]);
		switch (character)
		{
			case '"': escaped << "\\\""; break;
			case '\\': escaped << "\\\\"; break;
			case '\b': escaped << "\\b"; break;
			case '\f': escaped << "\\f"; break;
			case '\n': escaped << "\\n"; break;
			case '\r': escaped << "\\r"; break;
			case '\t': escaped << "\\t"; break;
			default:
				if (character < 0x20)
				{
					const char hex[] = "0123456789abcdef";
					escaped << "\\u00" << hex[(character >> 4) & 0x0f] << hex[character & 0x0f];
				}
				else
					escaped << static_cast<char>(character);
		}
	}
	return escaped.str();
}

std::string JsonString(const std::string& value)
{
	return "\"" + JsonEscape(value) + "\"";
}

std::string JsonString(const std::wstring& value)
{
	return JsonString(ToUtf8(value));
}

enum ProjectileOwnerType
{
	ProjectileOwnerPlayer,
	ProjectileOwnerSurvivor
};

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

const wchar_t* SurvivorRoleName(int pattern)
{
	const wchar_t* names[] = {
		L"충격 생존자",
		L"공포에 떠는 난민",
		L"은신한 승무원",
		L"가족을 찾는 난민",
		L"의무관",
		L"경비대원",
		L"정비기사",
		L"통신 분석관",
		L"탈영병",
		L"귀환교도",
	};
	return names[pattern % 10];
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
								   : L"귀환교도: 공허종의 기억 속에서도 우리와 같은 공포를 느꼈습니다.";
	}
}

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

float ProjectileScreenAngle(const Projectile& projectile)
{
	const float screenDirectionX =
		(projectile.velocity.x - projectile.velocity.y) * TileWidth * 0.5f;
	const float screenDirectionY =
		-(projectile.velocity.x + projectile.velocity.y) * TileHeight * 0.5f;
	return std::atan2(screenDirectionY, screenDirectionX);
}

void DrawProjectileVisual(PrototypeRenderer* renderer,
	const Projectile& projectile,
	float x,
	float y,
	float angle)
{
	const bool survivorShot = projectile.ownerType == ProjectileOwnerSurvivor;
	const float bodyLength = projectile.charged ? 31.0f : (survivorShot ? 25.0f : 22.0f);
	const float bodyWidth = projectile.charged ? 10.0f : 7.0f;
	const float red = projectile.charged || survivorShot ? 1.0f : 0.20f;
	const float green = projectile.charged ? 0.46f : (survivorShot ? 0.12f : 0.92f);
	const float blue = projectile.charged ? 0.10f : (survivorShot ? 0.10f : 1.0f);

	const float directionX = std::cos(angle);
	const float directionY = std::sin(angle);
	DrawRotatedRect(renderer,
		x - directionX * bodyLength * 0.38f,
		y - directionY * bodyLength * 0.38f,
		bodyLength * 1.25f,
		bodyWidth * 0.58f,
		angle,
		red,
		green * 0.70f,
		blue,
		0.46f);
	DrawRotatedRect(renderer, x, y, bodyLength, bodyWidth, angle, red, green, blue, 1.0f);
	DrawRotatedRect(renderer,
		x + directionX * bodyLength * 0.27f,
		y + directionY * bodyLength * 0.27f,
		bodyLength * 0.28f,
		bodyWidth * 0.38f,
		angle,
		1.0f,
		projectile.charged ? 0.94f : 1.0f,
		projectile.charged ? 0.54f : 1.0f,
		1.0f);
}
} // namespace

Game::Game(PrototypeRenderer* renderer)
	: m_Renderer(renderer),
	  m_MonitoringServer(new LocalMonitoringServer()),
	  m_MonitoringPublishTimer(0.0f),
	  m_MonitoringSequence(0),
	  m_MonitoringMapRevision(0),
	  m_EnemyAiTimeMs(0.0),
	  m_SurvivorAiTimeMs(0.0),
	  m_ProjectileCollisionTimeMs(0.0),
	  m_MonitoringJsonTimeMs(0.0),
	  m_MonitoringPayloadBytes(0)
{
	Reset();
	if (m_MonitoringServer->Start(8787))
	{
		m_MonitoringServer->SetMapJson(m_MonitoringMapJson);
		PublishMonitoringState(0.0f, true);
	}
}

Game::~Game()
{
	if (m_MonitoringServer != NULL)
	{
		m_MonitoringServer->Stop();
		delete m_MonitoringServer;
		m_MonitoringServer = NULL;
	}
}

void Game::Reset()
{
	m_Mode = OnFoot;
	m_Quest = InspectTerminal;
	m_Paused = false;
	m_CoreRecovered = false;
	m_NoctisSystemsOnline = false;
	m_ErebusSystemsOnline = false;
	for (int recordIndex = 0; recordIndex < 3; ++recordIndex)
		m_TutorialRecordsRead[recordIndex] = false;
	m_SignalChecked = false;
	m_MedicalDroneUsed = false;
	std::memset(m_Keys, 0, sizeof(m_Keys));
	std::memset(m_KeyPressed, 0, sizeof(m_KeyPressed));
	std::memset(m_SpecialKeys, 0, sizeof(m_SpecialKeys));
	m_MousePressed = false;
	m_MouseX = m_Renderer->Width() / 2;
	m_MouseY = m_Renderer->Height() / 2;
	m_Player = {-23.0f, -28.0f};
	m_LastMoveDirection = {1.0f, 0.0f};
	m_PlayerHealth = 100.0f;
	m_PlayerDeathTimer = 0.0f;
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
	for (int engineIndex = 0; engineIndex < 4; ++engineIndex)
		m_LevelEngineActive[engineIndex] = false;
	m_LevelControlPosition = {0.0f, 0.0f};
	// 127 x 127 is approximately eight times the walkable design area of the old 45 x 45 map.
	m_FirstLevelSize = 127;
	m_LevelSeed = static_cast<unsigned int>(std::time(NULL));
	m_FirstLevelTiles.clear();
	m_FirstLevelFloorVisuals.clear();
	m_FirstLevelWallVisuals.clear();
	m_LevelCorpses.clear();
	m_StoryDocuments.clear();
	m_LevelSurvivors.clear();
	m_MagazinePickups.clear();
	if (!m_ModelLibrary.IsLoaded())
		m_ModelLibrary.Load();
	m_ShipPosition = {0.0f, 0.0f};
	m_PreviousShipPosition = m_ShipPosition;
	m_ShipVelocity = {0.0f, 0.0f};
	m_ShipAngle = Pi * 0.5f;
	m_ShipHealth = 100.0f;
	m_ShipImpactFlash = 0.0f;
	m_TransitionTimer = 0.0f;
	m_AsteroidSpawnCounter = 0;
	m_CurrentShip = 0;
	m_TravelDestinationShip = 1;
	m_Message = L"WASD: 이동    E: 상호작용";
	m_MessageTimer = 6.0f;
	m_Dialogue.clear();
	m_DialogueSpeakerPosition = m_Player;
	m_MonitoringPublishTimer = 0.0f;
	m_DialogueTimer = 0.0f;

	m_Obstacles.clear();
	m_Obstacles.push_back({-18.0f, -14.5f, 0.65f, 0.55f, 42.0f, 0.16f, 0.25f, 0.30f, MedicalPod});
	m_Obstacles.push_back({-10.0f, -17.0f, 0.75f, 0.50f, 48.0f, 0.20f, 0.25f, 0.28f, ControlConsole});
	m_Obstacles.push_back({9.5f, -16.5f, 0.80f, 0.55f, 38.0f, 0.24f, 0.20f, 0.25f, CargoCrate});
	m_Obstacles.push_back({16.5f, -9.5f, 0.65f, 0.85f, 54.0f, 0.13f, 0.23f, 0.29f, PowerRelay});
	m_Obstacles.push_back({-18.0f, 10.0f, 0.60f, 0.80f, 50.0f, 0.14f, 0.21f, 0.27f, EngineModule});
	m_Obstacles.push_back({-9.5f, 17.0f, 0.75f, 0.50f, 44.0f, 0.18f, 0.24f, 0.29f, ControlConsole});
	m_Obstacles.push_back({10.0f, 16.5f, 0.55f, 0.85f, 49.0f, 0.14f, 0.23f, 0.30f, CoolantPipe});
	m_Obstacles.push_back({17.5f, 10.0f, 0.80f, 0.55f, 45.0f, 0.17f, 0.25f, 0.31f, CargoCrate});

	m_Enemies.clear();
	m_Enemies.push_back({{15.0f, 11.0f}, 2, false, 0.0f, {15.0f, 11.0f}, 0.0f});
	m_Enemies.push_back({{11.0f, 16.0f}, 2, false, 0.0f, {11.0f, 16.0f}, 1.4f});
	m_Enemies.push_back({{17.0f, 17.0f}, 2, false, 0.0f, {17.0f, 17.0f}, 2.8f});
	m_Enemies.push_back({{10.0f, 10.0f}, 2, false, 0.0f, {10.0f, 10.0f}, 4.2f});

	m_NeutralNpcs.clear();
	m_NeutralNpcs.push_back({{-11.0f, -10.0f}, true, 0.0f, -1, false, 0.0f});
	m_NeutralNpcs.push_back({{11.0f, -10.0f}, true, 1.7f, -1, false, 0.0f});
	m_NeutralNpcs.push_back({{-11.0f, 10.0f}, false, 0.8f, 0, false, 3.0f});
	m_NeutralNpcs.push_back({{16.5f, -16.0f}, false, 2.4f, 1, false, 3.0f});

	// The tutorial introduces survivor states gradually across its four connected rooms.
	m_TutorialSurvivors.clear();
	m_TutorialSurvivors.push_back(
		{{-14.0f, -10.0f}, 100, false, SurvivorFrozen, 3, 0.35f, 0.0f, 0.0f, {-14.0f, -10.0f}, false, false, {1.0f, 0.0f}, -1});
	m_TutorialSurvivors.push_back(
		{{-3.0f, -13.0f}, 101, false, SurvivorTrembling, 3, 0.70f, 0.0f, 0.0f, {-3.0f, -13.0f}, false, false, {1.0f, 0.0f}, -1});
	m_TutorialSurvivors.push_back(
		{{13.0f, -10.0f}, 102, false, SurvivorSearching, 3, 0.48f, 0.0f, 0.0f, {13.0f, -10.0f}, false, false, {1.0f, 0.0f}, -1});
	m_TutorialSurvivors.push_back(
		{{-10.0f, 14.0f}, 103, false, SurvivorMedic, 3, 0.42f, 0.0f, 0.0f, {-10.0f, 14.0f}, false, false, {1.0f, 0.0f}, -1});
	m_TutorialSurvivors.push_back(
		{{10.0f, 13.0f}, 104, false, SurvivorMechanic, 3, 0.55f, 0.0f, 0.0f, {10.0f, 13.0f}, false, false, {1.0f, 0.0f}, -1});
	m_TutorialSurvivors.push_back(
		{{13.5f, 17.0f}, 105, false, SurvivorGuard, 3, 0.82f, 0.0f, 1.2f, {13.5f, 17.0f}, false, false, {1.0f, 0.0f}, -1});
	AssignSurvivorSquads(m_TutorialSurvivors);

	m_Asteroids.clear();
	m_Asteroids.push_back({{-135.0f, 125.0f}, {31.0f, -8.0f}, 25.0f, 0.2f, 0.45f});
	m_Asteroids.push_back({{150.0f, 235.0f}, {-38.0f, -3.0f}, 31.0f, 1.1f, -0.32f});
	m_Asteroids.push_back({{-175.0f, 355.0f}, {43.0f, -11.0f}, 22.0f, 2.2f, 0.60f});
	m_Asteroids.push_back({{145.0f, 455.0f}, {-34.0f, -7.0f}, 28.0f, 0.7f, -0.48f});
	m_Asteroids.push_back({{-120.0f, 565.0f}, {29.0f, -13.0f}, 20.0f, 1.8f, 0.72f});
	RebuildMonitoringMapCache();
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
	m_EnemyAiTimeMs = 0.0;
	m_SurvivorAiTimeMs = 0.0;
	m_ProjectileCollisionTimeMs = 0.0;
	m_MonitoringJsonTimeMs = 0.0;
	m_MonitoringPayloadBytes = 0;
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
		m_DialogueTimer = std::max(0.0f, m_DialogueTimer - deltaSeconds);
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

		bool waitingForPlayerRespawn = false;
		if (m_PlayerHealth <= 0.0f && (m_Mode == OnFoot || m_Mode == FirstLevel))
		{
			waitingForPlayerRespawn = true;
			if (m_PlayerDeathTimer <= 0.0f)
			{
				m_PlayerDeathTimer = 1.35f;
				SetMessage(L"전투복 생체 신호 소실. 의료 비콘이 회수 중입니다.", 1.35f);
			}
			else
			{
				m_PlayerDeathTimer -= deltaSeconds;
				if (m_PlayerDeathTimer <= 0.0f)
				{
					m_Player = m_Mode == FirstLevel ? WorldPoint{-52.0f, -57.0f}
						: WorldPoint{-23.0f, -28.0f};
					m_PlayerHealth = m_PlayerMaxHealth;
					if (m_Mode != FirstLevel)
					{
						for (size_t i = 0; i < m_Enemies.size(); ++i)
						{
							m_Enemies[i].health = 2;
							m_Enemies[i].active = true;
						}
					}
					SetMessage(L"의료 비콘이 전투복을 복구했습니다.", 2.5f);
				}
			}
		}

		if (!waitingForPlayerRespawn && m_Mode == OnFoot)
			UpdateOnFoot(deltaSeconds);
		else if (!waitingForPlayerRespawn && m_Mode == ShipControl)
			UpdateShip(deltaSeconds);
		else if (!waitingForPlayerRespawn && m_Mode == Transition)
		{
			m_TransitionTimer += deltaSeconds;
			if (m_TransitionTimer >= 1.4f)
			{
				m_CurrentShip = m_TravelDestinationShip;
				m_ShipPosition = FixedShipPosition(m_CurrentShip);
				m_PreviousShipPosition = m_ShipPosition;
				m_ShipVelocity = {0.0f, 0.0f};

				if (m_CurrentShip == 1 && m_Quest == FlyToSignal)
				{
					m_Mode = FirstLevel;
					m_Quest = ExploreErebus;
					GenerateFirstLevel();
					m_LevelWeaponCollected = true;
					SetMessage(L"거대 수송선 에레보스: 생존자와 기록을 조사하고 적대 개체를 제압하십시오.", 5.0f);
				}
				else if (m_CurrentShip == 0 && m_Quest == ReturnToNoctis)
				{
					m_Mode = OnFoot;
					m_Quest = BoardPersonalShipToStation;
					m_Player = {-23.0f, -28.0f};
					SetMessage(L"노크티스 도킹 구역으로 귀환했습니다. 개인 우주선으로 정거장 항로를 선택하십시오.", 5.0f);
				}
				else if (m_CurrentShip == 2 && m_Quest == FlyToStation)
				{
					m_Mode = Complete;
					SetMessage(L"거대 우주 정거장에 도착했습니다. 다음 항로가 열렸습니다.", 6.0f);
				}
				else
				{
					m_Mode = OnFoot;
					m_Player = {-23.0f, -28.0f};
					SetMessage(m_CurrentShip == 1 ? L"난민 수송선 에레보스에 도킹했습니다."
												  : L"연구선 노크티스로 귀환했습니다.",
						3.0f);
				}
			}
		}
		else if (!waitingForPlayerRespawn && m_Mode == FirstLevel)
			UpdateFirstLevel(deltaSeconds);
	}

	PublishMonitoringState(deltaSeconds);
	unsigned int activeEnemyCount = 0;
	for (size_t index = 0; index < m_Enemies.size(); ++index)
		if (m_Enemies[index].active && m_Enemies[index].health > 0)
			++activeEnemyCount;
	const std::vector<LevelSurvivor>& activeSurvivorSet =
		m_Mode == FirstLevel ? m_LevelSurvivors : m_TutorialSurvivors;
	unsigned int activeSurvivorCount = 0;
	for (size_t index = 0; index < activeSurvivorSet.size(); ++index)
		if (activeSurvivorSet[index].health > 0.0f)
			++activeSurvivorCount;
	m_Renderer->SetGameUpdateMetrics(m_EnemyAiTimeMs,
		m_SurvivorAiTimeMs,
		m_ProjectileCollisionTimeMs,
		m_MonitoringJsonTimeMs,
		m_MonitoringPayloadBytes,
		static_cast<unsigned int>(m_Projectiles.size()),
		activeEnemyCount,
		activeSurvivorCount);

	std::memset(m_KeyPressed, 0, sizeof(m_KeyPressed));
	m_MousePressed = false;
}

WorldPoint Game::FixedShipPosition(int shipIndex) const
{
	if (shipIndex == 1)
		return ErebusSpacePosition;
	if (shipIndex == 2)
		return StationSpacePosition;
	if (shipIndex == 3)
		return Level02AsterionProfile().spacePosition;
	if (shipIndex == 4)
		return Level03VesperProfile().spacePosition;
	if (shipIndex == 5)
		return Level04HeliosProfile().spacePosition;
	return NoctisSpacePosition;
}

const wchar_t* Game::FixedShipName(int shipIndex) const
{
	if (shipIndex == 1)
		return L"에레보스";
	if (shipIndex == 2)
		return L"거대 우주 정거장";
	if (shipIndex == 3)
		return Level02AsterionProfile().name;
	if (shipIndex == 4)
		return Level03VesperProfile().name;
	if (shipIndex == 5)
		return Level04HeliosProfile().name;
	return L"노크티스";
}

void Game::BeginPersonalShipTravel(int destinationShip)
{
	m_TravelDestinationShip = destinationShip;
	m_ShipPosition = FixedShipPosition(m_CurrentShip);
	m_PreviousShipPosition = m_ShipPosition;
	m_ShipVelocity = {0.0f, 0.0f};
	const WorldPoint destination = FixedShipPosition(destinationShip);
	m_ShipAngle = std::atan2(destination.y - m_ShipPosition.y, destination.x - m_ShipPosition.x);
	m_Mode = ShipControl;
	SetMessage(L"개인 우주선 출항. 방향키로 조종하고 목적지 근처에서 E를 누르십시오.", 5.0f);
}

void Game::UpdateEnemies(float deltaSeconds)
{
	std::vector<LevelSurvivor>& survivors =
		m_Mode == FirstLevel ? m_LevelSurvivors : m_TutorialSurvivors;
	for (size_t i = 0; i < m_Enemies.size(); ++i)
	{
		Enemy& enemy = m_Enemies[i];
		if (!enemy.active || enemy.health <= 0)
			continue;
		enemy.moving = false;
		enemy.attackCooldown = std::max(0.0f, enemy.attackCooldown - deltaSeconds);

		WorldPoint target = m_Player;
		int targetType = 0;
		int targetIndex = -1;
		float targetDistance =
			Length(m_Player.x - enemy.position.x, m_Player.y - enemy.position.y);
		for (size_t survivorIndex = 0; survivorIndex < survivors.size(); ++survivorIndex)
		{
			const LevelSurvivor& survivor = survivors[survivorIndex];
			if (survivor.health <= 0)
				continue;
			const float survivorDistance = Length(
				survivor.position.x - enemy.position.x, survivor.position.y - enemy.position.y);
			if (survivorDistance < targetDistance)
			{
				target = survivor.position;
				targetDistance = survivorDistance;
				targetType = 1;
				targetIndex = static_cast<int>(survivorIndex);
			}
		}
		if (m_Mode == OnFoot)
		{
			for (size_t npcIndex = 0; npcIndex < m_NeutralNpcs.size(); ++npcIndex)
			{
				const NeutralNpc& npc = m_NeutralNpcs[npcIndex];
				if (npc.dead)
					continue;
				const float npcDistance = Length(
					npc.position.x - enemy.position.x, npc.position.y - enemy.position.y);
				if (npcDistance < targetDistance)
				{
					target = npc.position;
					targetDistance = npcDistance;
					targetType = 2;
					targetIndex = static_cast<int>(npcIndex);
				}
			}
		}

		const bool hasCombatTarget = targetDistance <= 10.0f;
		float movementSpeed = 1.15f;
		if (!hasCombatTarget)
		{
			enemy.wanderPhase += deltaSeconds * 0.42f;
			const float wanderRadius = m_Mode == FirstLevel ? 2.8f : 3.8f;
			target = {enemy.homePosition.x + std::cos(enemy.wanderPhase) * wanderRadius,
				enemy.homePosition.y + std::sin(enemy.wanderPhase * 0.83f) * wanderRadius};
			movementSpeed = 0.58f;
		}

		float dx = target.x - enemy.position.x;
		float dy = target.y - enemy.position.y;
		float distance = Length(dx, dy);
		if (distance > 0.65f)
		{
			enemy.facingDirection = {dx / distance, dy / distance};
			enemy.moving = true;
			float nextX = enemy.position.x + dx / distance * movementSpeed * deltaSeconds;
			float nextY = enemy.position.y + dy / distance * movementSpeed * deltaSeconds;
			if ((m_Mode == FirstLevel &&
					IsFirstLevelAreaWalkable(nextX, enemy.position.y, 0.36f)) ||
				(m_Mode != FirstLevel && CanMoveTo(nextX, enemy.position.y, 0.36f)))
				enemy.position.x = nextX;
			if ((m_Mode == FirstLevel &&
					IsFirstLevelAreaWalkable(enemy.position.x, nextY, 0.36f)) ||
				(m_Mode != FirstLevel && CanMoveTo(enemy.position.x, nextY, 0.36f)))
				enemy.position.y = nextY;
		}
		else if (hasCombatTarget && enemy.attackCooldown <= 0.0f)
		{
			if (targetType == 0)
			{
				if (m_DamageCooldown > 0.0f || m_DodgeTimer > 0.0f)
					continue;
				m_PlayerHealth -= 18.0f;
				m_DamageCooldown = 0.8f;
				SetMessage(L"공허종과 접촉했습니다. 계속 움직이십시오.", 1.4f);
			}
			else if (targetType == 1 && targetIndex >= 0)
			{
				LevelSurvivor& survivor = survivors[targetIndex];
				const float voidDamage = survivor.confused ? 0.5f : 1.0f;
				survivor.health -= voidDamage;
				survivor.fear = 1.0f;
				survivor.confused = survivor.armed && survivor.health > 0;
				SetMessage(survivor.health > 0 ? L"공허종이 생존자를 공격했습니다."
										  : L"공허종의 공격으로 생존자가 사망했습니다.",
					1.6f);
			}
			else if (targetType == 2 && targetIndex >= 0)
			{
				NeutralNpc& npc = m_NeutralNpcs[targetIndex];
				npc.health -= 1.0f;
				npc.dead = npc.health <= 0.0f;
				SetMessage(npc.dead ? L"공허종의 공격으로 승무원이 사망했습니다."
								 : L"공허종이 승무원을 공격했습니다.",
					1.6f);
			}
			enemy.attackCooldown = 0.9f;
		}
	}

	if (m_PlayerHealth <= 25.0f && !m_MedicalDroneUsed)
	{
		m_MedicalDroneUsed = true;
		m_PlayerHealth = 70.0f;
		SetMessage(L"응급 의료 드론이 생체 신호를 회복했습니다.", 2.2f);
	}
}

void Game::AssignSurvivorSquads(std::vector<LevelSurvivor>& survivors)
{
	for (size_t index = 0; index < survivors.size(); ++index)
		survivors[index].squadId = -1;
	int nextSquadId = 0;
	for (size_t seedIndex = 0; seedIndex < survivors.size(); ++seedIndex)
	{
		if (survivors[seedIndex].health <= 0 || !survivors[seedIndex].confused ||
			survivors[seedIndex].squadId >= 0)
			continue;
		std::vector<size_t> members(1, seedIndex);
		survivors[seedIndex].squadId = nextSquadId;
		for (size_t memberIndex = 0; memberIndex < members.size(); ++memberIndex)
		{
			const LevelSurvivor& member = survivors[members[memberIndex]];
			for (size_t candidateIndex = 0; candidateIndex < survivors.size(); ++candidateIndex)
			{
				LevelSurvivor& candidate = survivors[candidateIndex];
				if (candidate.health <= 0 || !candidate.confused || candidate.squadId >= 0)
					continue;
				if (Length(candidate.position.x - member.position.x,
						candidate.position.y - member.position.y) > 7.0f)
					continue;
				candidate.squadId = nextSquadId;
				members.push_back(candidateIndex);
			}
		}
		if (members.size() < 2)
		{
			survivors[seedIndex].squadId = -1;
		}
		else
		{
			++nextSquadId;
		}
	}
}

void Game::UpdateLevelSurvivors(float deltaSeconds)
{
	std::vector<LevelSurvivor>& survivors =
		m_Mode == FirstLevel ? m_LevelSurvivors : m_TutorialSurvivors;
	for (size_t survivorIndex = 0; survivorIndex < survivors.size(); ++survivorIndex)
	{
		LevelSurvivor& survivor = survivors[survivorIndex];
		if (survivor.health <= 0)
			continue;

		if (m_Mode == FirstLevel)
			survivor.actionTimer += deltaSeconds;
		survivor.fireCooldown = std::max(0.0f, survivor.fireCooldown - deltaSeconds);

		size_t squadLeaderIndex = survivorIndex;
		int squadRank = 0;
		if (survivor.squadId >= 0)
		{
			for (size_t otherIndex = 0; otherIndex < survivors.size(); ++otherIndex)
			{
				if (survivors[otherIndex].squadId != survivor.squadId ||
					survivors[otherIndex].health <= 0)
					continue;
				if (otherIndex < squadLeaderIndex)
					squadLeaderIndex = otherIndex;
				if (otherIndex < survivorIndex)
					++squadRank;
			}
		}
		const bool followsSquadLeader =
			survivor.squadId >= 0 && squadLeaderIndex != survivorIndex;
		if (survivor.pattern == SurvivorSearching || survivor.pattern == SurvivorMedic ||
			survivor.pattern == SurvivorSignalAnalyst || survivor.pattern == SurvivorGuard ||
			survivor.squadId >= 0)
		{
			float targetX = survivor.homePosition.x;
			float targetY = survivor.homePosition.y;
			float speed = survivor.pattern == SurvivorSearching ? 1.15f : 0.82f;
			if (followsSquadLeader)
			{
				const LevelSurvivor& leader = survivors[squadLeaderIndex];
				const float formationAngle = static_cast<float>(squadRank - 1) * 2.39996f;
				const float formationRadius = 1.05f + 0.22f * static_cast<float>((squadRank - 1) / 4);
				targetX = leader.position.x + std::cos(formationAngle) * formationRadius;
				targetY = leader.position.y + std::sin(formationAngle) * formationRadius;
				speed = 1.05f;
			}
			else if (m_Mode == OnFoot)
			{
				const WorldPoint tutorialRoute[] = {
					{-13.0f, -13.0f}, {13.0f, -13.0f}, {13.0f, 13.0f}, {-13.0f, 13.0f}};
				const int routeIndex =
					(survivor.survivorId + static_cast<int>(survivor.actionTimer)) % 4;
				targetX = tutorialRoute[routeIndex].x;
				targetY = tutorialRoute[routeIndex].y;
			}
			else
			{
				const float radius = survivor.pattern == SurvivorSearching ? 5.5f : 3.0f;
				targetX += std::cos(survivor.actionTimer * 0.24f + survivor.survivorId) * radius;
				targetY += std::sin(survivor.actionTimer * 0.19f + survivor.survivorId) * radius;
			}
			const float deltaX = targetX - survivor.position.x;
			const float deltaY = targetY - survivor.position.y;
			const float distance = Length(deltaX, deltaY);
			if (m_Mode == OnFoot && distance <= 0.45f)
			{
				survivor.actionTimer += 1.0f;
			}
			else if (distance > 0.05f)
			{
				survivor.aimDirection = {deltaX / distance, deltaY / distance};
				const float nextX = survivor.position.x + deltaX / distance * speed * deltaSeconds;
				const float nextY = survivor.position.y + deltaY / distance * speed * deltaSeconds;
				bool separatedX = true;
				bool separatedY = true;
				for (size_t otherIndex = 0; otherIndex < survivors.size(); ++otherIndex)
				{
					if (otherIndex == survivorIndex || survivors[otherIndex].health <= 0)
						continue;
					separatedX = separatedX && Length(nextX - survivors[otherIndex].position.x,
						survivor.position.y - survivors[otherIndex].position.y) >= 0.72f;
					separatedY = separatedY && Length(survivor.position.x - survivors[otherIndex].position.x,
						nextY - survivors[otherIndex].position.y) >= 0.72f;
				}
				const bool canMoveX = separatedX && (m_Mode == FirstLevel
					? IsFirstLevelAreaWalkable(nextX, survivor.position.y, 0.28f)
					: CanMoveTo(nextX, survivor.position.y, 0.28f));
				const bool canMoveY = separatedY && (m_Mode == FirstLevel
					? IsFirstLevelAreaWalkable(survivor.position.x, nextY, 0.28f)
					: CanMoveTo(survivor.position.x, nextY, 0.28f));
				if (canMoveX)
					survivor.position.x = nextX;
				if (canMoveY)
					survivor.position.y = nextY;
			}
		}

		if (!survivor.armed || survivor.fireCooldown > 0.0f)
			continue;

		WorldPoint nearestEnemy = m_Player;
		float nearestEnemyDistance = 10000.0f;
		for (size_t enemyIndex = 0; enemyIndex < m_Enemies.size(); ++enemyIndex)
		{
			const Enemy& enemy = m_Enemies[enemyIndex];
			if (!enemy.active || enemy.health <= 0)
				continue;
			const float enemyDistance = Length(
				enemy.position.x - survivor.position.x, enemy.position.y - survivor.position.y);
			if (enemyDistance < nearestEnemyDistance)
			{
				nearestEnemy = enemy.position;
				nearestEnemyDistance = enemyDistance;
			}
		}

		WorldPoint target = nearestEnemy;
		float targetDistance = nearestEnemyDistance;
		if (survivor.confused)
		{
			WorldPoint nearestSurvivor = m_Player;
			float nearestSurvivorDistance = 10000.0f;
			for (size_t otherIndex = 0; otherIndex < survivors.size(); ++otherIndex)
			{
				if (otherIndex == survivorIndex || survivors[otherIndex].health <= 0 ||
					(survivor.squadId >= 0 &&
						survivors[otherIndex].squadId == survivor.squadId))
					continue;
				const LevelSurvivor& other = survivors[otherIndex];
				const float otherDistance = Length(other.position.x - survivor.position.x,
					other.position.y - survivor.position.y);
				if (otherDistance < nearestSurvivorDistance)
				{
					nearestSurvivor = other.position;
					nearestSurvivorDistance = otherDistance;
				}
			}

			WorldPoint nearestNeutral = m_Player;
			float nearestNeutralDistance = 10000.0f;
			if (m_Mode == OnFoot)
			{
				for (size_t npcIndex = 0; npcIndex < m_NeutralNpcs.size(); ++npcIndex)
				{
					const NeutralNpc& npc = m_NeutralNpcs[npcIndex];
					if (npc.dead)
						continue;
					const float npcDistance = Length(npc.position.x - survivor.position.x,
						npc.position.y - survivor.position.y);
					if (npcDistance < nearestNeutralDistance)
					{
						nearestNeutral = npc.position;
						nearestNeutralDistance = npcDistance;
					}
				}
			}

			const float playerDistance =
				Length(m_Player.x - survivor.position.x, m_Player.y - survivor.position.y);
			target = m_Player;
			targetDistance = playerDistance;
			if (nearestSurvivorDistance < targetDistance)
			{
				target = nearestSurvivor;
				targetDistance = nearestSurvivorDistance;
			}
			if (nearestNeutralDistance < targetDistance)
			{
				target = nearestNeutral;
				targetDistance = nearestNeutralDistance;
			}
			if (nearestEnemyDistance < targetDistance)
			{
				target = nearestEnemy;
				targetDistance = nearestEnemyDistance;
			}

			// Panic rotates the perceived threat so every nearby faction can be misidentified.
			const int panicTarget =
				(survivor.survivorId + static_cast<int>(m_TotalTime / 2.0f)) % 4;
			if (panicTarget == 0 && nearestSurvivorDistance <= 10.0f)
			{
				target = nearestSurvivor;
				targetDistance = nearestSurvivorDistance;
			}
			else if (panicTarget == 1 && nearestNeutralDistance <= 10.0f)
			{
				target = nearestNeutral;
				targetDistance = nearestNeutralDistance;
			}
			else if (panicTarget == 2 && playerDistance <= 10.0f)
			{
				target = m_Player;
				targetDistance = playerDistance;
			}
			else if (panicTarget == 3 && nearestEnemyDistance <= 10.0f)
			{
				target = nearestEnemy;
				targetDistance = nearestEnemyDistance;
			}
		}

		if (targetDistance < 0.45f || targetDistance > 10.0f)
			continue;

		WorldPoint direction = {
			(target.x - survivor.position.x) / targetDistance,
			(target.y - survivor.position.y) / targetDistance,
		};
		survivor.aimDirection = direction;
		m_Projectiles.push_back({survivor.position,
			survivor.position,
			{direction.x * 8.5f, direction.y * 8.5f},
			1.0f,
			0.0f,
			8.5f,
			1,
			false,
			ProjectileOwnerSurvivor,
			static_cast<int>(survivorIndex)});
		survivor.fireCooldown = 1.15f + survivor.fear * 0.7f;
	}
}

void Game::HandleAttack()
{
	if (m_Quest == InspectTerminal || m_Quest == CollectTutorialWeapon ||
		m_AttackCooldown > 0.0f || m_ReloadTimer > 0.0f ||
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
	// Convert the mouse direction in the isometric screen plane back into world space.
	// Player movement remains WASD-driven, while aiming and firing remain independent.
	float aimScreenX = static_cast<float>(m_MouseX - m_Renderer->Width() / 2);
	float aimScreenY = static_cast<float>(m_Renderer->Height() / 2 - m_MouseY + 30);
	float aimScreenLength = Length(aimScreenX, aimScreenY);
	if (aimScreenLength < 1.0f)
	{
		aimScreenX = 1.0f;
		aimScreenY = 0.0f;
		aimScreenLength = 1.0f;
	}
	aimScreenX /= aimScreenLength;
	aimScreenY /= aimScreenLength;
	WorldPoint worldDirection = {aimScreenX / TileWidth - aimScreenY / TileHeight,
		-aimScreenX / TileWidth - aimScreenY / TileHeight};
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
		chargedBullet,
		ProjectileOwnerPlayer,
		-1});
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
		if (m_Mode == OnFoot &&
			!IsTutorialFloor(projectile.position.x, projectile.position.y))
			remove = true;

		for (size_t enemyIndex = 0;
			projectile.ownerType == ProjectileOwnerPlayer && enemyIndex < m_Enemies.size() && !remove;
			++enemyIndex)
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

		if ((m_Mode == FirstLevel || m_Mode == OnFoot) &&
			projectile.ownerType == ProjectileOwnerPlayer)
		{
			std::vector<LevelSurvivor>& survivors =
				m_Mode == FirstLevel ? m_LevelSurvivors : m_TutorialSurvivors;
			for (size_t survivorIndex = 0;
				survivorIndex < survivors.size() && !remove;
				++survivorIndex)
			{
				LevelSurvivor& survivor = survivors[survivorIndex];
				if (survivor.health <= 0 ||
					Length(projectile.position.x - survivor.position.x,
						survivor.position.y - projectile.position.y) >= 0.5f)
				{
					continue;
				}

				survivor.health -= projectile.damage;
				survivor.fear = 1.0f;
				survivor.confused = survivor.armed && survivor.health > 0;
				remove = true;
				SetMessage(survivor.health > 0 ? L"생존자가 공격받아 공황 상태에 빠졌습니다."
											  : L"생존자가 사망했습니다. 이 결과는 되돌릴 수 없습니다.",
					2.2f);
			}
		}
		if (m_Mode == OnFoot && projectile.ownerType == ProjectileOwnerPlayer && !remove)
		{
			for (size_t npcIndex = 0; npcIndex < m_NeutralNpcs.size() && !remove; ++npcIndex)
			{
				NeutralNpc& npc = m_NeutralNpcs[npcIndex];
				if (npc.dead || Length(projectile.position.x - npc.position.x,
					projectile.position.y - npc.position.y) >= 0.5f)
					continue;
				npc.health -= static_cast<float>(projectile.damage);
				npc.dead = npc.health <= 0.0f;
				remove = true;
				SetMessage(npc.dead ? L"승무원이 사망했습니다. 이 결과는 되돌릴 수 없습니다."
								 : L"승무원이 총격으로 부상했습니다.",
					2.0f);
			}
		}

		if ((m_Mode == FirstLevel || m_Mode == OnFoot) &&
			projectile.ownerType == ProjectileOwnerSurvivor && !remove)
		{
			std::vector<LevelSurvivor>& survivors =
				m_Mode == FirstLevel ? m_LevelSurvivors : m_TutorialSurvivors;
			for (size_t enemyIndex = 0; enemyIndex < m_Enemies.size() && !remove; ++enemyIndex)
			{
				Enemy& enemy = m_Enemies[enemyIndex];
				if (!enemy.active || enemy.health <= 0 ||
					Length(projectile.position.x - enemy.position.x,
						projectile.position.y - enemy.position.y) >= 0.55f)
					continue;
				enemy.health -= projectile.damage;
				remove = true;
				if (enemy.health <= 0)
					enemy.active = false;
				SetMessage(enemy.health > 0 ? L"경비대원이 공허종을 공격했습니다."
										: L"경비대원이 공허종을 제거했습니다.",
					1.4f);
			}
			if (!remove &&
				Length(projectile.position.x - m_Player.x, projectile.position.y - m_Player.y) < 0.5f)
			{
				m_PlayerHealth -= 9.0f;
				m_DamageCooldown = 0.35f;
				remove = true;
				SetMessage(L"공황 상태의 생존자가 플레이어를 적으로 오인했습니다.", 1.8f);
			}

			for (size_t survivorIndex = 0;
				survivorIndex < survivors.size() && !remove;
				++survivorIndex)
			{
				if (static_cast<int>(survivorIndex) == projectile.ownerId)
					continue;

				LevelSurvivor& survivor = survivors[survivorIndex];
				if (survivor.health <= 0 ||
					Length(projectile.position.x - survivor.position.x,
						projectile.position.y - survivor.position.y) >= 0.5f)
				{
					continue;
				}

				survivor.health -= 1.0f;
				survivor.fear = std::min(1.0f, survivor.fear + 0.35f);
				survivor.confused = survivor.armed && survivor.health > 0;
				remove = true;
				SetMessage(L"피아식별 실패로 생존자 사이에 총격이 발생했습니다.", 2.0f);
			}
			if (m_Mode == OnFoot)
			{
				for (size_t npcIndex = 0; npcIndex < m_NeutralNpcs.size() && !remove; ++npcIndex)
				{
					NeutralNpc& npc = m_NeutralNpcs[npcIndex];
					if (npc.dead || Length(projectile.position.x - npc.position.x,
						projectile.position.y - npc.position.y) >= 0.5f)
						continue;
					npc.health -= 1.0f;
					npc.dead = npc.health <= 0.0f;
					remove = true;
					SetMessage(npc.dead ? L"공황 상태의 오인 사격으로 승무원이 사망했습니다."
									: L"승무원이 오인 사격으로 부상했습니다.",
						2.0f);
				}
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
	for (size_t i = 0; i < m_TutorialSurvivors.size(); ++i)
	{
		LevelSurvivor& survivor = m_TutorialSurvivors[i];
		if (survivor.health <= 0 || !IsNear(survivor.position, 1.3f))
			continue;
		SetDialogue(SurvivorDialogue(survivor), survivor.position, 5.0f);
		survivor.talked = true;
		if (survivor.confused)
		{
			survivor.confused = false;
			survivor.fear = std::max(0.45f, survivor.fear - 0.30f);
		}
		return;
	}
	for (size_t i = 0; i < m_NeutralNpcs.size(); ++i)
	{
		NeutralNpc& npc = m_NeutralNpcs[i];
		if (npc.dead || !IsNear(npc.position, 1.25f))
			continue;
		if (npc.dialogueId == 0)
		{
			SetDialogue(npc.talked ? L"통신장교 서윤: 공허종의 신호에도 구조 요청이 섞여 있었어요. "
									L"지휘부는 그 부분을 지웠습니다."
								  : L"통신장교 서윤: 우리가 먼저 그 문을 열었어요... 그런데 모두 "
									L"적이 먼저 공격했다고 믿고 있어요.",
				npc.position, 5.0f);
		}
		else
		{
			SetDialogue(npc.talked ? L"정비사 라울: 살아남으면 진실을 전해 주세요. 복수만으로는 이 "
									L"함선을 다시 움직일 수 없습니다."
								  : L"정비사 라울: 냉각관이 터질 때 동료들을 두고 도망쳤습니다... "
									L"아직도 그 소리가 들립니다.",
				npc.position, 5.0f);
		}
		npc.talked = true;
		return;
	}
	if (m_Quest == InspectTerminal && IsNear(TerminalPosition, 1.15f))
	{
		m_Quest = CollectTutorialWeapon;
		SetMessage(L"함선 AI: 무기 보관함이 해제되었습니다. 펄스 카빈을 회수하십시오.", 3.2f);
		return;
	}
	if (m_Quest == CollectTutorialWeapon && IsNear(TutorialWeaponPosition, 1.2f))
	{
		m_Quest = FindCore;
		SetMessage(L"펄스 카빈을 획득했습니다. 기관실의 공허종을 처치하고 항법 코어를 회수하십시오.", 4.0f);
		return;
	}
	for (int recordIndex = 0; recordIndex < 3; ++recordIndex)
	{
		if (m_CurrentShip != 0 || !IsNear(TutorialRecordPositions[recordIndex], 1.05f))
			continue;
		m_TutorialRecordsRead[recordIndex] = true;
		const wchar_t* records[] = {
			L"승무 기록: 누구도 이 전쟁을 원하지 않았다. 경보가 울릴 때마다 사람들은 서로의 손을 잡았다.",
			L"보안 기록: 선내 침입체는 심연문 실험실에서 시작되었다. 외부 침입이라는 공식 발표와 다르다.",
			L"항법 기록: 에레보스의 구조 신호가 반복된다. 생존자가 있다면 아직 늦지 않았을지도 모른다."};
		SetMessage(records[recordIndex], 5.0f);
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
		m_Quest = ActivateNoctisSystems;
		SetMessage(L"항법 코어를 회수했습니다. 함교 제어 장치에서 노크티스의 시스템을 복구하십시오.", 4.0f);
		return;
	}
	if (m_CurrentShip == 0 && !m_CoreRecovered && IsNear(CockpitPosition, 1.25f))
	{
		SetMessage(L"항법 제어 장치가 응답하지 않습니다. 기관실에서 항법 코어를 먼저 회수하십시오.", 4.0f);
		return;
	}
	if (m_Quest == ActivateNoctisSystems && IsNear(CockpitPosition, 1.25f))
	{
		m_NoctisSystemsOnline = true;
		m_Quest = BoardPersonalShipToErebus;
		SetMessage(L"노크티스 시스템 복구 완료. 도킹 통로를 따라 개인 우주선으로 복귀하십시오.", 5.0f);
		return;
	}
	if (m_Quest == BoardPersonalShipToErebus &&
		IsNear(TutorialPersonalShipCockpitPosition, 1.6f))
	{
		m_Quest = FlyToSignal;
		BeginPersonalShipTravel(1);
		return;
	}
	if (m_Quest == BoardPersonalShipToStation &&
		IsNear(TutorialPersonalShipCockpitPosition, 1.6f))
	{
		m_Quest = FlyToStation;
		BeginPersonalShipTravel(2);
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
	const bool boostActive = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0;
	if (boostActive)
		thrust += 135.0f;
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
	const float boundaryAmount =
		std::fabs(m_ShipPosition.x - SpaceBoundaryCenter.x) / SpaceBoundaryHalfWidth +
		std::fabs(m_ShipPosition.y - SpaceBoundaryCenter.y) / SpaceBoundaryHalfHeight;
	if (boundaryAmount > 1.0f)
	{
		const float boundaryScale = 0.985f / boundaryAmount;
		m_ShipPosition.x = SpaceBoundaryCenter.x +
			(m_ShipPosition.x - SpaceBoundaryCenter.x) * boundaryScale;
		m_ShipPosition.y = SpaceBoundaryCenter.y +
			(m_ShipPosition.y - SpaceBoundaryCenter.y) * boundaryScale;
		m_ShipVelocity.x *= -0.25f;
		m_ShipVelocity.y *= -0.25f;
		SetMessage(L"항법 경계에 도달했습니다. 항로 안쪽으로 복귀하십시오.", 1.5f);
	}
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
	const WorldPoint destination = FixedShipPosition(m_TravelDestinationShip);
	float destinationDistance =
		Length(m_ShipPosition.x - destination.x, m_ShipPosition.y - destination.y);
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

		if (!IsInsideSpaceBoundary(asteroid.position))
		{
			RespawnAsteroid(asteroid, i);
			previousAsteroidPosition = asteroid.position;
		}

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

void Game::RespawnAsteroid(Asteroid& asteroid, size_t asteroidIndex)
{
	std::mt19937 random(
		m_LevelSeed + (++m_AsteroidSpawnCounter * 2654435761u) + static_cast<unsigned int>(asteroidIndex));
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
	std::uniform_real_distribution<float> angleDistribution(0.0f, Pi * 2.0f);
	std::uniform_real_distribution<float> speedDistribution(24.0f, 48.0f);
	std::uniform_real_distribution<float> radiusDistribution(19.0f, 32.0f);
	float normalizedX = 0.0f;
	float normalizedY = 0.0f;
	do
	{
		normalizedX = unit(random);
		normalizedY = unit(random);
		asteroid.position = {SpaceBoundaryCenter.x + normalizedX * SpaceBoundaryHalfWidth,
			SpaceBoundaryCenter.y + normalizedY * SpaceBoundaryHalfHeight};
	} while (std::fabs(normalizedX) + std::fabs(normalizedY) > 0.82f ||
		Length(asteroid.position.x - m_ShipPosition.x, asteroid.position.y - m_ShipPosition.y) < 115.0f);
	const float angle = angleDistribution(random);
	const float speed = speedDistribution(random);
	asteroid.velocity = {std::cos(angle) * speed, std::sin(angle) * speed};
	asteroid.radius = radiusDistribution(random);
	asteroid.rotation = angleDistribution(random);
	asteroid.rotationSpeed = unit(random) * 0.75f;
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

bool Game::AllStoryDocumentsRead() const
{
	if (m_StoryDocuments.empty())
		return false;
	for (size_t documentIndex = 0; documentIndex < m_StoryDocuments.size(); ++documentIndex)
		if (!m_StoryDocuments[documentIndex].read)
			return false;
	return true;
}

bool Game::AreLevelEnginesActive() const
{
	for (int engineIndex = 0; engineIndex < 4; ++engineIndex)
		if (!m_LevelEngineActive[engineIndex])
			return false;
	return true;
}

bool Game::AreLevelHostilesCleared() const
{
	if (!AllEnemiesDefeated())
		return false;
	for (size_t survivorIndex = 0; survivorIndex < m_LevelSurvivors.size(); ++survivorIndex)
	{
		const LevelSurvivor& survivor = m_LevelSurvivors[survivorIndex];
		if (survivor.squadId >= 0 && survivor.health > 0.0f)
			return false;
	}
	return true;
}

bool Game::IsInsideSpaceBoundary(const WorldPoint& point) const
{
	const float normalized = std::fabs(point.x - SpaceBoundaryCenter.x) / SpaceBoundaryHalfWidth +
		std::fabs(point.y - SpaceBoundaryCenter.y) / SpaceBoundaryHalfHeight;
	return normalized <= 1.0f;
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
	if (m_Mode == Complete)
		return L"거대 우주 정거장 도착: 다음 장으로 이어지는 항로를 확보했습니다";
	if (m_Mode == FirstLevel)
	{
		if (m_Quest == BoardPersonalShipToNoctis)
			return L"에레보스: 도킹 구역의 개인 우주선으로 복귀하십시오 [E]";
		if (!AreLevelEnginesActive())
			return L"에레보스: 보조 엔진 네 개를 가동하십시오";
		if (!AreLevelHostilesCleared())
			return L"에레보스: 공격하는 생존자와 공허종을 처치하십시오";
		return L"에레보스: 중앙 시스템 제어 장치에서 함선을 복구하십시오 [E]";
	}

	if ((m_Mode == ShipControl || m_Mode == Transition) && m_Quest == ReturnToNoctis)
		return L"목표: 연구선 노크티스에 도킹하십시오";
	if ((m_Mode == ShipControl || m_Mode == Transition) && m_Quest == FlyToStation)
		return L"목표: 거대 우주 정거장에 도킹하십시오";
	if (m_Quest == InspectTerminal)
		return L"목표: 의료실 단말기를 조사하십시오 [E]";
	if (m_Quest == CollectTutorialWeapon)
		return L"목표: 무기 보관함에서 펄스 카빈을 획득하십시오 [E]";
	if (m_Quest == FindCore && !AllEnemiesDefeated())
		return L"목표: 기관실로 이동하여 침입체를 제거하십시오";
	if (m_Quest == FindCore)
		return L"목표: 항법 코어를 회수하십시오 [E]";
	if (m_Quest == ActivateNoctisSystems)
		return L"목표: 함교에서 노크티스 시스템을 복구하십시오 [E]";
	if (m_Quest == BoardPersonalShipToErebus)
		return L"목표: 도킹 구역의 개인 우주선으로 복귀하십시오 [E]";
	if (m_Quest == FlyToSignal)
		return L"목표: 개인 우주선으로 에레보스에 도킹하십시오";
	if (m_Quest == BoardPersonalShipToStation)
		return L"목표: 개인 우주선으로 복귀하여 정거장 항로를 선택하십시오 [E]";
	return L"현재 위치: 연구선 노크티스";
}

std::vector<std::wstring> Game::ObjectiveLines() const
{
	std::vector<std::wstring> objectives;
	if (m_Mode == FirstLevel)
	{
		if (m_Quest == BoardPersonalShipToNoctis)
		{
			objectives.push_back(L"[완료] 에레보스 시스템 복구");
			objectives.push_back(L"[진행] 도킹 구역의 개인 우주선으로 복귀");
			return objectives;
		}
		int activeEngineCount = 0;
		for (int engineIndex = 0; engineIndex < 4; ++engineIndex)
			if (m_LevelEngineActive[engineIndex])
				++activeEngineCount;

		int defeatedVoidCount = 0;
		for (size_t enemyIndex = 0; enemyIndex < m_Enemies.size(); ++enemyIndex)
			if (m_Enemies[enemyIndex].health <= 0)
				++defeatedVoidCount;

		int hostilePanicTotal = 0;
		int defeatedPanicCount = 0;
		for (size_t survivorIndex = 0; survivorIndex < m_LevelSurvivors.size(); ++survivorIndex)
		{
			const LevelSurvivor& survivor = m_LevelSurvivors[survivorIndex];
			if (survivor.squadId < 0)
				continue;
			++hostilePanicTotal;
			if (survivor.health <= 0.0f)
				++defeatedPanicCount;
		}

		int readDocumentCount = 0;
		for (size_t documentIndex = 0; documentIndex < m_StoryDocuments.size(); ++documentIndex)
			if (m_StoryDocuments[documentIndex].read)
				++readDocumentCount;

		objectives.push_back(std::wstring(activeEngineCount == 4 ? L"[완료] " : L"[진행] ") +
			L"보조 엔진 가동  [" + std::to_wstring(activeEngineCount) + L"/4]");
		objectives.push_back(
			std::wstring(defeatedVoidCount == static_cast<int>(m_Enemies.size()) ? L"[완료] " : L"[진행] ") +
			L"공허종 제압  [" + std::to_wstring(defeatedVoidCount) + L"/" +
			std::to_wstring(m_Enemies.size()) + L"]");
		objectives.push_back(std::wstring(defeatedPanicCount == hostilePanicTotal ? L"[완료] " : L"[진행] ") +
			L"적대 공황 생존자 제압  [" + std::to_wstring(defeatedPanicCount) + L"/" +
			std::to_wstring(hostilePanicTotal) + L"]");
		objectives.push_back(std::wstring(AllStoryDocumentsRead() ? L"[완료/선택] " : L"[선택] ") +
			L"전쟁 자료 열람  [" + std::to_wstring(readDocumentCount) + L"/" +
			std::to_wstring(m_StoryDocuments.size()) + L"]");
		objectives.push_back(AreLevelEnginesActive() && AreLevelHostilesCleared()
			? L"[가능] 중앙 시스템 제어 장치 사용"
			: L"[잠김] 중앙 시스템 제어 장치 사용");
		return objectives;
	}
	objectives.push_back(ObjectiveText());
	return objectives;
}

std::string Game::BuildMonitoringJson() const
{
	const char* modeName = "on_foot";
	switch (m_Mode)
	{
		case ShipControl: modeName = "ship_control"; break;
		case Transition: modeName = "transition"; break;
		case FirstLevel: modeName = "erebus_level_1"; break;
		case Complete: modeName = "complete"; break;
		default: break;
	}
	const char* currentRegion = m_CurrentShip == 2 ? "station" : (m_CurrentShip == 1 ? "erebus" : "noctis");
	const bool erebusGenerated = !m_FirstLevelTiles.empty();

	int noctisNpcAlive = 0;
	for (size_t index = 0; index < m_TutorialSurvivors.size(); ++index)
		if (m_TutorialSurvivors[index].health > 0.0f)
			++noctisNpcAlive;
	for (size_t index = 0; index < m_NeutralNpcs.size(); ++index)
		if (!m_NeutralNpcs[index].dead && m_NeutralNpcs[index].health > 0.0f)
			++noctisNpcAlive;
	const int noctisNpcTotal =
		static_cast<int>(m_TutorialSurvivors.size() + m_NeutralNpcs.size());

	int erebusNpcAlive = 0;
	for (size_t index = 0; index < m_LevelSurvivors.size(); ++index)
		if (m_LevelSurvivors[index].health > 0.0f)
			++erebusNpcAlive;
	const int erebusNpcTotal = static_cast<int>(m_LevelSurvivors.size());

	int activeMobCount = 0;
	for (size_t index = 0; index < m_Enemies.size(); ++index)
		if (m_Enemies[index].health > 0)
			++activeMobCount;
	const int currentMobTotal = static_cast<int>(m_Enemies.size());
	const int noctisMobAlive = erebusGenerated ? 0 : activeMobCount;
	const int noctisMobTotal = 4;
	const int erebusMobAlive = erebusGenerated ? activeMobCount : 0;
	const int erebusMobTotal = erebusGenerated ? currentMobTotal : 0;

	const std::string noctisStatus = m_CurrentShip == 0
		? (m_CoreRecovered ? "current_secured" : "current_emergency")
		: (m_CoreRecovered ? "secured" : "emergency");
	const std::string erebusStatus = !erebusGenerated
		? "unvisited"
		: (AreLevelHostilesCleared() ? "secured" : (m_CurrentShip == 1 ? "current_contested" : "contested"));
	const std::string stationStatus = m_CurrentShip == 2 ? "current_reached" : "unvisited";

	std::ostringstream json;
	json << std::fixed << std::setprecision(2);
	json << "{\"schemaVersion\":4,\"sequence\":" << m_MonitoringSequence
		 << ",\"mapRevision\":" << m_MonitoringMapRevision
		 << ",\"shipCount\":5"
		 << ",\"gameMode\":" << JsonString(modeName)
		 << ",\"currentRegion\":" << JsonString(currentRegion)
		 << ",\"regions\":[";
	json << "{\"id\":\"noctis\",\"name\":" << JsonString(L"연구선 노크티스")
		 << ",\"status\":" << JsonString(noctisStatus)
		 << ",\"npcAlive\":" << noctisNpcAlive << ",\"npcTotal\":" << noctisNpcTotal
		 << ",\"mobAlive\":" << noctisMobAlive << ",\"mobTotal\":" << noctisMobTotal
		 << ",\"atmosphere\":{\"profile\":\"emergency_recovery\",\"fear\":64,\"contamination\":18,\"power\":"
		 << (m_NoctisSystemsOnline ? 82 : 24) << "}"
		 << ",\"mapAvailable\":true,\"bounds\":{\"minX\":-34,\"maxX\":22,\"minY\":-34,\"maxY\":22}},";
	json << "{\"id\":\"erebus\",\"name\":" << JsonString(L"수송선 에레보스")
		 << ",\"status\":" << JsonString(erebusStatus)
		 << ",\"npcAlive\":" << erebusNpcAlive << ",\"npcTotal\":" << erebusNpcTotal
		 << ",\"mobAlive\":" << erebusMobAlive << ",\"mobTotal\":" << erebusMobTotal
		 << ",\"atmosphere\":{\"profile\":\"contested_refugee_ship\",\"fear\":88,\"contamination\":73,\"power\":"
		 << (m_ErebusSystemsOnline ? 68 : 19) << "}"
		 << ",\"mapAvailable\":" << (erebusGenerated ? "true" : "false")
		 << ",\"bounds\":{\"minX\":-64,\"maxX\":64,\"minY\":-64,\"maxY\":64}},";
	const ShipLevelProfile remoteShipProfiles[] = {
		Level02AsterionProfile(), Level03VesperProfile(), Level04HeliosProfile()};
	for (int profileIndex = 0; profileIndex < 3; ++profileIndex)
	{
		const ShipLevelProfile& profile = remoteShipProfiles[profileIndex];
		json << "{\"id\":" << JsonString(profile.id) << ",\"name\":" << JsonString(profile.name)
			 << ",\"status\":" << JsonString(profile.status)
			 << ",\"npcAlive\":" << profile.npcAlive << ",\"npcTotal\":" << profile.npcTotal
			 << ",\"mobAlive\":" << profile.voidAlive << ",\"mobTotal\":" << profile.voidTotal
			 << ",\"atmosphere\":{\"profile\":" << JsonString(profile.atmosphereProfile)
			 << ",\"fear\":" << profile.fear << ",\"contamination\":" << profile.contamination
			 << ",\"power\":" << profile.power << "}"
			 << ",\"mapAvailable\":false,\"bounds\":{\"minX\":-64,\"maxX\":64,\"minY\":-64,\"maxY\":64}},";
	}
	json << "{\"id\":\"station\",\"name\":" << JsonString(L"거대 우주 정거장")
		 << ",\"status\":" << JsonString(stationStatus)
		 << ",\"npcAlive\":0,\"npcTotal\":0,\"mobAlive\":0,\"mobTotal\":0"
		 << ",\"atmosphere\":{\"profile\":\"unknown\",\"fear\":0,\"contamination\":0,\"power\":0}"
		 << ",\"mapAvailable\":false,\"bounds\":{\"minX\":-1,\"maxX\":1,\"minY\":-1,\"maxY\":1}}],";
	const bool playerVisible = m_Mode == OnFoot || m_Mode == FirstLevel;
	json << "\"player\":{\"visible\":" << (playerVisible ? "true" : "false")
		 << ",\"region\":" << JsonString(currentRegion)
		 << ",\"position\":{\"x\":" << m_Player.x << ",\"y\":" << m_Player.y << "}},";
	json << "\"npcs\":[";
	bool firstNpc = true;
	auto appendNpc = [&json, &firstNpc](const std::string& id,
		const std::wstring& name,
		const char* region,
		const char* state,
		float health,
		float maxHealth,
		float fear,
		bool armed,
		int squad,
		const WorldPoint& position)
	{
		if (!firstNpc)
			json << ',';
		firstNpc = false;
		json << "{\"id\":" << JsonString(id) << ",\"name\":" << JsonString(name)
			 << ",\"region\":" << JsonString(region) << ",\"state\":" << JsonString(state)
			 << ",\"health\":" << std::max(0.0f, health) << ",\"maxHealth\":" << maxHealth
			 << ",\"fear\":" << std::max(0.0f, fear)
			 << ",\"armed\":" << (armed ? "true" : "false")
			 << ",\"squad\":" << (squad >= 0 ? std::to_string(squad + 1) : "null")
			 << ",\"position\":{\"x\":" << position.x << ",\"y\":" << position.y << "}}";
	};

	for (size_t index = 0; index < m_TutorialSurvivors.size(); ++index)
	{
		const LevelSurvivor& npc = m_TutorialSurvivors[index];
		const char* state = npc.health <= 0.0f
			? "dead"
			: (npc.confused ? "panic" : (npc.fear >= 0.60f ? "frightened" : "stable"));
		appendNpc("noctis_survivor_" + std::to_string(index),
			SurvivorRoleName(npc.pattern), "noctis", state, npc.health, 3.0f, npc.fear, npc.armed, npc.squadId, npc.position);
	}
	for (size_t index = 0; index < m_NeutralNpcs.size(); ++index)
	{
		const NeutralNpc& npc = m_NeutralNpcs[index];
		const std::wstring name = npc.dialogueId == 0 ? L"통신장교 서윤" :
			(npc.dialogueId == 1 ? L"정비사 라울" : L"노크티스 승무원");
		appendNpc("noctis_crew_" + std::to_string(index), name, "noctis",
			npc.dead || npc.health <= 0.0f ? "dead" : "stable", npc.health, 3.0f, 0.0f, false, -1, npc.position);
	}
	for (size_t index = 0; index < m_LevelSurvivors.size(); ++index)
	{
		const LevelSurvivor& npc = m_LevelSurvivors[index];
		const char* state = npc.health <= 0.0f
			? "dead"
			: (npc.confused ? "panic" : (npc.fear >= 0.60f ? "frightened" : "stable"));
		appendNpc("erebus_survivor_" + std::to_string(index),
			SurvivorRoleName(npc.pattern), "erebus", state, npc.health, 3.0f, npc.fear, npc.armed, npc.squadId, npc.position);
	}
	for (int shipIndex = 0; shipIndex < 3; ++shipIndex)
	{
		const ShipLevelProfile& profile = remoteShipProfiles[shipIndex];
		for (int npcIndex = 0; npcIndex < 20; ++npcIndex)
		{
			const bool alive = npcIndex < profile.npcAlive;
			const float fear = alive
				? 0.38f + static_cast<float>((npcIndex * 13 + shipIndex * 19) % 57) / 100.0f
				: 1.0f;
			const char* state = !alive ? "dead" : (fear >= 0.78f ? "panic" : "frightened");
			const WorldPoint position = {
				-48.0f + static_cast<float>((npcIndex % 5) * 21),
				-46.0f + static_cast<float>((npcIndex / 5) * 27)};
			appendNpc(std::string(profile.id) + "_survivor_" +
					std::to_string(npcIndex),
				L"원격 생존 신호 " + std::to_wstring(npcIndex + 1),
				profile.id,
				state,
				alive ? 3.0f : 0.0f,
				3.0f,
				fear,
				npcIndex % 3 == 0,
				-1,
				position);
		}
	}
	json << "],\"mobs\":[";
	bool firstMob = true;
	for (size_t index = 0; index < m_Enemies.size(); ++index)
	{
		const Enemy& enemy = m_Enemies[index];
		if (enemy.health <= 0)
			continue;
		if (!firstMob)
			json << ',';
		firstMob = false;
		json << "{\"id\":" << JsonString(std::string(erebusGenerated ? "erebus_void_" : "noctis_void_") + std::to_string(index))
			 << ",\"region\":" << JsonString(erebusGenerated ? "erebus" : "noctis")
			 << ",\"health\":" << enemy.health << ",\"maxHealth\":" << (erebusGenerated ? 3 : 2)
			 << ",\"position\":{\"x\":" << enemy.position.x << ",\"y\":" << enemy.position.y << "}}";
	}
	json << "],\"objects\":[";
	bool firstObject = true;
	auto appendObject = [&json, &firstObject](const std::string& id,
		const std::wstring& name,
		const char* region,
		const char* type,
		const WorldPoint& position)
	{
		if (!firstObject)
			json << ',';
		firstObject = false;
		json << "{\"id\":" << JsonString(id) << ",\"name\":" << JsonString(name)
			 << ",\"region\":" << JsonString(region) << ",\"type\":" << JsonString(type)
			 << ",\"position\":{\"x\":" << position.x << ",\"y\":" << position.y << "}}";
	};
	if (m_Quest == InspectTerminal)
		appendObject("noctis_terminal", L"함선 터미널", "noctis", "terminal", TerminalPosition);
	if (m_Quest == CollectTutorialWeapon)
		appendObject("noctis_weapon", L"훈련용 무기", "noctis", "pickup", TutorialWeaponPosition);
	for (size_t index = 0; index < 3; ++index)
		appendObject("noctis_record_" + std::to_string(index), L"함선 기록", "noctis", "document", TutorialRecordPositions[index]);
	if (!m_CoreRecovered)
		appendObject("noctis_core", L"항법 코어", "noctis", "core", CorePosition);
	if (m_CoreRecovered && !m_SignalChecked)
		appendObject("noctis_signal", L"구조 신호 단말", "noctis", "terminal", SignalPosition);
	appendObject("noctis_cockpit", L"조종 장치", "noctis", "console", CockpitPosition);
	appendObject("noctis_personal_ship", L"개인 우주선", "noctis", "ship", TutorialPersonalShipDockPosition);
	if (erebusGenerated)
	{
		appendObject("erebus_personal_ship", L"개인 우주선", "erebus", "ship", FirstLevelPersonalShipDockPosition);
		if (!m_LevelWeaponCollected)
			appendObject("erebus_weapon", L"펄스 소총", "erebus", "pickup", FirstLevelWeaponPosition);
		for (int index = 0; index < 4; ++index)
			if (!m_LevelEngineActive[index])
				appendObject("erebus_engine_" + std::to_string(index), L"보조 엔진", "erebus", "engine", m_LevelEnginePositions[index]);
		appendObject("erebus_control", L"시스템 제어 장치", "erebus", "console", m_LevelControlPosition);
		for (size_t index = 0; index < m_StoryDocuments.size(); ++index)
			appendObject("erebus_document_" + std::to_string(index), L"현장 문서", "erebus", "document", m_StoryDocuments[index].position);
		for (size_t index = 0; index < m_MagazinePickups.size(); ++index)
			if (!m_MagazinePickups[index].collected)
				appendObject("erebus_magazine_" + std::to_string(index), L"예비 탄창", "erebus", "pickup", m_MagazinePickups[index].position);
	}
	json << "]}";
	return json.str();
}

std::string Game::BuildMonitoringMapJson() const
{
	std::ostringstream json;
	json << std::fixed << std::setprecision(2);
	json << "{\"schemaVersion\":3,\"mapRevision\":" << m_MonitoringMapRevision
		 << ",\"terrain\":{\"noctis\":[";
	bool firstTerrainTile = true;
	for (int tileY = -34; tileY <= 22; ++tileY)
	{
		for (int tileX = -34; tileX <= 22; ++tileX)
		{
			if (!IsTutorialFloor(static_cast<float>(tileX), static_cast<float>(tileY)))
				continue;
			bool roomTile = false;
			const int roomCenters[] = {-13, 13};
			for (int centerX = 0; centerX < 2 && !roomTile; ++centerX)
				for (int centerY = 0; centerY < 2 && !roomTile; ++centerY)
					roomTile = std::abs(tileX - roomCenters[centerX]) <= 8 &&
						std::abs(tileY - roomCenters[centerY]) <= 8;
			if (!firstTerrainTile)
				json << ',';
			firstTerrainTile = false;
			json << "[" << tileX << ',' << tileY << ',' << (roomTile ? 2 : 1) << "]";
		}
	}
	json << "],\"erebus\":[";
	firstTerrainTile = true;
	for (size_t index = 0; index < m_FirstLevelFloorVisuals.size(); ++index)
	{
		const MapVisualTile& tile = m_FirstLevelFloorVisuals[index];
		if (!firstTerrainTile)
			json << ',';
		firstTerrainTile = false;
		json << "[" << tile.position.x << ',' << tile.position.y << ',' << tile.type << "]";
	}
	json << "]}}";
	return json.str();
}

void Game::RebuildMonitoringMapCache()
{
	++m_MonitoringMapRevision;
	m_MonitoringMapJson = BuildMonitoringMapJson();
	RebuildGameplayMinimapCache();
	if (m_MonitoringServer != NULL && m_MonitoringServer->IsRunning())
		m_MonitoringServer->SetMapJson(m_MonitoringMapJson);
}

void Game::RebuildGameplayMinimapCache()
{
	m_TutorialMinimapTiles.clear();
	for (int cellY = -34; cellY <= 22; cellY += 2)
	{
		for (int cellX = -34; cellX <= 22; cellX += 2)
		{
			bool walkable = false;
			for (int offsetY = 0; offsetY < 2 && !walkable; ++offsetY)
				for (int offsetX = 0; offsetX < 2 && !walkable; ++offsetX)
					walkable = IsTutorialFloor(
						static_cast<float>(cellX + offsetX), static_cast<float>(cellY + offsetY));
			if (walkable)
				m_TutorialMinimapTiles.push_back(
					{{static_cast<float>(cellX) + 0.5f, static_cast<float>(cellY) + 0.5f}, 1, 0.0f});
		}
	}

	m_FirstLevelMinimapTiles.clear();
	const int cellSize = 5;
	const int halfSize = m_FirstLevelSize / 2;
	const int cellCount = (m_FirstLevelSize + cellSize - 1) / cellSize;
	std::vector<int> occupiedCells(static_cast<size_t>(cellCount * cellCount), 0);
	std::vector<int> roomCells(static_cast<size_t>(cellCount * cellCount), 0);
	for (size_t index = 0; index < m_FirstLevelFloorVisuals.size(); ++index)
	{
		const MapVisualTile& tile = m_FirstLevelFloorVisuals[index];
		const int tileX = static_cast<int>(tile.position.x) + halfSize;
		const int tileY = static_cast<int>(tile.position.y) + halfSize;
		const int cellX = tileX / cellSize;
		const int cellY = tileY / cellSize;
		if (cellX < 0 || cellX >= cellCount || cellY < 0 || cellY >= cellCount)
			continue;
		const size_t cellIndex = static_cast<size_t>(cellY * cellCount + cellX);
		occupiedCells[cellIndex] = 1;
		if (tile.type == 2)
			roomCells[cellIndex] = 1;
	}
	for (int cellY = 0; cellY < cellCount; ++cellY)
	{
		for (int cellX = 0; cellX < cellCount; ++cellX)
		{
			const size_t cellIndex = static_cast<size_t>(cellY * cellCount + cellX);
			if (occupiedCells[cellIndex] == 0)
				continue;
			const float worldX = static_cast<float>(cellX * cellSize - halfSize) + 2.0f;
			const float worldY = static_cast<float>(cellY * cellSize - halfSize) + 2.0f;
			m_FirstLevelMinimapTiles.push_back(
				{{worldX, worldY}, roomCells[cellIndex] != 0 ? 2 : 1, 0.0f});
		}
	}
}

void Game::PublishMonitoringState(float deltaSeconds, bool force)
{
	if (m_MonitoringServer == NULL || !m_MonitoringServer->IsRunning())
		return;
	m_MonitoringPublishTimer += deltaSeconds;
	if (!force && m_MonitoringPublishTimer < 0.10f)
		return;
	m_MonitoringPublishTimer = 0.0f;
	++m_MonitoringSequence;
	const std::chrono::steady_clock::time_point buildStart = std::chrono::steady_clock::now();
	const std::string stateJson = BuildMonitoringJson();
	const std::chrono::steady_clock::time_point buildEnd = std::chrono::steady_clock::now();
	m_MonitoringJsonTimeMs =
		std::chrono::duration<double, std::milli>(buildEnd - buildStart).count();
	m_MonitoringPayloadBytes = static_cast<unsigned int>(stateJson.size());
	m_MonitoringServer->SetStateJson(stateJson);
}

std::wstring Game::InteractionText() const
{
	if (m_Mode == ShipControl)
	{
		const WorldPoint destination = FixedShipPosition(m_TravelDestinationShip);
		float distance = Length(
			m_ShipPosition.x - destination.x, m_ShipPosition.y - destination.y);
		if (distance >= 85.0f)
			return L"";
		if (m_Quest == FlyToSignal)
			return L"[E] 구조 신호에 진입";
		if (m_TravelDestinationShip == 0)
			return L"[E] 노크티스에 도킹";
		if (m_TravelDestinationShip == 2)
			return L"[E] 거대 우주 정거장에 도킹";
		return L"[E] 에레보스에 도킹";
	}
	if (m_Mode == FirstLevel && !m_LevelWeaponCollected && IsNear(FirstLevelWeaponPosition, 1.2f))
		return L"[E] 훈련용 펄스 소총 회수";
	if (m_Mode == FirstLevel)
	{
		if (m_Quest == BoardPersonalShipToNoctis &&
			IsNear(FirstLevelPersonalShipCockpitPosition, 1.6f))
			return L"[E] 개인 우주선으로 노크티스에 귀환";
		for (int engineIndex = 0; engineIndex < 4; ++engineIndex)
		{
			if (IsNear(m_LevelEnginePositions[engineIndex], 1.45f))
				return m_LevelEngineActive[engineIndex] ? L"보조 엔진 가동 중"
											 : L"[E] 정지된 보조 엔진 가동";
		}
		if (IsNear(m_LevelControlPosition, 1.5f))
		{
			if (!AreLevelEnginesActive())
				return L"[E] 감염된 시스템 제어 장치 - 엔진 복구 필요";
			if (!AreLevelHostilesCleared())
				return L"[E] 조종 장치 확인 - 적대 개체 제거 필요";
			return m_ErebusSystemsOnline ? L"에레보스 시스템 가동 중"
									 : L"[E] 에레보스 시스템 복구";
		}
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
			if (m_LevelSurvivors[index].health > 0 &&
				IsNear(m_LevelSurvivors[index].position, 1.3f))
				return m_LevelSurvivors[index].confused ? L"적대적인 무장 생존자"
												 : L"[E] 생존자와 대화";
		}
	}
	if (m_Mode != OnFoot)
		return L"";
	if (m_CurrentShip == 0 && IsNear(CockpitPosition, 1.25f))
	{
		if (!m_CoreRecovered)
			return L"[E] 노크티스 항법 제어 장치 - 항법 코어 필요";
		if (m_Quest == ActivateNoctisSystems)
			return L"[E] 노크티스 시스템 복구";
	}
	if ((m_Quest == BoardPersonalShipToErebus || m_Quest == BoardPersonalShipToStation) &&
		IsNear(TutorialPersonalShipCockpitPosition, 1.6f))
		return m_Quest == BoardPersonalShipToErebus ? L"[E] 개인 우주선으로 에레보스 출항"
												 : L"[E] 개인 우주선으로 정거장 출항";
	for (size_t index = 0; index < m_TutorialSurvivors.size(); ++index)
	{
		if (m_TutorialSurvivors[index].health > 0 &&
			IsNear(m_TutorialSurvivors[index].position, 1.3f))
			return L"[E] 생존자와 대화";
	}
	for (size_t i = 0; i < m_NeutralNpcs.size(); ++i)
	{
		const NeutralNpc& npc = m_NeutralNpcs[i];
		if (!npc.dead && IsNear(npc.position, 1.25f))
			return npc.dialogueId == 0 ? L"[E] 통신장교 서윤과 대화" : L"[E] 정비사 라울과 대화";
	}
	if (m_Quest == InspectTerminal && IsNear(TerminalPosition, 1.15f))
		return L"[E] 단말기 조사";
	if (m_Quest == CollectTutorialWeapon && IsNear(TutorialWeaponPosition, 1.2f))
		return L"[E] 펄스 카빈 획득";
	for (int recordIndex = 0; recordIndex < 3; ++recordIndex)
		if (m_CurrentShip == 0 && IsNear(TutorialRecordPositions[recordIndex], 1.05f))
			return m_TutorialRecordsRead[recordIndex] ? L"[E] 기록 다시 읽기" : L"[E] 함선 기록 읽기";
	if (m_CurrentShip == 0 && m_CoreRecovered && !m_SignalChecked && IsNear(SignalPosition, 1.05f))
		return L"[E] 구조 신호 조사";
	if (m_Quest == FindCore && AllEnemiesDefeated() && IsNear(CorePosition, 1.2f))
		return L"[E] 항법 코어 회수";
	if (m_Quest == ActivateNoctisSystems && IsNear(CockpitPosition, 1.25f))
		return L"[E] 항법 코어 설치 및 노크티스 시스템 복구";
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
	if (m_Mode == OnFoot || m_Mode == FirstLevel)
	{
		const ScreenPoint screen = WorldToScreen(x, y, height);
		const float horizontalLimit = static_cast<float>(m_Renderer->Width()) * 0.5f + 180.0f;
		const float verticalLimit = static_cast<float>(m_Renderer->Height()) * 0.5f + 180.0f;
		actor->SetVisible(
			std::fabs(screen.x) <= horizontalLimit && std::fabs(screen.y) <= verticalLimit);
	}
	return actor;
}

void Game::RenderInterior()
{
	m_SceneGraph.Clear();
	m_Renderer->BeginFrame(
		m_CurrentShip == 0 ? 0.012f : 0.018f, 0.020f, m_CurrentShip == 0 ? 0.035f : 0.045f, 1.0f);
	int width = m_Renderer->Width();
	int height = m_Renderer->Height();

	for (int x = -34; x <= 22; ++x)
	{
		for (int y = -34; y <= 22; ++y)
		{
			if (!IsTutorialFloor(static_cast<float>(x), static_cast<float>(y)))
				continue;
			ScreenPoint p = WorldToScreen(static_cast<float>(x), static_cast<float>(y));
			if (std::fabs(p.x) > width * 0.5f + TileWidth ||
				std::fabs(p.y) > height * 0.5f + TileHeight)
				continue;
			float checker = ((x + y) & 1) ? 0.015f : 0.0f;
			float restored = m_CoreRecovered ? 0.035f : 0.0f;
			const bool personalShipInterior =
				std::fabs(static_cast<float>(x) + 28.0f) <= 4.5f &&
				std::fabs(static_cast<float>(y) + 28.0f) <= 4.5f;
			const bool dockingPassage = !personalShipInterior &&
				((y >= -30 && y <= -26 && x >= -28 && x <= -20) ||
					(x >= -22 && x <= -18 && y >= -28 && y <= -13));
			float floorRed = 0.075f + checker;
			float floorGreen = 0.105f + restored;
			float floorBlue = 0.13f + restored;
			if (dockingPassage)
			{
				floorRed = 0.10f + checker;
				floorGreen = 0.145f;
				floorBlue = 0.16f;
			}
			if (personalShipInterior)
			{
				floorRed = 0.055f + checker;
				floorGreen = 0.17f;
				floorBlue = 0.205f;
			}
			m_Renderer->DrawDiamond(p.x,
				p.y,
				TileWidth - 2.0f,
				TileHeight - 1.0f,
				floorRed,
				floorGreen,
				floorBlue,
				1.0f);
		}
	}

	for (int x = -34; x <= 22; ++x)
	{
		for (int y = -34; y <= 22; ++y)
		{
			if (IsTutorialFloor(static_cast<float>(x), static_cast<float>(y)))
				continue;

			// Each solid tile can border the interior on more than one side. Draw one
			// module per exposed floor edge so corners wrap instead of choosing an
			// arbitrary single orientation for the whole tile.
			const int neighborOffsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
			for (int side = 0; side < 4; ++side)
			{
				const int offsetX = neighborOffsets[side][0];
				const int offsetY = neighborOffsets[side][1];
				if (!IsTutorialFloor(static_cast<float>(x + offsetX),
						static_cast<float>(y + offsetY)))
				{
					continue;
				}

				const float wallX = static_cast<float>(x) + static_cast<float>(offsetX) * 0.5f;
				const float wallY = static_cast<float>(y) + static_cast<float>(offsetY) * 0.5f;
				const float wallRotation = offsetX != 0 ? Pi * 0.5f : 0.0f;
				CreateSceneActor("HullWall", wallX, wallY, 0.0f, 1, wallX + wallY,
					[this, wallRotation](const ActorTransform& transform)
					{
						const ScreenPoint wall =
							WorldToScreen(transform.x, transform.y, transform.height);
						m_Renderer->DrawSoftShadow(
							wall.x + 8.0f, wall.y - 4.0f, 62.0f, 18.0f, 0.82f);
						m_ModelLibrary.DrawMesh(
							m_Renderer, "ship_wall", wall.x, wall.y, 19.0f, wallRotation);
					});
			}
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
	CreatePersonalShipInteriorActors(TutorialPersonalShipDockPosition);
	CreateSceneActor("Player",
		m_Player.x,
		m_Player.y,
		0.0f,
		1,
		m_Player.x + m_Player.y,
		[this](const ActorTransform& transform)
		{
			WorldPoint position = {transform.x, transform.y};
			DrawCharacter(position,
				0.20f,
				0.72f,
				0.86f,
				false,
				m_LastMoveDirection,
				m_PlayerMoving,
				m_PlayerHealth <= 0.0f);
		});
	for (size_t i = 0; i < m_Enemies.size(); ++i)
	{
		if (m_Enemies[i].active || m_Enemies[i].health <= 0)
		{
			const Enemy enemy = m_Enemies[i];
			const WorldPoint position = enemy.position;
			CreateSceneActor("Enemy",
				position.x,
				position.y,
				0.0f,
				1,
				position.x + position.y,
				[this, enemy](const ActorTransform& transform)
				{
					WorldPoint placedPosition = {transform.x, transform.y};
					DrawCharacter(placedPosition,
						0.62f,
						0.08f,
						0.72f,
						true,
						enemy.facingDirection,
						enemy.moving,
						enemy.health <= 0,
						enemy.attackCooldown >= 0.62f
							? Clamp((0.9f - enemy.attackCooldown) / 0.28f, 0.001f, 1.0f)
							: 0.0f);
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
	for (size_t i = 0; i < m_TutorialSurvivors.size(); ++i)
	{
		const LevelSurvivor survivor = m_TutorialSurvivors[i];
		CreateSceneActor("TutorialSurvivor",
			survivor.position.x,
			survivor.position.y,
			0.0f,
			1,
			survivor.position.x + survivor.position.y,
			[this, survivor, i](const ActorTransform& transform)
			{
				LevelSurvivor placedSurvivor = survivor;
				placedSurvivor.position = {transform.x, transform.y};
				DrawLevelSurvivor(placedSurvivor, i);
			});
	}
	for (size_t i = 0; i < m_Projectiles.size(); ++i)
	{
		const Projectile projectile = m_Projectiles[i];
		const WorldPoint position = projectile.position;
		CreateSceneActor("Projectile",
			position.x,
			position.y,
			29.0f,
			2,
			position.x + position.y,
			[this, projectile](const ActorTransform& transform)
			{
				ScreenPoint shot = WorldToScreen(transform.x, transform.y, transform.height);
				m_Renderer->DrawSoftShadow(shot.x, shot.y - 25.0f, 24.0f, 7.0f, 0.18f);
				DrawProjectileVisual(
					m_Renderer, projectile, shot.x, shot.y, ProjectileScreenAngle(projectile));
			});
	}
	m_SceneGraph.Render();
	m_Renderer->SetSceneMetrics(m_SceneGraph.VisibleActorCount(),
		m_SceneGraph.CulledActorCount(),
		m_SceneGraph.ActorPoolSize(),
		m_SceneGraph.TransformTimeMs(),
		m_SceneGraph.SortTimeMs(),
		m_SceneGraph.ActorRenderTimeMs());

	if (m_CurrentShip == 0)
	{
		ScreenPoint terminal = WorldToScreen(TerminalPosition.x, TerminalPosition.y, 22.0f);
		m_Renderer->DrawRect(terminal.x, terminal.y, 22.0f, 34.0f, 0.10f, 0.75f, 0.92f, 0.9f);
		if (m_Quest == CollectTutorialWeapon)
		{
			ScreenPoint weapon = WorldToScreen(TutorialWeaponPosition.x, TutorialWeaponPosition.y, 11.0f);
			m_Renderer->DrawSoftShadow(weapon.x, weapon.y - 10.0f, 60.0f, 16.0f, 0.65f);
			m_ModelLibrary.DrawMesh(m_Renderer, "pulse_rifle_mesh", weapon.x, weapon.y,
				38.0f * (1.0f + std::sin(m_TotalTime * 4.0f) * 0.08f));
		}
		for (int recordIndex = 0; recordIndex < 3; ++recordIndex)
		{
			ScreenPoint record = WorldToScreen(
				TutorialRecordPositions[recordIndex].x, TutorialRecordPositions[recordIndex].y, 5.0f);
			m_Renderer->DrawDiamond(record.x,
				record.y,
				24.0f,
				12.0f,
				m_TutorialRecordsRead[recordIndex] ? 0.18f : 0.75f,
				0.58f,
				0.18f,
				0.95f);
		}
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
	DrawFlightControlConsole(cockpit.x, cockpit.y, m_NoctisSystemsOnline, false);
	m_Renderer->DrawString(cockpit.x - 75.0f,
		cockpit.y + 92.0f,
		L"노크티스 항법 제어 장치",
		m_NoctisSystemsOnline ? 0.28f : 0.70f,
		m_NoctisSystemsOnline ? 0.88f : 0.36f,
		m_NoctisSystemsOnline ? 0.92f : 0.34f,
		0.95f);
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
	m_Renderer->DrawRect(width * 0.42f, -height * 0.27f, 12.0f, 42.0f, 0.12f, 0.60f, 0.75f, 0.55f);
}

void Game::SetDialogue(
	const std::wstring& dialogue, const WorldPoint& speakerPosition, float seconds)
{
	m_Dialogue = dialogue;
	m_DialogueSpeakerPosition = speakerPosition;
	m_DialogueTimer = seconds;
}

void Game::DrawFlightControlConsole(float x, float y, bool powered, bool infected)
{
	m_Renderer->DrawSoftShadow(x, y - 9.0f, 148.0f, 34.0f, 0.86f);
	m_ModelLibrary.DrawMesh(m_Renderer, "ship_pilot_seat", x, y, 24.0f);
	m_ModelLibrary.DrawMesh(m_Renderer, "ship_console", x, y + 10.0f, 24.0f);
	if (infected)
		m_Renderer->DrawDiamond(x, y + 26.0f, 112.0f, 45.0f, 0.46f, 0.03f, 0.62f, 0.22f);
	else if (powered)
		m_Renderer->DrawDiamond(x, y + 31.0f, 36.0f, 15.0f, 0.08f, 0.72f, 0.86f, 0.30f);
	return;
	const float pulse = 0.76f + std::sin(m_TotalTime * 5.0f) * 0.16f;
	const float signalRed = infected ? 0.72f : (powered ? 0.10f : 0.34f);
	const float signalGreen = infected ? 0.08f : (powered ? 0.72f : 0.22f);
	const float signalBlue = infected ? 0.88f : (powered ? 0.84f : 0.25f);

	// Raised pilot chair and armored pedestal.
	m_Renderer->DrawSoftShadow(x, y - 10.0f, 138.0f, 30.0f, 0.88f);
	m_Renderer->DrawDiamond(x, y - 1.0f, 134.0f, 47.0f, 0.045f, 0.085f, 0.11f, 1.0f);
	m_Renderer->DrawRect(x, y + 36.0f, 32.0f, 54.0f, 0.07f, 0.11f, 0.14f, 1.0f);
	m_Renderer->DrawRect(x, y + 59.0f, 46.0f, 8.0f, 0.14f, 0.21f, 0.24f, 1.0f);
	m_Renderer->DrawDiamond(x, y + 17.0f, 50.0f, 24.0f, 0.08f, 0.15f, 0.18f, 1.0f);

	// Wraparound side consoles with layered screens and key clusters.
	for (int side = -1; side <= 1; side += 2)
	{
		const float sideX = x + side * 48.0f;
		m_Renderer->DrawDiamond(sideX, y + 24.0f, 48.0f, 25.0f, 0.065f, 0.13f, 0.16f, 1.0f);
		m_Renderer->DrawDiamond(sideX,
			y + 29.0f,
			37.0f,
			16.0f,
			signalRed * 0.48f,
			signalGreen * 0.58f,
			signalBlue * 0.62f,
			powered || infected ? pulse : 0.55f);
		for (int button = 0; button < 3; ++button)
			m_Renderer->DrawRect(sideX - 10.0f + button * 10.0f,
				y + 13.0f,
				5.0f,
				4.0f,
				button == 1 ? 0.92f : signalRed,
				button == 1 ? 0.46f : signalGreen,
				signalBlue,
				0.94f);
		DrawRotatedRect(m_Renderer,
			sideX + side * 11.0f,
			y + 7.0f,
			5.0f,
			22.0f,
			side * 0.24f,
			0.24f,
			0.34f,
			0.37f,
			1.0f);
	}

	// Central navigation radar, rotating sweep, status bars, and targeting reticle.
	m_Renderer->DrawDiamond(x, y + 46.0f, 61.0f, 38.0f, 0.035f, 0.08f, 0.10f, 1.0f);
	m_Renderer->DrawDiamond(
		x, y + 46.0f, 49.0f, 30.0f, signalRed * 0.34f, signalGreen * 0.46f, signalBlue * 0.50f, 0.90f);
	m_Renderer->DrawDiamond(x, y + 46.0f, 28.0f, 17.0f, 0.025f, 0.09f, 0.11f, 0.96f);
	const float sweepAngle = powered ? m_TotalTime * 1.8f : -0.65f;
	DrawRotatedRect(m_Renderer,
		x + std::cos(sweepAngle) * 10.0f,
		y + 46.0f + std::sin(sweepAngle) * 6.0f,
		24.0f,
		2.5f,
		sweepAngle,
		signalRed,
		signalGreen,
		signalBlue,
		0.94f);
	m_Renderer->DrawDiamond(x, y + 46.0f, 7.0f, 7.0f, 0.86f, 0.94f, 0.94f, 1.0f);
	for (int gauge = -3; gauge <= 3; ++gauge)
		m_Renderer->DrawRect(x + gauge * 14.0f,
			y + 2.0f,
			8.0f,
			4.0f,
			gauge == 0 ? 0.94f : signalRed,
			gauge == 0 ? 0.38f : signalGreen,
			signalBlue,
			powered || infected ? 0.94f : 0.48f);

	if (infected)
	{
		// Organic-looking infection veins overlay the same console model.
		for (int vein = -2; vein <= 2; ++vein)
			DrawRotatedRect(m_Renderer,
				x + vein * 23.0f,
				y + 33.0f + std::sin(m_TotalTime * 3.0f + vein) * 3.0f,
				36.0f,
				4.0f,
				0.72f + vein * 0.18f,
				0.55f,
				0.04f,
				0.76f,
				pulse);
		m_Renderer->DrawDiamond(x, y + 36.0f, 118.0f, 48.0f, 0.46f, 0.03f, 0.62f, 0.20f);
	}
}

void Game::DrawWorldBlock(const Obstacle& obstacle)
{
	ScreenPoint foot = WorldToScreen(obstacle.x, obstacle.y);
	const char* resourceMesh = NULL;
	float resourceScale = 19.0f;
	if (obstacle.type == HullWall)
		resourceMesh = "ship_wall";
	else if (obstacle.type == ControlConsole)
	{
		resourceMesh = "ship_console";
		resourceScale = 24.0f;
	}
	else if (obstacle.type == MedicalPod)
	{
		resourceMesh = "ship_medbed";
		resourceScale = 23.0f;
	}
	else if (obstacle.type == PowerRelay)
	{
		resourceMesh = "ship_power";
		resourceScale = 21.0f;
	}
	else if (obstacle.type == EngineModule)
	{
		resourceMesh = "ship_engine";
		resourceScale = 23.0f;
	}
	else if (obstacle.type == CoolantPipe)
	{
		resourceMesh = "ship_coolant";
		resourceScale = 22.0f;
	}
	if (resourceMesh != NULL)
	{
		m_Renderer->DrawSoftShadow(foot.x + 8.0f, foot.y - 4.0f, 86.0f, 23.0f, 0.82f);
		m_ModelLibrary.DrawMesh(m_Renderer, resourceMesh, foot.x, foot.y, resourceScale);
		return;
	}
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

void Game::DrawCharacter(const WorldPoint& point,
	float r,
	float g,
	float b,
	bool enemy,
	const WorldPoint& facingDirection,
	bool moving,
	bool dead,
	float attackPhase)
{
	const ScreenPoint foot = WorldToScreen(point.x, point.y);
	if (dead)
	{
		WorldPoint deathFacing = facingDirection;
		if (!enemy)
		{
			const float mouseScreenX = static_cast<float>(m_MouseX - m_Renderer->Width() / 2);
			const float mouseScreenY = static_cast<float>(m_Renderer->Height() / 2 - m_MouseY + 30);
			deathFacing = {mouseScreenX / TileWidth - mouseScreenY / TileHeight,
				-mouseScreenX / TileWidth - mouseScreenY / TileHeight};
		}
		float deathLength = Length(deathFacing.x, deathFacing.y);
		if (deathLength < 0.001f)
		{
			deathFacing = {1.0f, 0.0f};
			deathLength = 1.0f;
		}
		deathFacing.x /= deathLength;
		deathFacing.y /= deathLength;
		const float deathScreenX = (deathFacing.x - deathFacing.y) * TileWidth * 0.5f;
		const float deathScreenY = -(deathFacing.x + deathFacing.y) * TileHeight * 0.5f;
		const float deathScreenAngle = std::atan2(deathScreenY, deathScreenX);
		const float corpseScreenRotation = enemy
			? (deathScreenX >= 0.0f ? -0.18f : 0.18f)
			: deathScreenAngle - Pi * 0.5f;
		const float corpseWorldRotation =
			std::atan2(deathFacing.y, deathFacing.x) + ModelForwardAxisCorrection;
		m_Renderer->DrawSoftShadow(
			foot.x, foot.y + 3.0f, enemy ? 58.0f : 48.0f, enemy ? 19.0f : 14.0f, 0.88f);
		m_ModelLibrary.DrawMesh(m_Renderer,
			enemy ? "void_quadruped" : "humanoid_soldier",
			foot.x,
			foot.y + (enemy ? 4.0f : 2.0f),
			enemy ? 16.0f : 17.0f,
			corpseWorldRotation,
			corpseScreenRotation,
			enemy ? 0.28f : 0.34f);
		if (enemy)
		{
			m_ModelLibrary.DrawMesh(m_Renderer,
				"void_claw_left",
				foot.x,
				foot.y + 4.0f,
				16.0f,
				corpseWorldRotation,
				corpseScreenRotation,
				0.28f,
				0.0f,
				0.22f,
				-0.82f);
			m_ModelLibrary.DrawMesh(m_Renderer,
				"void_claw_right",
				foot.x,
				foot.y + 4.0f,
				16.0f,
				corpseWorldRotation,
				corpseScreenRotation,
				0.28f,
				0.0f,
				-0.22f,
				-0.82f);
		}
		return;
	}
	const float directionLength = Length(facingDirection.x, facingDirection.y);
	float facingAngle = directionLength > 0.001f
		? std::atan2(facingDirection.y, facingDirection.x) + ModelForwardAxisCorrection
		: ModelForwardAxisCorrection;
	const bool hasWeapon = !enemy && (m_Mode == FirstLevel
		? m_LevelWeaponCollected
		: m_Quest != InspectTerminal && m_Quest != CollectTutorialWeapon);
	float aimScreenX = 1.0f;
	float aimScreenY = 0.0f;
	float aimAngle = 0.0f;
	if (!enemy)
	{
		// The player rotates independently from movement and always faces the cursor.
		aimScreenX = static_cast<float>(m_MouseX - m_Renderer->Width() / 2);
		aimScreenY = static_cast<float>(m_Renderer->Height() / 2 - m_MouseY + 30);
		float aimLength = Length(aimScreenX, aimScreenY);
		if (aimLength < 1.0f)
		{
			aimScreenX = 1.0f;
			aimScreenY = 0.0f;
			aimLength = 1.0f;
		}
		aimScreenX /= aimLength;
		aimScreenY /= aimLength;
		const WorldPoint mouseWorldDirection = {
			aimScreenX / TileWidth - aimScreenY / TileHeight,
			-aimScreenX / TileWidth - aimScreenY / TileHeight};
		aimAngle = std::atan2(mouseWorldDirection.y, mouseWorldDirection.x);
		facingAngle = aimAngle + ModelForwardAxisCorrection;
	}
	const float movementPhase =
		std::sin(m_TotalTime * (enemy ? 9.0f : 11.0f) + point.x * 0.37f + point.y * 0.21f);
	const float idlePhase = std::sin(m_TotalTime * 2.2f + point.x * 0.13f) * 0.25f;
	const float animationPhase = moving ? movementPhase : idlePhase;
	const float bob = moving ? std::fabs(animationPhase) * 2.8f : animationPhase * 0.8f;
	const float bodySway = animationPhase * (moving ? 0.055f : 0.015f);
	float facingScreenX = (facingDirection.x - facingDirection.y) * TileWidth * 0.5f;
	float facingScreenY = -(facingDirection.x + facingDirection.y) * TileHeight * 0.5f;
	float facingScreenLength = Length(facingScreenX, facingScreenY);
	if (facingScreenLength < 0.001f)
	{
		facingScreenX = 1.0f;
		facingScreenY = 0.0f;
		facingScreenLength = 1.0f;
	}
	facingScreenX /= facingScreenLength;
	facingScreenY /= facingScreenLength;
	const float attackWeight = enemy ? std::sin(Clamp(attackPhase, 0.0f, 1.0f) * Pi) : 0.0f;
	const float attackLunge = attackWeight * 8.0f;
	const float renderX = foot.x + facingScreenX * attackLunge;
	const float renderY = foot.y + bob + facingScreenY * attackLunge;

	m_Renderer->DrawSoftShadow(
		foot.x + 5.0f, foot.y, enemy ? 46.0f : 34.0f, 14.0f, 0.92f);
	m_ModelLibrary.DrawMesh(m_Renderer,
		enemy ? "void_quadruped" : "humanoid_soldier",
		renderX,
		renderY,
		enemy ? 17.0f : 18.0f,
		facingAngle + bodySway);

	if (enemy)
	{
		// These are the actual Blender-authored blade.L/blade.R objects, exported
		// separately so they can rotate around their armature socket at runtime.
		const float clawSwing = attackWeight * 0.78f;
		m_ModelLibrary.DrawMesh(m_Renderer,
			"void_claw_left",
			renderX,
			renderY,
			17.0f,
			facingAngle + bodySway,
			0.0f,
			1.0f,
			-clawSwing,
			0.22f,
			-0.82f);
		m_ModelLibrary.DrawMesh(m_Renderer,
			"void_claw_right",
			renderX,
			renderY,
			17.0f,
			facingAngle + bodySway,
			0.0f,
			1.0f,
			clawSwing,
			-0.22f,
			-0.82f);
		return;
	}

	if (!hasWeapon)
		return;

	// Build the aiming pose from actual shoulder-to-hand segments so the rifle stays
	// seated between both hands instead of floating above the character.
	const float aimBreathing = std::sin(m_TotalTime * 2.2f) * 0.8f;
	const float torsoX = foot.x;
	const float torsoY = foot.y + 21.0f + bob + aimBreathing;
	const float perpendicularX = -aimScreenY;
	const float perpendicularY = aimScreenX;
	const float recoil = m_AttackFlash > 0.0f ? -2.5f : 0.0f;
	const ScreenPoint rearShoulder = {
		torsoX + perpendicularX * 4.0f, torsoY + perpendicularY * 2.5f};
	const ScreenPoint frontShoulder = {
		torsoX - perpendicularX * 4.0f, torsoY - perpendicularY * 2.5f};
	const ScreenPoint triggerHand = {
		torsoX + aimScreenX * (7.0f + recoil), torsoY + aimScreenY * (7.0f + recoil)};
	const ScreenPoint supportHand = {
		torsoX + aimScreenX * (14.0f + recoil), torsoY + aimScreenY * (14.0f + recoil)};
	auto drawArmSegment = [&](const ScreenPoint& shoulder,
		const ScreenPoint& hand,
		float red,
		float green,
		float blue) {
		const float dx = hand.x - shoulder.x;
		const float dy = hand.y - shoulder.y;
		DrawRotatedRect(m_Renderer,
			(shoulder.x + hand.x) * 0.5f,
			(shoulder.y + hand.y) * 0.5f,
			Length(dx, dy),
			4.5f,
			std::atan2(dy, dx),
			red,
			green,
			blue,
			1.0f);
	};
	drawArmSegment(rearShoulder, triggerHand, 0.15f, 0.18f, 0.20f);
	drawArmSegment(frontShoulder, supportHand, 0.22f, 0.10f, 0.05f);
	const float weaponX = torsoX + aimScreenX * (13.0f + recoil);
	const float weaponY = torsoY + aimScreenY * (13.0f + recoil);
	m_ModelLibrary.DrawMesh(
		m_Renderer,
		"pulse_rifle_mesh",
		weaponX,
		weaponY,
		22.0f,
		aimAngle + ModelForwardAxisCorrection);
	// Hands are drawn after the weapon so the grip reads clearly in the final silhouette.
	m_Renderer->DrawEllipse(
		triggerHand.x, triggerHand.y, 5.5f, 5.5f, 0.34f, 0.24f, 0.20f, 1.0f);
	m_Renderer->DrawEllipse(
		supportHand.x, supportHand.y, 5.5f, 5.5f, 0.34f, 0.24f, 0.20f, 1.0f);

	if (m_AttackFlash > 0.0f)
	{
		const float muzzleX = torsoX + aimScreenX * 27.0f;
		const float muzzleY = torsoY + aimScreenY * 27.0f;
		m_Renderer->DrawDiamond(
			muzzleX, muzzleY, 30.0f, 10.0f, 0.20f, 0.88f, 1.0f, 0.88f);
	}
}

void Game::DrawNeutralNpc(const NeutralNpc& npc)
{
	const ScreenPoint foot = WorldToScreen(npc.position.x, npc.position.y);
	const float idlePhase = std::sin(m_TotalTime * 2.4f + npc.animationOffset) * 0.35f;
	const float verticalOffset = npc.dead ? 0.0f : idlePhase;
	const float bodySway = npc.dead ? 0.0f : idlePhase * 0.04f;
	const float npcFacingScreenAngle = std::atan2(-TileHeight * 0.5f, TileWidth * 0.5f);
	m_Renderer->DrawSoftShadow(foot.x, foot.y, npc.dead ? 54.0f : 34.0f, 13.0f, 0.88f);
	m_ModelLibrary.DrawMesh(m_Renderer,
		"humanoid_soldier",
		foot.x,
		foot.y + verticalOffset,
		18.0f,
		ModelForwardAxisCorrection + bodySway,
		npc.dead ? npcFacingScreenAngle - Pi * 0.5f : 0.0f,
		npc.dead ? 0.34f : 1.0f);
	return;
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

	const WorldPoint boundaryWorld[] = {{SpaceBoundaryCenter.x, SpaceBoundaryCenter.y - SpaceBoundaryHalfHeight},
		{SpaceBoundaryCenter.x + SpaceBoundaryHalfWidth, SpaceBoundaryCenter.y},
		{SpaceBoundaryCenter.x, SpaceBoundaryCenter.y + SpaceBoundaryHalfHeight},
		{SpaceBoundaryCenter.x - SpaceBoundaryHalfWidth, SpaceBoundaryCenter.y}};
	ScreenPoint boundaryScreen[4];
	for (int vertexIndex = 0; vertexIndex < 4; ++vertexIndex)
	{
		boundaryScreen[vertexIndex] = {
			(boundaryWorld[vertexIndex].x - m_ShipPosition.x) * 0.72f,
			(boundaryWorld[vertexIndex].y - m_ShipPosition.y) * 0.72f};
	}
	for (int edgeIndex = 0; edgeIndex < 4; ++edgeIndex)
	{
		const ScreenPoint start = boundaryScreen[edgeIndex];
		const ScreenPoint end = boundaryScreen[(edgeIndex + 1) % 4];
		const float edgeX = end.x - start.x;
		const float edgeY = end.y - start.y;
		DrawRotatedRect(m_Renderer,
			(start.x + end.x) * 0.5f,
			(start.y + end.y) * 0.5f,
			Length(edgeX, edgeY),
			4.0f,
			std::atan2(edgeY, edgeX),
			0.18f,
			0.52f,
			0.68f,
			0.50f);
	}

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

	for (int fixedShipIndex = 0; fixedShipIndex < 6; ++fixedShipIndex)
	{
		const WorldPoint fixedPosition = FixedShipPosition(fixedShipIndex);
		const float fixedX = (fixedPosition.x - m_ShipPosition.x) * 0.72f;
		const float fixedY = (fixedPosition.y - m_ShipPosition.y) * 0.72f;
		const bool destination = fixedShipIndex == m_TravelDestinationShip;
		if (destination)
		{
			const float beaconPulse = 0.55f + std::sin(m_TotalTime * 5.0f) * 0.25f;
			m_Renderer->DrawDiamond(
				fixedX, fixedY, 125.0f, 58.0f, 0.08f, 0.34f, 0.46f, beaconPulse);
		}
		if (fixedShipIndex == 2)
		{
			m_Renderer->DrawDiamond(fixedX, fixedY + 12.0f, 270.0f, 126.0f,
				0.08f, 0.15f, 0.22f, 1.0f);
			m_Renderer->DrawDiamond(fixedX, fixedY + 12.0f, 180.0f, 86.0f,
				0.12f, 0.48f, 0.58f, 0.92f);
			m_Renderer->DrawRect(fixedX, fixedY + 55.0f, 34.0f, 150.0f,
				0.10f, 0.24f, 0.31f, 1.0f);
		}
		else
		{
			const float scale = fixedShipIndex == 1 ? 2.3f : (fixedShipIndex >= 3 ? 1.65f : 1.8f);
			const float red = fixedShipIndex == 1 ? 0.32f : (fixedShipIndex == 4 ? 0.34f : 0.20f);
			const float green = fixedShipIndex == 3 ? 0.62f : (fixedShipIndex == 5 ? 0.38f : 0.48f);
			DrawShip(fixedX, fixedY + 14.0f, -Pi * 0.5f, scale, red, green, 0.58f);
		}
		m_Renderer->DrawString(
			fixedX - 54.0f, fixedY + 104.0f, FixedShipName(fixedShipIndex), 0.48f, 0.90f, 0.94f, 1.0f);
	}
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
	const WorldPoint destinationPosition = FixedShipPosition(m_TravelDestinationShip);
	float destinationDirectionX = destinationPosition.x - m_ShipPosition.x;
	float destinationDirectionY = destinationPosition.y - m_ShipPosition.y;
	const float destinationDirectionLength = Length(destinationDirectionX, destinationDirectionY);
	if (destinationDirectionLength > 0.001f)
	{
		destinationDirectionX /= destinationDirectionLength;
		destinationDirectionY /= destinationDirectionLength;
		const float arrowRadius = 112.0f;
		const float arrowX = shipX + destinationDirectionX * arrowRadius;
		const float arrowY = shipY + destinationDirectionY * arrowRadius;
		const float perpendicularX = -destinationDirectionY;
		const float perpendicularY = destinationDirectionX;
		const ScreenPoint tip = {arrowX + destinationDirectionX * 14.0f,
			arrowY + destinationDirectionY * 14.0f};
		const ScreenPoint left = {arrowX - destinationDirectionX * 10.0f + perpendicularX * 8.0f,
			arrowY - destinationDirectionY * 10.0f + perpendicularY * 8.0f};
		const ScreenPoint right = {arrowX - destinationDirectionX * 10.0f - perpendicularX * 8.0f,
			arrowY - destinationDirectionY * 10.0f - perpendicularY * 8.0f};
		m_Renderer->DrawQuad(tip, left, right, right, 1.0f, 0.08f, 0.05f, 0.96f);
	}
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
	m_Renderer->SetSceneMetrics(m_SceneGraph.VisibleActorCount(),
		m_SceneGraph.CulledActorCount(),
		m_SceneGraph.ActorPoolSize(),
		m_SceneGraph.TransformTimeMs(),
		m_SceneGraph.SortTimeMs(),
		m_SceneGraph.ActorRenderTimeMs());

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

void Game::DrawLevelSurvivor(const LevelSurvivor& survivor, size_t survivorIndex)
{
	const ScreenPoint screen = WorldToScreen(survivor.position.x, survivor.position.y);
	if (survivor.health <= 0)
	{
		WorldPoint deathDirection = survivor.aimDirection;
		float deathDirectionLength = Length(deathDirection.x, deathDirection.y);
		if (deathDirectionLength < 0.001f)
		{
			deathDirection = {1.0f, 0.0f};
			deathDirectionLength = 1.0f;
		}
		deathDirection.x /= deathDirectionLength;
		deathDirection.y /= deathDirectionLength;
		const float deathScreenX =
			(deathDirection.x - deathDirection.y) * TileWidth * 0.5f;
		const float deathScreenY =
			-(deathDirection.x + deathDirection.y) * TileHeight * 0.5f;
		const float deathScreenAngle = std::atan2(deathScreenY, deathScreenX);
		const float deathWorldAngle =
			std::atan2(deathDirection.y, deathDirection.x) + ModelForwardAxisCorrection;
		m_Renderer->DrawSoftShadow(screen.x, screen.y - 3.0f, 54.0f, 12.0f, 0.84f);
		m_ModelLibrary.DrawMesh(
			m_Renderer,
			"humanoid_soldier",
			screen.x,
			screen.y,
			17.0f,
			deathWorldAngle,
			deathScreenAngle - Pi * 0.5f,
			0.34f);
		return;
	}

	const float trembleStrength = survivor.pattern == SurvivorTrembling || survivor.confused
		? 1.4f + survivor.fear * 1.8f
		: 0.25f;
	const float tremble =
		std::sin(m_TotalTime * (18.0f + survivor.fear * 12.0f) + survivorIndex) * trembleStrength;
	const float breathing = std::sin(m_TotalTime * 2.1f + survivorIndex) * 0.45f;
	const float survivorDirectionLength =
		Length(survivor.aimDirection.x, survivor.aimDirection.y);
	const float survivorFacingAngle = survivorDirectionLength > 0.001f
		? std::atan2(survivor.aimDirection.y, survivor.aimDirection.x) +
			ModelForwardAxisCorrection
		: ModelForwardAxisCorrection;
	m_Renderer->DrawSoftShadow(screen.x, screen.y, 34.0f, 12.0f, 0.82f);
	m_ModelLibrary.DrawMesh(m_Renderer,
		"humanoid_soldier",
		screen.x + tremble,
		screen.y + breathing,
		18.0f,
		survivorFacingAngle + tremble * 0.012f);

	if (survivor.armed)
	{
		const float warning = survivor.confused ? 0.92f : 0.35f;
		float aimScreenX = (survivor.aimDirection.x - survivor.aimDirection.y) * TileWidth * 0.5f;
		float aimScreenY =
			-(survivor.aimDirection.x + survivor.aimDirection.y) * TileHeight * 0.5f;
		const float aimScreenLength = Length(aimScreenX, aimScreenY);
		if (aimScreenLength > 0.001f)
		{
			aimScreenX /= aimScreenLength;
			aimScreenY /= aimScreenLength;
		}
		const float weaponAngle = survivorDirectionLength > 0.001f
			? std::atan2(survivor.aimDirection.y, survivor.aimDirection.x)
			: 0.0f;
		m_ModelLibrary.DrawMesh(m_Renderer,
			"pulse_rifle_mesh",
			screen.x + tremble + aimScreenX * 13.0f,
			screen.y + 21.0f + breathing + aimScreenY * 13.0f,
			22.0f,
			weaponAngle + ModelForwardAxisCorrection);
		m_Renderer->DrawDiamond(screen.x + tremble + aimScreenX * 29.0f,
			screen.y + 32.0f + breathing + aimScreenY * 16.0f,
			7.0f,
			4.0f,
			warning,
			survivor.confused ? 0.12f : 0.52f,
			0.10f,
			0.85f);
	}

	std::wstring label = SurvivorRoleName(survivor.pattern);
	if (survivor.confused)
		label += L" [피아식별 불가]";
	if (survivor.squadId >= 0)
	{
		std::wostringstream squadLabel;
		squadLabel << L" [분대 " << survivor.squadId + 1 << L"]";
		label += squadLabel.str();
	}
	m_Renderer->DrawString(screen.x - 34.0f,
		screen.y + 61.0f,
		label,
		survivor.confused ? 1.0f : 0.62f,
		survivor.confused ? 0.32f : 0.82f,
		survivor.confused ? 0.24f : 0.76f,
		0.92f);
}

void Game::CreatePersonalShipInteriorActors(const WorldPoint& dockPosition)
{
	const Obstacle fixtures[] = {
		{dockPosition.x,
			dockPosition.y,
			0.70f,
			0.45f,
			38.0f,
			0.08f,
			0.36f,
			0.46f,
			ControlConsole},
		{dockPosition.x - 2.6f,
			dockPosition.y + 1.8f,
			0.55f,
			0.65f,
			42.0f,
			0.10f,
			0.29f,
			0.38f,
			PowerRelay},
		{dockPosition.x + 2.6f,
			dockPosition.y + 1.8f,
			0.55f,
			0.65f,
			42.0f,
			0.10f,
			0.29f,
			0.38f,
			PowerRelay},
		{dockPosition.x - 2.4f,
			dockPosition.y - 2.2f,
			0.62f,
			0.52f,
			47.0f,
			0.12f,
			0.34f,
			0.42f,
			EngineModule},
		{dockPosition.x + 2.4f,
			dockPosition.y - 2.2f,
			0.62f,
			0.52f,
			47.0f,
			0.12f,
			0.34f,
			0.42f,
			EngineModule},
	};

	const size_t fixtureCount = sizeof(fixtures) / sizeof(fixtures[0]);
	for (size_t fixtureIndex = 0; fixtureIndex < fixtureCount; ++fixtureIndex)
	{
		const Obstacle fixture = fixtures[fixtureIndex];
		CreateSceneActor("PersonalShipFixture",
			fixture.x,
			fixture.y,
			0.0f,
			2,
			fixture.x + fixture.y,
			[this, fixture](const ActorTransform& transform)
			{
				Obstacle placedFixture = fixture;
				placedFixture.x = transform.x;
				placedFixture.y = transform.y;
				DrawWorldBlock(placedFixture);
			});
	}

	CreateSceneActor("PersonalShipCockpit",
		dockPosition.x,
		dockPosition.y + 3.0f,
		0.0f,
		3,
		dockPosition.x + dockPosition.y + 3.0f,
		[this](const ActorTransform& transform)
		{
			const ScreenPoint cockpit = WorldToScreen(transform.x, transform.y, 17.0f);
			DrawFlightControlConsole(cockpit.x, cockpit.y, true, false);
			m_Renderer->DrawString(cockpit.x - 61.0f,
				cockpit.y + 78.0f,
				L"개인 우주선 조종석",
				0.30f,
				0.86f,
				0.96f,
				0.95f);
		});
}

void Game::DrawAsteroid(const Asteroid& asteroid)
{
	float x = (asteroid.position.x - m_ShipPosition.x) * 0.72f;
	float y = (asteroid.position.y - m_ShipPosition.y) * 0.72f;
	const int variant = static_cast<int>(std::fabs(asteroid.radius)) % 4;
	const char* meshes[] = {"asteroid_a", "asteroid_b", "asteroid_c", "asteroid_d"};
	m_Renderer->DrawSoftShadow(x + 12.0f, y - 12.0f, asteroid.radius * 2.5f, asteroid.radius, 0.92f);
	m_ModelLibrary.DrawMesh(
		m_Renderer, meshes[variant], x, y, asteroid.radius * 0.85f, asteroid.rotation);
	return;
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
	int screenWidth = m_Renderer->Width();
	int screenHeight = m_Renderer->Height();

	for (size_t index = 0; index < m_FirstLevelFloorVisuals.size(); ++index)
	{
		const MapVisualTile& tile = m_FirstLevelFloorVisuals[index];
		ScreenPoint screen = WorldToScreen(tile.position.x, tile.position.y);
		if (std::fabs(screen.x) > screenWidth * 0.5f + 110.0f ||
			std::fabs(screen.y) > screenHeight * 0.5f + 110.0f)
			continue;

		float roomLight = tile.type == 2 ? 0.018f : 0.0f;
		const bool personalShipInterior =
			std::fabs(tile.position.x + 57.0f) <= 4.5f &&
			std::fabs(tile.position.y + 57.0f) <= 4.5f;
		const bool dockingPassage = !personalShipInterior && tile.type == 1 &&
			tile.position.x < -31.0f && tile.position.y < -43.0f;
		float floorRed = 0.050f + tile.variation + roomLight;
		float floorGreen = 0.078f + tile.variation + roomLight;
		float floorBlue = 0.098f + tile.variation + roomLight;
		if (dockingPassage)
		{
			floorRed = 0.105f + tile.variation;
			floorGreen = 0.135f + tile.variation;
			floorBlue = 0.145f + tile.variation;
		}
		if (personalShipInterior)
		{
			floorRed = 0.050f + tile.variation;
			floorGreen = 0.175f + tile.variation;
			floorBlue = 0.215f + tile.variation;
		}
		m_Renderer->DrawDiamond(screen.x,
			screen.y,
			TileWidth - 2.0f,
			TileHeight - 1.0f,
			floorRed,
			floorGreen,
			floorBlue,
			1.0f);
		if (tile.type == 1 && (static_cast<int>(tile.position.x + tile.position.y) & 3) == 0)
			m_Renderer->DrawDiamond(
				screen.x, screen.y + 1.0f, 21.0f, 7.0f, 0.10f, 0.42f, 0.46f, 0.42f);
	}

	for (size_t index = 0; index < m_FirstLevelWallVisuals.size(); ++index)
	{
		const WorldPoint position = m_FirstLevelWallVisuals[index];
		const int neighborOffsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
		for (int side = 0; side < 4; ++side)
		{
			const int offsetX = neighborOffsets[side][0];
			const int offsetY = neighborOffsets[side][1];
			if (!IsFirstLevelWalkable(position.x + static_cast<float>(offsetX),
					position.y + static_cast<float>(offsetY)))
			{
				continue;
			}

			const float wallX = position.x + static_cast<float>(offsetX) * 0.5f;
			const float wallY = position.y + static_cast<float>(offsetY) * 0.5f;
			const ScreenPoint wallScreen = WorldToScreen(wallX, wallY);
			if (std::fabs(wallScreen.x) > screenWidth * 0.5f + 120.0f ||
				std::fabs(wallScreen.y) > screenHeight * 0.5f + 120.0f)
			{
				continue;
			}

			const float wallRotation = offsetX != 0 ? Pi * 0.5f : 0.0f;
			CreateSceneActor("CorridorWall",
				wallX,
				wallY,
				0.0f,
				1,
				wallX + wallY,
				[this, wallRotation](const ActorTransform& transform)
				{
					const ScreenPoint wall =
						WorldToScreen(transform.x, transform.y, transform.height);
					m_Renderer->DrawSoftShadow(wall.x, wall.y, 52.0f, 16.0f, 0.76f);
					m_ModelLibrary.DrawMesh(
						m_Renderer, "ship_wall", wall.x, wall.y, 20.0f, wallRotation);
				});
		}
	}
	CreatePersonalShipInteriorActors(FirstLevelPersonalShipDockPosition);

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
				m_ModelLibrary.DrawMesh(m_Renderer, "pulse_rifle_mesh", weapon.x, weapon.y, 38.0f * pulse);
			});
	}

	for (int engineIndex = 0; engineIndex < 4; ++engineIndex)
	{
		const WorldPoint enginePosition = m_LevelEnginePositions[engineIndex];
		const bool active = m_LevelEngineActive[engineIndex];
		CreateSceneActor("LevelAuxiliaryEngine",
			enginePosition.x,
			enginePosition.y,
			0.0f,
			2,
			enginePosition.x + enginePosition.y,
			[this, active, engineIndex](const ActorTransform& transform)
			{
				const ScreenPoint engine = WorldToScreen(transform.x, transform.y, 8.0f);
				const float rotation = active ? m_TotalTime * (2.4f + engineIndex * 0.12f) : 0.0f;
				const float glow = active ? 0.72f + std::sin(m_TotalTime * 6.0f + engineIndex) * 0.18f : 0.20f;
				const float casingRed = active ? 0.08f : 0.17f;
				const float casingGreen = active ? 0.42f : 0.18f;
				const float casingBlue = active ? 0.52f : 0.22f;
				m_Renderer->DrawSoftShadow(engine.x + 12.0f, engine.y - 8.0f, 116.0f, 30.0f, 0.84f);
				m_ModelLibrary.DrawMesh(m_Renderer, "ship_engine", engine.x, engine.y, 27.0f);
				m_Renderer->DrawDiamond(engine.x + 32.0f,
					engine.y + 3.0f,
					88.0f,
					46.0f,
					casingRed * 0.72f,
					casingGreen * 0.72f,
					casingBlue * 0.72f,
					1.0f);
				for (int housingBand = 0; housingBand < 3; ++housingBand)
				{
					const float bandX = engine.x + 18.0f + housingBand * 20.0f;
					DrawRotatedRect(m_Renderer,
						bandX,
						engine.y + 4.0f,
						6.0f,
						45.0f - housingBand * 5.0f,
						0.0f,
						active ? 0.20f : 0.30f,
						active ? 0.68f : 0.28f,
						active ? 0.72f : 0.27f,
						0.92f);
				}
				m_Renderer->DrawDiamond(engine.x - 8.0f,
					engine.y + 8.0f,
					108.0f,
					88.0f,
					active ? 0.18f : 0.30f,
					active ? 0.72f : 0.30f,
					active ? 0.76f : 0.29f,
					1.0f);
				m_Renderer->DrawDiamond(engine.x - 8.0f,
					engine.y + 8.0f,
					91.0f,
					72.0f,
					0.025f,
					active ? 0.16f : 0.07f,
					active ? 0.19f : 0.08f,
					1.0f);
				for (int bladeIndex = 0; bladeIndex < 12; ++bladeIndex)
				{
					const float bladeAngle = rotation + static_cast<float>(bladeIndex) * Pi / 6.0f;
					const float bladeX = engine.x - 8.0f + std::cos(bladeAngle) * 23.0f;
					const float bladeY = engine.y + 8.0f + std::sin(bladeAngle) * 17.0f;
					DrawRotatedRect(m_Renderer,
						bladeX,
						bladeY,
						34.0f,
						5.5f,
						bladeAngle,
						active ? 0.28f : 0.34f,
						active ? 0.82f : 0.34f,
						active ? 0.86f : 0.32f,
						0.94f);
				}
				m_Renderer->DrawDiamond(
					engine.x - 8.0f, engine.y + 8.0f, 29.0f, 25.0f, 0.12f, glow, glow, 1.0f);
				m_Renderer->DrawDiamond(engine.x - 8.0f,
					engine.y + 12.0f,
					12.0f,
					14.0f,
					active ? 0.62f : 0.42f,
					active ? 0.98f : 0.40f,
					active ? 1.0f : 0.36f,
					1.0f);
				m_Renderer->DrawString(engine.x - 61.0f,
					engine.y + 62.0f,
					active ? L"보조 엔진 가동" : L"정지된 보조 엔진 [E]",
					active ? 0.30f : 0.92f,
					active ? 0.90f : 0.42f,
					active ? 0.92f : 0.30f,
					0.95f);
			});
	}

	const bool enginesActive = AreLevelEnginesActive();
	const bool departureReady = enginesActive && AreLevelHostilesCleared();
	CreateSceneActor("InfectedFlightControl",
		m_LevelControlPosition.x,
		m_LevelControlPosition.y,
		0.0f,
		2,
		m_LevelControlPosition.x + m_LevelControlPosition.y,
		[this, enginesActive, departureReady](const ActorTransform& transform)
		{
			const ScreenPoint control = WorldToScreen(transform.x, transform.y, 14.0f);
			DrawFlightControlConsole(control.x, control.y, enginesActive, !enginesActive);
			m_Renderer->DrawString(control.x - 75.0f,
				control.y + 92.0f,
				!enginesActive ? L"감염된 시스템 제어 장치"
							   : (departureReady ? L"에레보스 시스템 제어 장치 [E]"
											 : L"시스템 잠금: 적대 개체 제거 필요"),
				enginesActive ? 0.34f : 0.90f,
				enginesActive ? 0.90f : 0.24f,
				enginesActive ? 0.92f : 0.96f,
				0.96f);
		});

	for (size_t index = 0; index < m_LevelCorpses.size(); ++index)
	{
		const WorldPoint position = m_LevelCorpses[index];
		const ScreenPoint corpseScreen = WorldToScreen(position.x, position.y);
		if (std::fabs(corpseScreen.x) > screenWidth * 0.5f + 100.0f ||
			std::fabs(corpseScreen.y) > screenHeight * 0.5f + 100.0f)
		{
			continue;
		}
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
				m_ModelLibrary.DrawMesh(m_Renderer, "humanoid_soldier", corpse.x, corpse.y, 18.0f, 1.57f);
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
			8,
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
				m_Renderer->DrawDiamond(
					screen.x, screen.y + 3.0f, 45.0f * pulse, 20.0f * pulse, 0.08f, 0.68f, 0.82f, 0.24f);
				m_Renderer->DrawRect(
					screen.x, screen.y + 22.0f, 4.0f, 34.0f, 0.10f, 0.72f, 0.88f, 0.28f);
				m_ModelLibrary.Draw(m_Renderer, "ammo_magazine", screen.x, screen.y, pulse);
			});
	}

	for (size_t index = 0; index < m_LevelSurvivors.size(); ++index)
	{
		const LevelSurvivor& survivor = m_LevelSurvivors[index];
		const ScreenPoint survivorScreen = WorldToScreen(survivor.position.x, survivor.position.y);
		if (std::fabs(survivorScreen.x) > screenWidth * 0.5f + 100.0f ||
			std::fabs(survivorScreen.y) > screenHeight * 0.5f + 100.0f)
		{
			continue;
		}
		CreateSceneActor("LevelSurvivor",
			survivor.position.x,
			survivor.position.y,
			0.0f,
			3,
			survivor.position.x + survivor.position.y,
			[this, survivor, index](const ActorTransform& transform)
			{
				LevelSurvivor renderedSurvivor = survivor;
				renderedSurvivor.position = {transform.x, transform.y};
				DrawLevelSurvivor(renderedSurvivor, index);
			});
	}

	for (size_t index = 0; index < m_Enemies.size(); ++index)
	{
		if (!m_Enemies[index].active && m_Enemies[index].health > 0)
			continue;

		const Enemy enemyState = m_Enemies[index];
		const WorldPoint position = enemyState.position;
		const ScreenPoint enemyScreen = WorldToScreen(position.x, position.y);
		if (std::fabs(enemyScreen.x) > screenWidth * 0.5f + 100.0f ||
			std::fabs(enemyScreen.y) > screenHeight * 0.5f + 100.0f)
		{
			continue;
		}
		CreateSceneActor("VoidScout",
			position.x,
			position.y,
			0.0f,
			3,
			position.x + position.y,
			[this, enemyState](const ActorTransform& transform)
			{
				WorldPoint position = {transform.x, transform.y};
				DrawCharacter(position,
					0.62f,
					0.08f,
					0.72f,
					true,
					enemyState.facingDirection,
					enemyState.moving,
					enemyState.health <= 0,
					enemyState.attackCooldown >= 0.62f
						? Clamp((0.9f - enemyState.attackCooldown) / 0.28f, 0.001f, 1.0f)
						: 0.0f);
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
			DrawCharacter(position,
				0.20f,
				0.72f,
				0.86f,
				false,
				m_LastMoveDirection,
				m_PlayerMoving,
				m_PlayerHealth <= 0.0f);
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
				const bool survivorShot = projectile.ownerType == ProjectileOwnerSurvivor;
				const float projectileAngle = ProjectileScreenAngle(projectile);
				DrawRotatedRect(m_Renderer,
					(bullet.x + previous.x) * 0.5f,
					(bullet.y + previous.y) * 0.5f,
					tracerWidth,
					5.0f,
					projectileAngle,
					projectile.charged || survivorShot ? 1.0f : 0.10f,
					projectile.charged ? 0.34f : (survivorShot ? 0.12f : 0.62f),
					projectile.charged ? 0.08f : (survivorShot ? 0.10f : 0.94f),
					projectile.charged ? 0.70f : 0.42f);
				DrawProjectileVisual(m_Renderer, projectile, bullet.x, bullet.y, projectileAngle);
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
	m_Renderer->SetSceneMetrics(m_SceneGraph.VisibleActorCount(),
		m_SceneGraph.CulledActorCount(),
		m_SceneGraph.ActorPoolSize(),
		m_SceneGraph.TransformTimeMs(),
		m_SceneGraph.SortTimeMs(),
		m_SceneGraph.ActorRenderTimeMs());
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
	m_Renderer->SetSceneMetrics(m_SceneGraph.VisibleActorCount(),
		m_SceneGraph.CulledActorCount(),
		m_SceneGraph.ActorPoolSize(),
		m_SceneGraph.TransformTimeMs(),
		m_SceneGraph.SortTimeMs(),
		m_SceneGraph.ActorRenderTimeMs());
}

void Game::RenderInterface()
{
	int width = m_Renderer->Width();
	int height = m_Renderer->Height();
	const std::vector<std::wstring> objectives = ObjectiveLines();
	const float missionWidth = std::min(560.0f, static_cast<float>(width) - 28.0f);
	const float missionHeight = 42.0f + static_cast<float>(objectives.size()) * 25.0f;
	const float missionCenterX = -width * 0.5f + missionWidth * 0.5f + 14.0f;
	const float missionCenterY = height * 0.5f - missionHeight * 0.5f - 14.0f;
	m_Renderer->DrawRect(missionCenterX,
		missionCenterY,
		missionWidth,
		missionHeight,
		0.015f,
		0.025f,
		0.045f,
		0.92f);
	m_Renderer->DrawString(-width * 0.5f + 28.0f,
		height * 0.5f - 39.0f,
		L"임무",
		0.32f,
		0.82f,
		0.94f,
		1.0f);
	for (size_t objectiveIndex = 0; objectiveIndex < objectives.size(); ++objectiveIndex)
	{
		const bool optional = objectives[objectiveIndex].find(L"선택") != std::wstring::npos;
		m_Renderer->DrawString(-width * 0.5f + 28.0f,
			height * 0.5f - 68.0f - static_cast<float>(objectiveIndex) * 25.0f,
			objectives[objectiveIndex],
			optional ? 0.66f : 0.76f,
			optional ? 0.72f : 0.88f,
			optional ? 0.76f : 0.94f,
			1.0f);
	}
	if (m_Mode == OnFoot || m_Mode == FirstLevel)
		RenderGameplayMinimap();

	if (m_Mode == OnFoot || m_Mode == FirstLevel)
	{
		if (m_Mode == OnFoot)
		{
			const bool emergencyPower = m_CurrentShip == 0;
			m_Renderer->DrawRect(-width * 0.5f + 108.0f,
				-height * 0.5f + 91.0f,
				200.0f,
				24.0f,
				0.025f,
				0.04f,
				0.06f,
				0.90f);
			m_Renderer->DrawDiamond(-width * 0.5f + 24.0f,
				-height * 0.5f + 91.0f,
				10.0f,
				10.0f,
				emergencyPower ? 0.92f : 0.62f,
				emergencyPower ? 0.20f : 0.30f,
				emergencyPower ? 0.16f : 0.84f,
				0.96f);
			m_Renderer->DrawString(-width * 0.5f + 36.0f,
				-height * 0.5f + 88.0f,
				emergencyPower ? L"함선 상태: 비상 전력" : L"함선 상태: 생체 신호 감지",
				emergencyPower ? 0.94f : 0.72f,
				emergencyPower ? 0.42f : 0.58f,
				emergencyPower ? 0.30f : 0.90f,
				1.0f);
		}
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
		m_Renderer->DrawString(-width * 0.5f + 22.0f,
			-height * 0.5f + 60.0f,
			L"방향키 회전/추진 | 왼쪽 Shift 독립 부스터 | 목적지 근처 E 도킹",
			0.54f,
			0.72f,
			0.80f,
			0.92f);
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
		const float interactionWidth = std::min(
			std::max(210.0f, EstimatedTextWidth(interaction) + 28.0f),
			static_cast<float>(width) - 28.0f);
		m_Renderer->DrawRect(
			0.0f, -height * 0.5f + 27.0f, interactionWidth, 34.0f, 0.02f, 0.05f, 0.07f, 0.92f);
		m_Renderer->DrawString(-interactionWidth * 0.5f + 14.0f,
			-height * 0.5f + 23.0f,
			interaction,
			0.78f,
			0.94f,
			0.96f,
			1.0f);
	}
	if (m_MessageTimer > 0.0f)
	{
		const float statusWidth = std::min(430.0f, static_cast<float>(width) * 0.46f);
		const float statusLeft = -width * 0.5f + 14.0f;
		const float statusBottom = -height * 0.5f + 113.0f;
		const float horizontalPadding = 13.0f;
		const float verticalPadding = 11.0f;
		const float lineHeight = 22.0f;
		const std::vector<std::wstring> messageLines =
			WrapTextToWidth(m_Message, statusWidth - horizontalPadding * 2.0f);
		const float statusHeight = verticalPadding * 2.0f +
			lineHeight * static_cast<float>(messageLines.size());
		m_Renderer->DrawRect(statusLeft + statusWidth * 0.5f,
			statusBottom + statusHeight * 0.5f,
			statusWidth,
			statusHeight,
			0.025f,
			0.025f,
			0.045f,
			0.94f);
		m_Renderer->DrawRect(statusLeft + 3.0f,
			statusBottom + statusHeight * 0.5f,
			6.0f,
			statusHeight,
			0.94f,
			0.56f,
			0.20f,
			0.92f);
		for (size_t lineIndex = 0; lineIndex < messageLines.size(); ++lineIndex)
			m_Renderer->DrawString(statusLeft + horizontalPadding,
				statusBottom + statusHeight - verticalPadding - 17.0f -
					static_cast<float>(lineIndex) * lineHeight,
				messageLines[lineIndex],
				0.82f,
				0.82f,
				0.90f,
				1.0f);
	}
	if (m_DialogueTimer > 0.0f && (m_Mode == OnFoot || m_Mode == FirstLevel))
	{
		const float dialogueWidth = std::min(500.0f, static_cast<float>(width) * 0.50f);
		const float dialogueRight = width * 0.5f - 14.0f;
		const float dialogueLeft = dialogueRight - dialogueWidth;
		const float dialogueBottom =
			-height * 0.5f + (m_Mode == FirstLevel ? 105.0f : 52.0f);
		const float horizontalPadding = 13.0f;
		const float verticalPadding = 11.0f;
		const float lineHeight = 22.0f;
		const std::vector<std::wstring> dialogueLines =
			WrapTextToWidth(m_Dialogue, dialogueWidth - horizontalPadding * 2.0f);
		const float dialogueHeight = verticalPadding * 2.0f +
			lineHeight * static_cast<float>(dialogueLines.size());
		m_Renderer->DrawRect(dialogueLeft + dialogueWidth * 0.5f,
			dialogueBottom + dialogueHeight * 0.5f,
			dialogueWidth,
			dialogueHeight,
			0.025f,
			0.045f,
			0.055f,
			0.95f);
		m_Renderer->DrawRect(dialogueLeft + 3.0f,
			dialogueBottom + dialogueHeight * 0.5f,
			6.0f,
			dialogueHeight,
			0.20f,
			0.76f,
			0.84f,
			0.94f);
		for (size_t lineIndex = 0; lineIndex < dialogueLines.size(); ++lineIndex)
			m_Renderer->DrawString(dialogueLeft + horizontalPadding,
				dialogueBottom + dialogueHeight - verticalPadding - 17.0f -
					static_cast<float>(lineIndex) * lineHeight,
				dialogueLines[lineIndex],
				0.88f,
				0.92f,
				0.94f,
				1.0f);
	}
	if (m_Keys['\t'] && (m_Mode == OnFoot || m_Mode == FirstLevel))
		m_Renderer->DrawString(-220.0f,
			height * 0.5f - 58.0f,
			L"WASD 8방향 이동 | E 상호작용 | 마우스 조준·공격 | Space 회피",
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

void Game::RenderGameplayMinimap()
{
	const int screenWidth = m_Renderer->Width();
	const int screenHeight = m_Renderer->Height();
	const float centerX = screenWidth * 0.5f - 145.0f;
	const float centerY = screenHeight * 0.5f - 124.0f;
	const float panelWidth = 258.0f;
	const float panelHeight = 190.0f;
	const float mapHalfWidth = 111.0f;
	const float mapHalfHeight = 72.0f;
	const float worldHalfSize = m_Mode == FirstLevel ? 64.0f : 34.0f;

	m_Renderer->DrawDiamond(
		centerX, centerY, panelWidth + 8.0f, panelHeight + 8.0f, 0.10f, 0.42f, 0.50f, 0.78f);
	m_Renderer->DrawDiamond(
		centerX, centerY, panelWidth, panelHeight, 0.012f, 0.028f, 0.040f, 0.95f);
	m_Renderer->DrawString(centerX - 118.0f,
		centerY + 91.0f,
		m_Mode == FirstLevel ? L"에레보스 전술 지도" : L"노크티스 전술 지도",
		0.52f,
		0.88f,
		0.94f,
		1.0f);

	auto project = [centerX, centerY, mapHalfWidth, mapHalfHeight, worldHalfSize](
		const WorldPoint& point)
	{
		const float isoX = point.x - point.y;
		const float isoY = -(point.x + point.y) * 0.5f;
		return ScreenPoint{centerX + isoX / (worldHalfSize * 2.0f) * mapHalfWidth,
			centerY + isoY / worldHalfSize * mapHalfHeight};
	};
	auto drawMarker = [this, &project](
		const WorldPoint& point, float size, float red, float green, float blue, float alpha)
	{
		const ScreenPoint marker = project(point);
		m_Renderer->DrawDiamond(marker.x, marker.y, size, size * 0.68f, red, green, blue, alpha);
	};

	const std::vector<MapVisualTile>& terrain =
		m_Mode == FirstLevel ? m_FirstLevelMinimapTiles : m_TutorialMinimapTiles;
	const float terrainWidth = m_Mode == FirstLevel ? 7.5f : 10.0f;
	const float terrainHeight = m_Mode == FirstLevel ? 3.8f : 5.0f;
	for (size_t index = 0; index < terrain.size(); ++index)
	{
		const MapVisualTile& tile = terrain[index];
		const ScreenPoint position = project(tile.position);
		m_Renderer->DrawDiamond(position.x,
			position.y,
			terrainWidth,
			terrainHeight,
			tile.type == 2 ? 0.13f : 0.08f,
			tile.type == 2 ? 0.30f : 0.22f,
			tile.type == 2 ? 0.36f : 0.28f,
			0.92f);
	}

	const std::vector<LevelSurvivor>& survivors =
		m_Mode == FirstLevel ? m_LevelSurvivors : m_TutorialSurvivors;
	for (size_t index = 0; index < survivors.size(); ++index)
	{
		const LevelSurvivor& survivor = survivors[index];
		if (survivor.health <= 0.0f)
			drawMarker(survivor.position, 6.0f, 0.34f, 0.38f, 0.40f, 0.88f);
		else if (survivor.confused)
			drawMarker(survivor.position, 7.0f, 1.0f, 0.48f, 0.18f, 1.0f);
		else
			drawMarker(survivor.position, 6.5f, 0.34f, 0.90f, 0.58f, 1.0f);
	}
	if (m_Mode == OnFoot)
	{
		for (size_t index = 0; index < m_NeutralNpcs.size(); ++index)
		{
			const NeutralNpc& npc = m_NeutralNpcs[index];
			drawMarker(npc.position,
				6.0f,
				0.34f,
				npc.dead ? 0.38f : 0.90f,
				npc.dead ? 0.40f : 0.58f,
				0.90f);
		}
	}

	for (size_t index = 0; index < m_Enemies.size(); ++index)
	{
		const Enemy& enemy = m_Enemies[index];
		if (enemy.active && enemy.health > 0)
			drawMarker(enemy.position, 7.5f, 0.78f, 0.28f, 1.0f, 1.0f);
	}

	if (m_Mode == OnFoot)
	{
		for (size_t index = 0; index < 3; ++index)
			drawMarker(TutorialRecordPositions[index],
				6.0f,
				m_TutorialRecordsRead[index] ? 0.38f : 1.0f,
				m_TutorialRecordsRead[index] ? 0.42f : 0.78f,
				0.18f,
				0.95f);
		if (m_Quest == InspectTerminal)
			drawMarker(TerminalPosition, 8.0f, 0.20f, 0.72f, 0.92f, 1.0f);
		if (m_Quest == CollectTutorialWeapon)
			drawMarker(TutorialWeaponPosition, 8.0f, 1.0f, 0.78f, 0.18f, 1.0f);
		if (!m_CoreRecovered)
			drawMarker(CorePosition, 9.0f, 1.0f, 0.36f, 0.18f, 1.0f);
		if (m_CoreRecovered && !m_SignalChecked)
			drawMarker(SignalPosition, 8.0f, 0.86f, 0.34f, 0.94f, 1.0f);
		if (m_Quest == ActivateNoctisSystems)
			drawMarker(CockpitPosition, 9.0f, 0.20f, 0.82f, 0.94f, 1.0f);
		drawMarker(TutorialPersonalShipDockPosition, 9.0f, 0.22f, 0.76f, 1.0f, 1.0f);
	}
	else
	{
		drawMarker(FirstLevelPersonalShipDockPosition, 9.0f, 0.22f, 0.76f, 1.0f, 1.0f);
		for (size_t index = 0; index < m_StoryDocuments.size(); ++index)
			drawMarker(m_StoryDocuments[index].position,
				6.0f,
				m_StoryDocuments[index].read ? 0.38f : 1.0f,
				m_StoryDocuments[index].read ? 0.42f : 0.78f,
				0.18f,
				0.95f);
		for (size_t index = 0; index < m_MagazinePickups.size(); ++index)
			if (!m_MagazinePickups[index].collected)
				drawMarker(m_MagazinePickups[index].position, 6.5f, 0.20f, 0.76f, 0.94f, 1.0f);
		for (int engineIndex = 0; engineIndex < 4; ++engineIndex)
			if (!m_LevelEngineActive[engineIndex])
				drawMarker(m_LevelEnginePositions[engineIndex], 8.0f, 1.0f, 0.58f, 0.16f, 1.0f);
		drawMarker(m_LevelControlPosition, 9.0f, 0.22f, 0.84f, 0.94f, 1.0f);
		if (!m_LevelWeaponCollected)
			drawMarker(FirstLevelWeaponPosition, 8.0f, 1.0f, 0.78f, 0.18f, 1.0f);
	}

	drawMarker(m_Player, 10.0f, 0.22f, 0.92f, 1.0f, 1.0f);
	m_Renderer->DrawString(centerX - 118.0f,
		centerY - 97.0f,
		L"청록:플레이어  녹색:NPC  보라:공허종  노랑:목표",
		0.58f,
		0.70f,
		0.76f,
		0.90f);
}
