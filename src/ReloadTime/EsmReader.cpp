#include "EsmReader.h"

#include "Inflate.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string_view>
#include <unordered_map>

// Reads weapon / first-person subgraph entries straight out of the loaded plugin
// files. This mirrors what the engine's TESRace loader registers, without
// depending on version-specific engine addresses:
//
//   HumanRace (RACE 0x00013746) --SRAC--> HumanRaceSubGraphData
//   ... plus every RACE record whose SADD points at that record (transitively):
//   the DLCs and animation mods keep their entries in their own additive
//   subgraph records instead of editing the vanilla one.
//
// Each block is SAKD* / SGNM / SAPT* / STKD* / SRAF.

namespace IIF::ReloadTime::Esm
{
	namespace
	{
		constexpr std::uint32_t kCompressedFlag = 0x00040000;
		constexpr std::uint32_t kVanillaHumanRaceRaw = 0x00013746;
		constexpr std::uint32_t kVanillaSubgraphRaw = 0x00166729;
		constexpr std::uint32_t kMaxPluginHeaderSize = 16u * 1024u * 1024u;
		constexpr std::uint32_t kMaxRaceGroupSize = 256u * 1024u * 1024u;
		constexpr std::uint32_t kMaxRecordSize = 64u * 1024u * 1024u;

		constexpr std::uint16_t kRoleWeapon = 1;
		constexpr std::uint16_t kPerspectiveFirst = 1;

		std::uint16_t Read16(const std::uint8_t* a_p) { return static_cast<std::uint16_t>(a_p[0] | (a_p[1] << 8)); }
		std::uint32_t Read32(const std::uint8_t* a_p)
		{
			return static_cast<std::uint32_t>(a_p[0]) |
			       (static_cast<std::uint32_t>(a_p[1]) << 8) |
			       (static_cast<std::uint32_t>(a_p[2]) << 16) |
			       (static_cast<std::uint32_t>(a_p[3]) << 24);
		}

		std::string ReadCString(const std::uint8_t* a_p, std::size_t a_size)
		{
			std::size_t length = 0;
			while (length < a_size && a_p[length] != 0) ++length;
			return std::string(reinterpret_cast<const char*>(a_p), length);
		}

		std::string ToLower(std::string a_value)
		{
			std::transform(a_value.begin(), a_value.end(), a_value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_value;
		}

		bool IsType(const char* a_type, const char* a_expected) { return std::memcmp(a_type, a_expected, 4) == 0; }

		class FileReader
		{
		public:
			[[nodiscard]] bool Open(const std::string& a_path)
			{
				file.open(a_path, std::ios::binary);
				if (!file.is_open()) return false;
				file.seekg(0, std::ios::end);
				const auto end = file.tellg();
				if (end <= 0) return false;
				size = static_cast<std::uint64_t>(end);
				return true;
			}

			[[nodiscard]] std::uint64_t Size() const { return size; }

			[[nodiscard]] bool ReadAt(std::uint64_t a_offset, void* a_dst, std::size_t a_bytes)
			{
				if (a_bytes == 0) return true;
				if (a_offset > size || a_bytes > size - a_offset) return false;
				file.clear();
				file.seekg(static_cast<std::streamoff>(a_offset), std::ios::beg);
				if (!file) return false;
				file.read(static_cast<char*>(a_dst), static_cast<std::streamsize>(a_bytes));
				return file.gcount() == static_cast<std::streamsize>(a_bytes);
			}

		private:
			std::ifstream file;
			std::uint64_t size{ 0 };
		};

		struct PluginContext
		{
			PluginRef ref;
			ModIndex index;
			std::vector<std::string> masters;  // lowercase master names; index 0 = first master
			std::uint64_t firstGroupOffset{ 0 };
			bool valid{ false };
		};

		// Resolves a file-local form ID to a runtime form ID. In plugin files the
		// high byte is a master index, except that `masterCount` means "this file".
		bool RemapFormID(std::uint32_t a_raw, const PluginContext& a_plugin, const std::unordered_map<std::string, ModIndex>& a_mods, std::uint32_t& a_out)
		{
			a_out = 0;
			if (a_raw == 0) return false;

			const auto high = static_cast<std::uint8_t>(a_raw >> 24);
			const auto masterCount = static_cast<std::size_t>(a_plugin.masters.size());

			ModIndex target{};
			bool found = false;

			if (high < masterCount) {
				const auto it = a_mods.find(a_plugin.masters[high]);
				if (it != a_mods.end()) {
					target = it->second;
					found = true;
				}
			}
			else if (high == masterCount) {
				target = a_plugin.index;
				found = true;
			}
			else if (high == 0xFE) {
				// Reference into a light master: its small file index lives in bits 12..23.
				const std::uint16_t lightIndex = static_cast<std::uint16_t>((a_raw >> 12) & 0xFFF);
				for (const auto& [name, index] : a_mods) {
					if (index.light && index.smallFileIndex == lightIndex) {
						target = index;
						found = true;
						break;
					}
				}
			}

			if (!found || target.compileIndex == 0xFF) return false;

			if (target.light) {
				a_out = 0xFE000000u | (static_cast<std::uint32_t>(target.smallFileIndex) << 12) | (a_raw & 0x00000FFFu);
			}
			else {
				a_out = (static_cast<std::uint32_t>(target.compileIndex) << 24) | (a_raw & 0x00FFFFFFu);
			}
			return true;
		}

		template <class Fn>
		void ForEachSubrecord(const std::uint8_t* a_data, std::size_t a_size, Fn&& a_fn)
		{
			std::size_t pos = 0;
			std::uint32_t extended = 0;
			while (pos + 6 <= a_size) {
				const char* type = reinterpret_cast<const char*>(a_data + pos);
				const std::uint16_t storedSize = Read16(a_data + pos + 4);
				pos += 6;

				if (IsType(type, "XXXX")) {
					if (pos + 4 > a_size) return;
					extended = Read32(a_data + pos);
					pos += 4;
					continue;
				}

				const std::size_t length = extended != 0 ? extended : storedSize;
				extended = 0;
				if (pos + length > a_size) return;
				a_fn(type, a_data + pos, length);
				pos += length;
			}
		}

		bool InflateRecordBody(const std::uint8_t* a_data, std::size_t a_size, std::vector<std::uint8_t>& a_out)
		{
			// Compressed record: u32 uncompressed size, then a zlib stream.
			if (a_size < 4) return false;
			const std::uint32_t uncompressed = Read32(a_data);
			if (uncompressed == 0 || uncompressed > kMaxRecordSize) return false;
			return ZlibDecompress(a_data + 4, a_size - 4, uncompressed, a_out);
		}

		bool ReadRecordData(FileReader& a_reader, std::uint64_t a_dataOffset, std::uint32_t a_dataSize, std::uint32_t a_flags, std::vector<std::uint8_t>& a_out)
		{
			a_out.clear();
			if (a_dataSize == 0 || a_dataSize > kMaxRecordSize) return false;
			a_out.resize(a_dataSize);
			if (!a_reader.ReadAt(a_dataOffset, a_out.data(), a_out.size())) {
				a_out.clear();
				return false;
			}
			if ((a_flags & kCompressedFlag) == 0) return true;

			std::vector<std::uint8_t> inflated;
			if (!InflateRecordBody(a_out.data(), a_out.size(), inflated)) {
				a_out.clear();
				return false;
			}
			a_out = std::move(inflated);
			return true;
		}

		bool ReadPluginHeader(FileReader& a_reader, PluginContext& a_plugin)
		{
			std::uint8_t header[24];
			if (!a_reader.ReadAt(0, header, sizeof(header))) return false;
			if (std::memcmp(header, "TES4", 4) != 0) return false;

			const std::uint32_t dataSize = Read32(header + 4);
			if (dataSize == 0 || dataSize > kMaxPluginHeaderSize) return false;

			std::vector<std::uint8_t> data(dataSize);
			if (!a_reader.ReadAt(24, data.data(), data.size())) return false;

			a_plugin.masters.clear();
			ForEachSubrecord(data.data(), data.size(), [&](const char* a_type, const std::uint8_t* a_body, std::size_t a_length) {
				if (IsType(a_type, "MAST")) a_plugin.masters.push_back(ToLower(ReadCString(a_body, a_length)));
			});

			a_plugin.firstGroupOffset = 24ull + dataSize;
			return a_plugin.firstGroupOffset <= a_reader.Size();
		}

		bool FindRaceGroup(FileReader& a_reader, std::uint64_t a_firstGroupOffset, std::uint64_t& a_groupOffset, std::uint32_t& a_groupSize)
		{
			std::uint64_t pos = a_firstGroupOffset;
			while (pos + 24 <= a_reader.Size()) {
				std::uint8_t header[24];
				if (!a_reader.ReadAt(pos, header, sizeof(header))) return false;

				const std::uint32_t size = Read32(header + 4);
				if (std::memcmp(header, "GRUP", 4) == 0) {
					if (size < 24) return false;
					if (std::memcmp(header + 8, "RACE", 4) == 0) {
						if (size > kMaxRaceGroupSize || pos + size > a_reader.Size()) return false;
						a_groupOffset = pos;
						a_groupSize = size;
						return true;
					}
					pos += size;
				}
				else {
					pos += 24ull + size;
				}
			}
			return false;
		}

		struct RecordLocation
		{
			std::size_t pluginIndex{ 0 };
			std::uint64_t dataOffset{ 0 };
			std::uint32_t dataSize{ 0 };
			std::uint32_t flags{ 0 };
		};

		struct RaceRecordInfo
		{
			RecordLocation location;
			std::uint32_t subgraphField{ 0 };  // SRAC (runtime form ID)
			std::uint32_t additiveField{ 0 };  // SADD (runtime form ID)
		};

		// Walks a RACE group already in memory (records plus nested block groups).
		// a_fileBase is the absolute file offset of a_data[0]; the callback also
		// receives the record body pointer so SRAC/SADD can be read without I/O.
		template <class Fn>
		void WalkRaceGroup(const std::uint8_t* a_data, std::size_t a_size, std::uint64_t a_fileBase, int a_depth, Fn&& a_fn)
		{
			if (a_depth > 8) return;

			std::size_t pos = 0;
			while (pos + 24 <= a_size) {
				const std::uint8_t* header = a_data + pos;

				if (std::memcmp(header, "GRUP", 4) == 0) {
					const std::uint32_t groupSize = Read32(header + 4);
					if (groupSize < 24 || pos + groupSize > a_size) return;
					WalkRaceGroup(a_data + pos + 24, groupSize - 24, a_fileBase + pos + 24, a_depth + 1, a_fn);
					pos += groupSize;
					continue;
				}

				const std::uint32_t dataSize = Read32(header + 4);
				if (pos + 24ull + dataSize > a_size) return;
				a_fn(header, a_data + pos + 24, dataSize, a_fileBase + pos + 24);
				pos += 24ull + dataSize;
			}
		}

		struct SubgraphBlock
		{
			std::vector<std::uint32_t> targetKeywords;
			std::vector<std::uint32_t> actorKeywords;
			std::string behavior;
			std::vector<std::string> animDirs;
			bool broken{ false };

			void Reset()
			{
				targetKeywords.clear();
				actorKeywords.clear();
				behavior.clear();
				animDirs.clear();
				broken = false;
			}
		};

		void ParseSubgraphData(const std::vector<std::uint8_t>& a_record, const PluginContext& a_plugin,
			const std::unordered_map<std::string, ModIndex>& a_mods, std::vector<SubgraphEntry>& a_out)
		{
			SubgraphBlock block{};

			const auto finishBlock = [&](std::uint16_t a_role, std::uint16_t a_perspective) {
				if (a_role == kRoleWeapon && a_perspective == kPerspectiveFirst &&
					!block.broken && !block.behavior.empty() && !block.animDirs.empty()) {
					SubgraphEntry entry;
					entry.targetKeywords = std::move(block.targetKeywords);
					entry.actorKeywords = std::move(block.actorKeywords);
					entry.behavior = std::move(block.behavior);
					entry.animDirs = std::move(block.animDirs);
					a_out.push_back(std::move(entry));
				}
				block.Reset();
			};

			ForEachSubrecord(a_record.data(), a_record.size(),
				[&](const char* a_type, const std::uint8_t* a_body, std::size_t a_length) {
					if (IsType(a_type, "STKD") || IsType(a_type, "SAKD")) {
						if (a_length < 4) return;
						std::uint32_t runtimeID = 0;
						if (!RemapFormID(Read32(a_body), a_plugin, a_mods, runtimeID)) {
							block.broken = true;
							return;
						}
						if (IsType(a_type, "STKD")) block.targetKeywords.push_back(runtimeID);
						else block.actorKeywords.push_back(runtimeID);
					}
					else if (IsType(a_type, "SGNM")) {
						block.behavior = ReadCString(a_body, a_length);
					}
					else if (IsType(a_type, "SAPT")) {
						block.animDirs.push_back(ReadCString(a_body, a_length));
					}
					else if (IsType(a_type, "SRAF")) {
						if (a_length < 4) return;
						finishBlock(Read16(a_body), Read16(a_body + 2));
					}
				});
		}
	}

	bool LoadSubgraphEntries(const std::vector<PluginRef>& a_plugins, std::uint32_t a_humanRaceFormID,
		std::uint32_t a_defaultSubgraphFormID, std::vector<SubgraphEntry>& a_out,
		std::size_t* a_recordCount)
	{
		a_out.clear();
		if (a_recordCount) *a_recordCount = 0;
		if (a_plugins.empty()) return false;

		std::unordered_map<std::string, ModIndex> mods;
		for (const auto& plugin : a_plugins) {
			if (plugin.index.compileIndex == 0xFF) continue;
			mods[ToLower(plugin.filename)] = plugin.index;
		}

		std::vector<PluginContext> contexts(a_plugins.size());
		for (std::size_t i = 0; i < a_plugins.size(); ++i) {
			auto& context = contexts[i];
			context.ref = a_plugins[i];
			context.index = a_plugins[i].index;
			if (context.index.compileIndex == 0xFF) continue;

			FileReader reader;
			if (!reader.Open(context.ref.path)) continue;
			context.valid = ReadPluginHeader(reader, context);
		}

		// Pass 1: index every RACE record, its SRAC/SADD and the winning HumanRace
		// subgraph link.
		std::unordered_map<std::uint32_t, RaceRecordInfo> raceRecords;
		std::uint32_t humanRaceSubgraphID = 0;
		std::uint32_t humanRaceAdditiveID = 0;
		std::uint32_t vanillaSubgraphID = 0;

		for (std::size_t i = 0; i < contexts.size(); ++i) {
			const auto& context = contexts[i];
			if (!context.valid) continue;

			FileReader reader;
			if (!reader.Open(context.ref.path)) continue;

			std::uint64_t groupOffset = 0;
			std::uint32_t groupSize = 0;
			if (!FindRaceGroup(reader, context.firstGroupOffset, groupOffset, groupSize)) continue;

			std::vector<std::uint8_t> group(groupSize);
			if (!reader.ReadAt(groupOffset, group.data(), group.size())) continue;

			std::vector<std::uint8_t> decompressed;
			WalkRaceGroup(group.data() + 24, groupSize - 24ull, groupOffset + 24, 0,
				[&](const std::uint8_t* a_header, const std::uint8_t* a_body, std::uint32_t a_dataSize, std::uint64_t a_dataOffset) {
					if (std::memcmp(a_header, "RACE", 4) != 0) return;

					const std::uint32_t raw = Read32(a_header + 12);
					std::uint32_t runtimeID = 0;
					if (!RemapFormID(raw, context, mods, runtimeID)) return;

					RaceRecordInfo info;
					info.location = RecordLocation{ i, a_dataOffset, a_dataSize, Read32(a_header + 8) };

					const std::uint8_t* body = a_body;
					std::size_t bodySize = a_dataSize;
					if ((info.location.flags & kCompressedFlag) != 0) {
						if (InflateRecordBody(a_body, a_dataSize, decompressed)) {
							body = decompressed.data();
							bodySize = decompressed.size();
						}
						else {
							body = nullptr;
						}
					}

					if (body) {
						ForEachSubrecord(body, bodySize, [&](const char* a_type, const std::uint8_t* a_value, std::size_t a_length) {
							if (a_length < 4) return;
							if (IsType(a_type, "SRAC")) {
								std::uint32_t id = 0;
								if (RemapFormID(Read32(a_value), context, mods, id)) info.subgraphField = id;
							}
							else if (IsType(a_type, "SADD")) {
								std::uint32_t id = 0;
								if (RemapFormID(Read32(a_value), context, mods, id)) info.additiveField = id;
							}
						});
					}

					// Later plugins override earlier ones.
					raceRecords[runtimeID] = info;
					if (raw == kVanillaSubgraphRaw) vanillaSubgraphID = runtimeID;

					const bool isHumanRace = a_humanRaceFormID != 0 ? runtimeID == a_humanRaceFormID : raw == kVanillaHumanRaceRaw;
					if (isHumanRace) {
						if (info.subgraphField != 0) humanRaceSubgraphID = info.subgraphField;
						if (info.additiveField != 0) humanRaceAdditiveID = info.additiveField;
					}
				});
		}

		std::uint32_t baseID = humanRaceSubgraphID != 0 ? humanRaceSubgraphID
		                                                : (humanRaceAdditiveID != 0 ? humanRaceAdditiveID : a_defaultSubgraphFormID);
		if (baseID == 0) baseID = vanillaSubgraphID;
		if (baseID == 0 || !raceRecords.contains(baseID)) return false;

		// Additive subgraph data: any RACE record whose SADD points at the base
		// record (or at another additive record). The engine merges those into the
		// same race table, which is how DLCs and animation mods ship weapon entries.
		std::set<std::uint32_t> collection{ baseID };
		bool changed = true;
		while (changed) {
			changed = false;
			for (const auto& [id, info] : raceRecords) {
				if (info.additiveField == 0 || !collection.contains(info.additiveField)) continue;
				if (collection.insert(id).second) changed = true;
			}
		}

		// Base first, then additive records in plugin load order (later plugins and
		// additive records win keyword-size ties in SubgraphData::Select).
		struct OrderedID
		{
			std::size_t pluginIndex{ 0 };
			std::uint64_t dataOffset{ 0 };
			std::uint32_t id{ 0 };
		};

		std::vector<OrderedID> ordered;
		ordered.reserve(collection.size());
		for (const auto id : collection) {
			if (id == baseID) continue;
			const auto it = raceRecords.find(id);
			if (it == raceRecords.end()) continue;
			ordered.push_back(OrderedID{ it->second.location.pluginIndex, it->second.location.dataOffset, id });
		}
		std::sort(ordered.begin(), ordered.end(), [](const OrderedID& a_lhs, const OrderedID& a_rhs) {
			if (a_lhs.pluginIndex != a_rhs.pluginIndex) return a_lhs.pluginIndex < a_rhs.pluginIndex;
			return a_lhs.dataOffset < a_rhs.dataOffset;
		});

		std::vector<std::uint32_t> parseOrder;
		parseOrder.reserve(ordered.size() + 1);
		parseOrder.push_back(baseID);
		for (const auto& item : ordered) parseOrder.push_back(item.id);

		// Pass 2: parse the base record and every additive record.
		std::size_t parsedRecords = 0;
		for (const auto id : parseOrder) {
			const auto it = raceRecords.find(id);
			if (it == raceRecords.end()) continue;
			const auto& location = it->second.location;
			if (location.pluginIndex >= contexts.size()) continue;

			const auto& context = contexts[location.pluginIndex];
			FileReader reader;
			if (!reader.Open(context.ref.path)) continue;

			std::vector<std::uint8_t> record;
			if (!ReadRecordData(reader, location.dataOffset, location.dataSize, location.flags, record)) continue;

			const std::size_t before = a_out.size();
			ParseSubgraphData(record, context, mods, a_out);
			if (a_out.size() != before) ++parsedRecords;
		}

		if (a_recordCount) *a_recordCount = parsedRecords;
		return !a_out.empty();
	}
}
