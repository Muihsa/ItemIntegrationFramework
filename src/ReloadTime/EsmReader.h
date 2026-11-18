#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace IIF::ReloadTime
{
	// One RACE "Subgraph Data" block reduced to what a reload lookup needs.
	// Keyword vectors hold runtime form IDs (load order already applied).
	struct SubgraphEntry
	{
		std::vector<std::uint32_t> targetKeywords;
		std::vector<std::uint32_t> actorKeywords;
		std::string behavior;
		std::vector<std::string> animDirs;
	};

	namespace Esm
	{
		struct ModIndex
		{
			std::uint8_t compileIndex{ 0xFF };
			std::uint16_t smallFileIndex{ 0xFFFF };
			bool light{ false };
		};

		struct PluginRef
		{
			std::string filename;  // file name only, used for master resolution
			std::string path;      // absolute path on disk
			ModIndex index;
		};

		// Scans a_plugins in load order and returns the weapon / first-person
		// entries of the subgraph data record the running game would use:
		//   HumanRace (a_humanRaceFormID) --SRAC--> subgraph data record,
		// merged with every additive record whose SADD points at it.
		// a_humanRaceFormID / a_defaultSubgraphFormID may be 0, in which case the
		// vanilla object IDs (0x00013746 / 0x00166729) are matched by raw ID.
		// a_recordCount (optional) receives the number of subgraph records parsed.
		bool LoadSubgraphEntries(const std::vector<PluginRef>& a_plugins, std::uint32_t a_humanRaceFormID,
			std::uint32_t a_defaultSubgraphFormID, std::vector<SubgraphEntry>& a_out,
			std::size_t* a_recordCount = nullptr);
	}
}
