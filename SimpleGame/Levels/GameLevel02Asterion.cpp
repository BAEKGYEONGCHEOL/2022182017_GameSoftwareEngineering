#include "stdafx.h"
#include "../Game.h"

ShipLevelProfile Game::Level02AsterionProfile() const
{
	return {3,
		"asterion",
		L"의료선 아스테리온",
		{-400.0f, 280.0f},
		17,
		20,
		49,
		49,
		"remote_distress",
		"quarantine_silence",
		79,
		61,
		46};
}
