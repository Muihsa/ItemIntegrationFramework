#pragma once
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "EsmReader.h"

namespace IIF::ReloadTime
{
	// The active RACE subgraph table (weapon / first-person entries) parsed once
	// after game load, plus the keyword-subset lookup the engine performs when it
	// picks the animation directory for a weapon.
	class SubgraphData
	{
	public:
		static SubgraphData& GetSingleton();

		// Builds the table from the loaded plugin files. Idempotent and safe to
		// call from the main thread only (it reads the plugin list).
		bool Initialize();

		[[nodiscard]] bool IsReady() const { return ready; }
		[[nodiscard]] std::size_t EntryCount() const { return entries.size(); }
		[[nodiscard]] const std::vector<SubgraphEntry>& Entries() const { return entries; }

		// Candidates: actor keywords ⊆ a_actorKeywords and target keywords ⊆
		// a_weaponKeywords. The most specific entry (largest target keyword set)
		// wins; ties prefer the later definition, matching "last wins" plugin
		// override semantics.
		[[nodiscard]] const SubgraphEntry* Select(const std::unordered_set<std::uint32_t>& a_weaponKeywords,
			const std::unordered_set<std::uint32_t>& a_actorKeywords) const;

	private:
		SubgraphData() = default;

		bool Load();

		std::vector<SubgraphEntry> entries;
		bool ready{ false };
	};
}
