#include "pch.h"
#include "ReloadTime/SubgraphData.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <mutex>

namespace IIF::ReloadTime
{
	namespace
	{
		bool EqualsNoCase(std::string_view a_value, std::string_view a_expected)
		{
			if (a_value.size() != a_expected.size()) return false;
			for (std::size_t i = 0; i < a_value.size(); ++i) {
				if (std::tolower(static_cast<unsigned char>(a_value[i])) != std::tolower(static_cast<unsigned char>(a_expected[i]))) return false;
			}
			return true;
		}

		std::string MakePluginPath(const RE::TESFile& a_file)
		{
			const std::string filename(a_file.filename);
			if (filename.empty()) return {};

			std::string directory(a_file.path);
			if (!directory.empty() && directory.back() != '\\' && directory.back() != '/') directory += '\\';

			std::vector<std::string> candidates;
			if (!directory.empty()) candidates.push_back(directory + filename);
			candidates.push_back(filename);

			// Fallbacks for a missing/odd TESFile::path: <game dir>\Data\<plugin>.
			wchar_t exePath[MAX_PATH]{};
			if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
				const auto dataDir = std::filesystem::path(exePath).parent_path() / "Data";
				candidates.push_back((dataDir / filename).string());
			}
			candidates.push_back((std::filesystem::path("Data") / filename).string());

			std::error_code ec;
			for (const auto& candidate : candidates) {
				if (std::filesystem::exists(candidate, ec)) return candidate;
				ec.clear();
			}
			return candidates.front();
		}

		bool IsSubset(const std::vector<std::uint32_t>& a_subset, const std::unordered_set<std::uint32_t>& a_superset)
		{
			for (const auto id : a_subset) {
				if (!a_superset.contains(id)) return false;
			}
			return true;
		}
	}

	SubgraphData& SubgraphData::GetSingleton()
	{
		static SubgraphData singleton;
		return singleton;
	}

	bool SubgraphData::Initialize()
	{
		// Not std::call_once: a failed attempt (early call before the plugin list
		// is compiled) must be retryable, e.g. from the kGameLoaded handler.
		static std::mutex mutex;
		const std::lock_guard lock(mutex);
		if (ready) return true;
		ready = Load();
		return ready;
	}

	const SubgraphEntry* SubgraphData::Select(const std::unordered_set<std::uint32_t>& a_weaponKeywords,
		const std::unordered_set<std::uint32_t>& a_actorKeywords) const
	{
		const SubgraphEntry* best = nullptr;
		std::size_t bestSize = 0;
		const SubgraphEntry* wildcard = nullptr;

		for (const auto& entry : entries) {
			if (!IsSubset(entry.actorKeywords, a_actorKeywords)) continue;

			// Entries without target keywords only serve as a last-resort default.
			if (entry.targetKeywords.empty()) {
				wildcard = &entry;
				continue;
			}
			if (!IsSubset(entry.targetKeywords, a_weaponKeywords)) continue;

			if (entry.targetKeywords.size() >= bestSize) {
				best = &entry;
				bestSize = entry.targetKeywords.size();
			}
		}

		return best ? best : wildcard;
	}

	bool SubgraphData::Load()
	{
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			REX::WARN("[IIF] Reload time: TESDataHandler unavailable; subgraph table not built.");
			return false;
		}

		// Merge every source that can enumerate the loaded plugins, then apply a
		// stable load order (regular by compile index, light plugins afterwards).
		std::map<std::string, Esm::PluginRef> unique;

		const auto addFile = [&unique](const RE::TESFile* a_file) {
			if (!a_file || a_file->compileIndex == 0xFF) return;
			if (a_file->filename[0] == '\0') return;

			Esm::PluginRef ref;
			ref.filename = a_file->filename;
			ref.path = MakePluginPath(*a_file);
			ref.index.compileIndex = a_file->compileIndex;
			ref.index.smallFileIndex = a_file->smallFileCompileIndex;
			ref.index.light = a_file->flags.all(RE::TESFile::RecordFlag::kSmallFile);

			std::string key = ref.filename;
			std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			unique[key] = std::move(ref);
		};

		for (auto* file : dataHandler->files) addFile(file);
		for (auto* file : dataHandler->compiledFileCollection.files) addFile(file);
		for (auto* file : dataHandler->compiledFileCollection.smallFiles) addFile(file);

		std::vector<Esm::PluginRef> plugins;
		plugins.reserve(unique.size());
		for (auto& [name, ref] : unique) plugins.push_back(ref);

		std::sort(plugins.begin(), plugins.end(), [](const Esm::PluginRef& a_lhs, const Esm::PluginRef& a_rhs) {
			if (a_lhs.index.light != a_rhs.index.light) return !a_lhs.index.light;
			return a_lhs.index.light ? a_lhs.index.smallFileIndex < a_rhs.index.smallFileIndex
			                         : a_lhs.index.compileIndex < a_rhs.index.compileIndex;
		});

		std::uint32_t humanRaceFormID = 0;
		std::uint32_t defaultSubgraphFormID = 0;
		if (const auto* humanRace = dataHandler->LookupForm(0x00013746, "Fallout4.esm")) {
			humanRaceFormID = humanRace->GetFormID();
		}
		if (const auto* subgraph = dataHandler->LookupForm(0x00166729, "Fallout4.esm")) {
			defaultSubgraphFormID = subgraph->GetFormID();
		}

		std::vector<SubgraphEntry> loaded;
		std::size_t recordCount = 0;
		const bool ok = Esm::LoadSubgraphEntries(plugins, humanRaceFormID, defaultSubgraphFormID, loaded, &recordCount);
		if (!ok) {
			REX::WARN("[IIF] Reload time: no RACE subgraph data found ({} plugins, humanRace={:08X}, subgraph={:08X}).",
				plugins.size(), humanRaceFormID, defaultSubgraphFormID);
			for (const auto& plugin : plugins) {
				if (EqualsNoCase(plugin.filename, "Fallout4.esm")) {
					std::error_code ec;
					REX::WARN("[IIF] Reload time: Fallout4.esm path='{}' exists={}", plugin.path,
						std::filesystem::exists(plugin.path, ec));
					break;
				}
			}
			return false;
		}

		entries = std::move(loaded);
		REX::INFO("[IIF] Reload time: {} weapon/first-person subgraph entries from {} subgraph record(s), {} plugins.",
			entries.size(), recordCount, plugins.size());
		return true;
	}
}
