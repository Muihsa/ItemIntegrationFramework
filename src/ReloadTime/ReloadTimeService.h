#pragma once
#include "pch.h"

#include <optional>
#include <string_view>

namespace IIF::ReloadTime
{
	// Parses the active RACE subgraph table. Idempotent; call once after game load
	// (a lazy first-use call is also safe).
	void Initialize();

	// Reload clip length for a weapon, converted to in-game time with the
	// weapon's reloadSpeed. a_id accepts ReloadEmpty / ReloadReserve (plus the
	// WPNReload names and a few aliases). With a_multiplyBySpeed the result is
	// duration * reloadSpeed instead of duration / reloadSpeed.
	std::optional<float> GetReloadTime(RE::TESForm* a_form, RE::TBO_InstanceData* a_instance, std::string_view a_id, bool a_multiplyBySpeed);
}
