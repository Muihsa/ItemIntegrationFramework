#include "pch.h"
#include "ReloadTime/ReloadTimeService.h"

#include "ReloadTime/HkxReader.h"
#include "ReloadTime/SubgraphData.h"

#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace IIF::ReloadTime
{
	namespace
	{
		enum class ReloadClip
		{
			kNone,
			kEmpty,
			kReserve
		};

		bool EqualsNoCase(std::string_view a_value, std::string_view a_expected)
		{
			if (a_value.size() != a_expected.size()) return false;
			for (std::size_t i = 0; i < a_value.size(); ++i) {
				if (std::tolower(static_cast<unsigned char>(a_value[i])) != std::tolower(static_cast<unsigned char>(a_expected[i]))) return false;
			}
			return true;
		}

		ReloadClip ParseClipID(std::string_view a_id)
		{
			if (EqualsNoCase(a_id, "ReloadEmpty") || EqualsNoCase(a_id, "Empty") ||
				EqualsNoCase(a_id, "Reload") || EqualsNoCase(a_id, "WPNReload")) {
				return ReloadClip::kEmpty;
			}
			if (EqualsNoCase(a_id, "ReloadReserve") || EqualsNoCase(a_id, "Reserve") ||
				EqualsNoCase(a_id, "Tactical") || EqualsNoCase(a_id, "TacticalReload") ||
				EqualsNoCase(a_id, "WPNReloadReserve")) {
				return ReloadClip::kReserve;
			}
			return ReloadClip::kNone;
		}

		std::string_view LeafFor(ReloadClip a_clip)
		{
			return a_clip == ReloadClip::kReserve ? std::string_view("WPNReloadReserve") : std::string_view("WPNReload");
		}

		std::uint64_t HashString(std::string_view a_value)
		{
			std::uint64_t hash = 1469598103934665603ull;
			for (const char c : a_value) {
				hash ^= static_cast<unsigned char>(c);
				hash *= 1099511628211ull;
			}
			return hash;
		}

		// Bounded: the first few distinct misses per (weapon, clip) are logged.
		void LogMiss(std::uint32_t a_weaponID, int a_clip, std::string_view a_reason, const std::string& a_detail)
		{
			static std::unordered_set<std::uint64_t> seen;
			static std::size_t logged = 0;
			if (logged >= 64) return;

			const std::uint64_t key = (static_cast<std::uint64_t>(a_weaponID) << 12) ^
			                          (static_cast<std::uint64_t>(a_clip) << 8) ^
			                          HashString(a_reason);
			if (!seen.insert(key).second) return;
			++logged;

			REX::INFO("[IIF] Reload time miss: weapon={:08X} clip={} reason={} detail={}",
				a_weaponID, a_clip ? "reserve" : "empty", a_reason, a_detail);
		}

		std::string DescribeDirs(const SubgraphEntry& a_entry)
		{
			std::string text;
			for (std::size_t i = 0; i < a_entry.animDirs.size() && i < 3; ++i) {
				if (!text.empty()) text += " | ";
				text += a_entry.animDirs[i];
			}
			return text;
		}

		void AddKeywordForm(const RE::BGSKeywordForm* a_form, std::unordered_set<std::uint32_t>& a_out)
		{
			if (!a_form || !a_form->keywords) return;
			const auto count = a_form->numKeywords;
			if (count == 0 || count > 4096) return;
			for (std::uint32_t i = 0; i < count; ++i) {
				if (const auto* keyword = a_form->keywords[i]) a_out.insert(keyword->formID);
			}
		}

		// No C++ objects with destructors in these functions: __try lives in the
		// callers below (MSVC C2712).
		const RE::BGSKeywordForm* ReadInstanceKeywordsUnsafe(RE::TBO_InstanceData* a_instance)
		{
			return static_cast<RE::TESObjectWEAP::InstanceData*>(a_instance)->keywords;
		}

		const RE::BGSKeywordForm* SafeInstanceKeywords(RE::TBO_InstanceData* a_instance)
		{
			__try {
				return ReadInstanceKeywordsUnsafe(a_instance);
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				return nullptr;
			}
		}

		float ReadReloadSpeedUnsafe(RE::TESObjectWEAP* a_weapon, RE::TBO_InstanceData* a_instance)
		{
			if (a_instance) return static_cast<RE::TESObjectWEAP::InstanceData*>(a_instance)->reloadSpeed;
			return a_weapon->weaponData.reloadSpeed;
		}

		float SafeReloadSpeed(RE::TESObjectWEAP* a_weapon, RE::TBO_InstanceData* a_instance)
		{
			__try {
				return ReadReloadSpeedUnsafe(a_weapon, a_instance);
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				return 0.0f;
			}
		}

		std::uint64_t HashKeywords(const std::unordered_set<std::uint32_t>& a_keywords)
		{
			std::vector<std::uint32_t> sorted(a_keywords.begin(), a_keywords.end());
			std::sort(sorted.begin(), sorted.end());

			std::uint64_t hash = 1469598103934665603ull;  // FNV-1a offset basis
			for (const auto value : sorted) {
				hash ^= value;
				hash *= 1099511628211ull;
			}
			return hash;
		}

		struct WeaponKeywords
		{
			std::unordered_set<std::uint32_t> values;
			std::uint64_t hash{ 0 };
		};

		WeaponKeywords CollectWeaponKeywords(RE::TESObjectWEAP* a_weapon, RE::TBO_InstanceData* a_instance)
		{
			WeaponKeywords result;

			// Base weapon keywords plus the OMOD-merged instance keywords; the
			// latter is where Anims* keywords added by mods end up.
			AddKeywordForm(a_weapon, result.values);
			const RE::BGSKeywordForm* instanceKeywords = a_instance ? SafeInstanceKeywords(a_instance) : nullptr;
			AddKeywordForm(instanceKeywords, result.values);
			if (!a_instance) AddKeywordForm(a_weapon->weaponData.keywords, result.values);

			result.hash = HashKeywords(result.values);
			return result;
		}

		const std::unordered_set<std::uint32_t>& ActorKeywords()
		{
			static std::unordered_set<std::uint32_t> keywords;
			static bool initialized = false;
			if (!initialized) {
				if (auto* player = RE::PlayerCharacter::GetSingleton()) {
					AddKeywordForm(player->race, keywords);
					initialized = true;
				}
			}
			return keywords;
		}

		// (weapon form ID, keyword set hash) -> subgraph entry index, -1 when none.
		std::unordered_map<std::uint64_t, std::int32_t>& SelectionCache()
		{
			static std::unordered_map<std::uint64_t, std::int32_t> cache;
			return cache;
		}

		// (entry index, clip) -> resolved animation resource path, empty when none.
		std::unordered_map<std::uint64_t, std::string>& AnimationPathCache()
		{
			static std::unordered_map<std::uint64_t, std::string> cache;
			return cache;
		}

		std::string ResolveAnimationPath(const SubgraphEntry& a_entry, std::string_view a_leaf)
		{
			for (const auto& directory : a_entry.animDirs) {
				if (directory.empty()) continue;

				std::string candidate = directory;
				if (candidate.back() != '\\' && candidate.back() != '/') candidate += '\\';
				candidate.append(a_leaf);
				candidate += ".hkx";

				if (ResourceExists(candidate)) return candidate;
			}
			return {};
		}

		// Item cards can be evaluated from more than one thread; the caches below
		// are plain containers, so serialise lookups.
		std::mutex& ServiceMutex()
		{
			static std::mutex mutex;
			return mutex;
		}
	}

	void Initialize()
	{
		if (!SubgraphData::GetSingleton().Initialize()) {
			REX::WARN("[IIF] Reload time: subgraph data unavailable, reload time cards stay hidden.");
		}
	}

	std::optional<float> GetReloadTime(RE::TESForm* a_form, RE::TBO_InstanceData* a_instance, std::string_view a_id, bool a_multiplyBySpeed)
	{
		const std::lock_guard serviceLock(ServiceMutex());

		{
			static bool loggedFirst = false;
			if (!loggedFirst) {
				loggedFirst = true;
				REX::INFO("[IIF] Reload time: first lookup formID={:08X} id='{}' instance={}",
					a_form ? a_form->GetFormID() : 0, std::string(a_id), a_instance != nullptr);
			}
		}

		if (!a_form || !a_form->Is(RE::ENUM_FORM_ID::kWEAP)) return std::nullopt;

		auto* weapon = a_form->As<RE::TESObjectWEAP>();
		if (!weapon) return std::nullopt;

		const auto clip = ParseClipID(a_id);
		if (clip == ReloadClip::kNone) return std::nullopt;

		const std::uint32_t weaponID = weapon->GetFormID();
		const int clipIndex = clip == ReloadClip::kReserve ? 1 : 0;
		const auto miss = [weaponID, clipIndex](std::string_view a_reason, std::string a_detail = {}) -> std::optional<float> {
			LogMiss(weaponID, clipIndex, a_reason, a_detail);
			return std::nullopt;
		};

		auto& subgraph = SubgraphData::GetSingleton();
		if (!subgraph.IsReady() && !subgraph.Initialize()) {
			return miss("subgraph unavailable", "entries=0");
		}

		const auto keywords = CollectWeaponKeywords(weapon, a_instance);
		const std::uint64_t selectionKey = (static_cast<std::uint64_t>(weaponID) << 32) ^ keywords.hash;

		auto& selections = SelectionCache();
		auto selection = selections.find(selectionKey);
		if (selection == selections.end()) {
			const auto* entry = subgraph.Select(keywords.values, ActorKeywords());
			std::int32_t index = -1;
			if (entry) {
				const auto& entries = subgraph.Entries();
				if (entry >= entries.data() && entry < entries.data() + entries.size()) {
					index = static_cast<std::int32_t>(entry - entries.data());
				}
			}
			selection = selections.emplace(selectionKey, index).first;
		}
		if (selection->second < 0) {
			return miss("no subgraph entry", "kws=" + std::to_string(keywords.values.size()) +
			                                    " actorKws=" + std::to_string(ActorKeywords().size()) +
			                                    " entries=" + std::to_string(subgraph.EntryCount()));
		}

		const auto& entries = subgraph.Entries();
		const std::size_t entryIndex = static_cast<std::size_t>(selection->second);
		if (entryIndex >= entries.size()) return miss("entry index out of range", {});
		const auto& entry = entries[entryIndex];

		const auto leaf = LeafFor(clip);
		const std::uint64_t pathKey = (static_cast<std::uint64_t>(entryIndex) << 8) | static_cast<std::uint64_t>(clip);

		auto& paths = AnimationPathCache();
		auto path = paths.find(pathKey);
		if (path == paths.end()) {
			path = paths.emplace(pathKey, ResolveAnimationPath(entry, leaf)).first;
		}
		if (path->second.empty()) {
			return miss("no animation file", "behavior=" + entry.behavior + " dirs=" + DescribeDirs(entry));
		}

		if (entry.behavior.empty() || !BehaviorDefinesClip(entry.behavior, leaf)) {
			return miss("behavior lacks clip", entry.behavior);
		}

		const auto duration = ReadAnimationDuration(path->second);
		if (!duration) return miss("duration unavailable", path->second);

		const float reloadSpeed = SafeReloadSpeed(weapon, a_instance);
		if (!std::isfinite(reloadSpeed) || reloadSpeed <= 0.0f) {
			return miss("invalid reload speed", std::to_string(reloadSpeed));
		}

		const float seconds = a_multiplyBySpeed ? (*duration * reloadSpeed) : (*duration / reloadSpeed);
		if (!std::isfinite(seconds) || seconds <= 0.0f) return miss("invalid result", std::to_string(seconds));

		{
			static std::size_t logged = 0;
			if (logged < 6) {
				++logged;
				REX::INFO("[IIF] Reload time: weapon={:08X} clip={} = {:.3f}s (duration={:.3f}, speed={:.3f}, path={})",
					weaponID, clipIndex ? "reserve" : "empty", seconds, *duration, reloadSpeed, path->second);
			}
		}
		return seconds;
	}
}
