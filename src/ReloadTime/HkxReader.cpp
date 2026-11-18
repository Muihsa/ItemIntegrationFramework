#include "pch.h"
#include "ReloadTime/HkxReader.h"

#include "ReloadTime/Inflate.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Havok packfile reader for two questions only:
//   1. how long is the animation clip in this .hkx?
//   2. does this behavior graph assign a clip generator for <leaf>?
// Everything is read through BSResource so BA2 archives and mod overrides are
// honoured, and answers are cached per session.
//
// Resource access notes:
//   - Reads go through a stream opened with the full-read hint (the same shape
//     OAR uses for multi-megabyte animations); the plain one-argument stream
//     intermittently returns nothing for larger archive entries.
//   - NiBinaryStream::GetBufferInfo().fileSize is treated as a hint only.
//   - A payload that arrives still zlib-compressed is inflated before parsing.
//   - Loose files are also readable from <Data>\<path> as a last resort.

namespace IIF::ReloadTime
{
	namespace
	{
		constexpr std::uint32_t kPackfileMagic0 = 0x57E0E057;
		constexpr std::uint32_t kPackfileMagic1 = 0x10C0C010;
		constexpr std::uint32_t kPackfileVersion11 = 0x0B;
		constexpr std::uint32_t kTagfileMagic = 0xCAB00D1E;
		constexpr std::size_t kMaxResourceSize = 50u * 1024u * 1024u;
		constexpr std::size_t kMaxCompressedAnimation = 8u * 1024u * 1024u;
		constexpr std::size_t kMaxInflatedAnimation = 32u * 1024u * 1024u;
		constexpr std::size_t kMaxBehaviorSize = 16u * 1024u * 1024u;
		constexpr std::size_t kReadChunk = 256u * 1024u;
		constexpr int kMaxBehaviorDepth = 4;

		std::uint16_t Read16(const std::uint8_t* a_p) { return static_cast<std::uint16_t>(a_p[0] | (a_p[1] << 8)); }
		std::uint32_t Read32(const std::uint8_t* a_p)
		{
			return static_cast<std::uint32_t>(a_p[0]) |
			       (static_cast<std::uint32_t>(a_p[1]) << 8) |
			       (static_cast<std::uint32_t>(a_p[2]) << 16) |
			       (static_cast<std::uint32_t>(a_p[3]) << 24);
		}
		std::int32_t ReadI32(const std::uint8_t* a_p) { return static_cast<std::int32_t>(Read32(a_p)); }
		float ReadF32(const std::uint8_t* a_p)
		{
			const std::uint32_t bits = Read32(a_p);
			float value = 0.0f;
			std::memcpy(&value, &bits, sizeof(value));
			return value;
		}

		std::string ToLower(std::string a_value)
		{
			std::transform(a_value.begin(), a_value.end(), a_value.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_value;
		}

		bool StartsWithNoCase(std::string_view a_value, std::string_view a_prefix)
		{
			if (a_value.size() < a_prefix.size()) return false;
			for (std::size_t i = 0; i < a_prefix.size(); ++i) {
				if (std::tolower(static_cast<unsigned char>(a_value[i])) != std::tolower(static_cast<unsigned char>(a_prefix[i]))) return false;
			}
			return true;
		}

		bool EndsWithNoCase(std::string_view a_value, std::string_view a_suffix)
		{
			if (a_value.size() < a_suffix.size()) return false;
			return StartsWithNoCase(a_value.substr(a_value.size() - a_suffix.size()), a_suffix);
		}

		bool LooksLikeZlib(const std::uint8_t* a_data, std::size_t a_size)
		{
			if (!a_data || a_size < 6) return false;
			if ((a_data[0] & 0x0F) != 8) return false;
			if (((static_cast<std::uint32_t>(a_data[0]) << 8) | a_data[1]) % 31 != 0) return false;
			return (a_data[1] & 0x20) == 0;
		}

		std::string HexHead(const std::uint8_t* a_data, std::size_t a_size, std::size_t a_count)
		{
			static constexpr char kDigits[] = "0123456789abcdef";
			std::string text;
			const std::size_t count = (std::min)(a_size, a_count);
			text.reserve(count * 2);
			for (std::size_t i = 0; i < count; ++i) {
				text.push_back(kDigits[a_data[i] >> 4]);
				text.push_back(kDigits[a_data[i] & 0x0F]);
			}
			return text;
		}

		// Card evaluation can be reached from more than one thread, and every
		// cache below is a plain container: serialise the reader with a recursive
		// mutex (behavior resolution re-enters ResourceExists).
		std::recursive_mutex& ReaderMutex()
		{
			static std::recursive_mutex mutex;
			return mutex;
		}

		// Bounded diagnostics: the first few distinct problems are logged so a
		// single in-game item inspection is enough to pinpoint a broken stage.
		void LogResourceProblem(std::string_view a_reason, const std::string& a_detail)
		{
			static std::unordered_set<std::string> seen;
			if (seen.size() >= 12) return;
			if (!seen.insert(std::string(a_reason) + "|" + a_detail).second) return;
			REX::WARN("[IIF] Reload time: {} ('{}')", a_reason, a_detail);
		}

		std::size_t ReadFully(RE::NiBinaryStream& a_stream, void* a_buf, std::size_t a_size)
		{
			auto* bytes = static_cast<std::uint8_t*>(a_buf);
			std::size_t total = 0;
			while (total < a_size) {
				const std::size_t got = a_stream.binary_read(bytes + total, a_size - total);
				if (got == 0) break;
				total += got;
			}
			return total;
		}

		// === BSResource access =================================================

		// The four-argument constructor (full-read hint) is private in
		// CommonLibF4RD, so it is invoked through the same engine entry points the
		// library itself uses. This is the shape OAR reads multi-megabyte archive
		// animations with; the plain constructor can return nothing for some
		// entries.
		class EngineStream final
		{
		public:
			EngineStream() = default;
			~EngineStream() { Close(); }

			EngineStream(const EngineStream&) = delete;
			EngineStream& operator=(const EngineStream&) = delete;

			[[nodiscard]] bool Open(const char* a_path)
			{
				using Ctor_t = void (*)(void*, const char*, bool, void*, bool);
				static REL::Relocation<Ctor_t> ctor{ REL::ID{ 1198116, 2269830 } };
				ctor(storage, a_path, false, nullptr, true);
				constructed = true;
				return good();
			}

			void Close()
			{
				if (!constructed) return;
				constructed = false;
				using Dtor_t = void (*)(void*);
				static REL::Relocation<Dtor_t> dtor{ REL::ID{ 1516202, 2269832 } };
				dtor(storage);
			}

			[[nodiscard]] bool good() const
			{
				return constructed && static_cast<bool>(*reinterpret_cast<const RE::BSResourceNiBinaryStream*>(storage));
			}

			[[nodiscard]] RE::BSResourceNiBinaryStream& Get()
			{
				return *reinterpret_cast<RE::BSResourceNiBinaryStream*>(storage);
			}

		private:
			alignas(8) std::uint8_t storage[0x30]{};
			bool constructed{ false };
		};

		bool ProbeResourceUnsafe(const char* a_path)
		{
			RE::BSResourceNiBinaryStream stream(a_path);
			return static_cast<bool>(stream);
		}

		// Reads up to a_size bytes starting at a_offset. A shorter a_out means the
		// resource ended; the return value is false only when the stream could not
		// be opened or the skip failed.
		bool ReadResourceRangeUnsafe(const char* a_path, std::uint64_t a_offset, std::size_t a_size,
			std::size_t& a_bytesRead, std::size_t& a_fileSize, std::vector<std::uint8_t>& a_out)
		{
			a_out.clear();
			a_bytesRead = 0;
			a_fileSize = 0;
			if (a_offset > kMaxResourceSize || a_size > kMaxResourceSize) return false;

			EngineStream stream;
			if (!stream.Open(a_path)) return false;

			RE::NiBinaryStream::BufferInfo info{};
			stream.Get().GetBufferInfo(info);
			a_fileSize = info.fileSize;  // diagnostic only; may be 0

			std::uint8_t scratch[1024];
			std::uint64_t remaining = a_offset;
			while (remaining > 0) {
				const std::size_t chunk = static_cast<std::size_t>((std::min)(remaining, static_cast<std::uint64_t>(sizeof(scratch))));
				if (ReadFully(stream.Get(), scratch, chunk) != chunk) return false;
				remaining -= chunk;
			}

			a_out.resize(a_size);
			a_bytesRead = ReadFully(stream.Get(), a_out.data(), a_out.size());
			a_out.resize(a_bytesRead);
			return true;
		}

		bool ReadWholeResourceUnsafe(const char* a_path, std::size_t a_cap, std::vector<std::uint8_t>& a_out)
		{
			a_out.clear();
			EngineStream stream;
			if (!stream.Open(a_path)) return false;

			std::vector<std::uint8_t> chunk(kReadChunk);
			for (;;) {
				const std::size_t got = ReadFully(stream.Get(), chunk.data(), chunk.size());
				if (got > 0) {
					if (a_out.size() + got > a_cap) return false;
					a_out.insert(a_out.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(got));
				}
				if (got < chunk.size()) return !a_out.empty();
			}
		}

		bool SafeProbeResource(const std::string& a_path)
		{
			__try {
				return ProbeResourceUnsafe(a_path.c_str());
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				return false;
			}
		}

		bool SafeReadResourceRange(const std::string& a_path, std::uint64_t a_offset, std::size_t a_size,
			std::size_t& a_bytesRead, std::size_t& a_fileSize, std::vector<std::uint8_t>& a_out)
		{
			__try {
				return ReadResourceRangeUnsafe(a_path.c_str(), a_offset, a_size, a_bytesRead, a_fileSize, a_out);
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				a_out.clear();
				a_bytesRead = 0;
				return false;
			}
		}

		bool SafeReadWholeResource(const std::string& a_path, std::size_t a_cap, std::vector<std::uint8_t>& a_out)
		{
			__try {
				return ReadWholeResourceUnsafe(a_path.c_str(), a_cap, a_out);
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				a_out.clear();
				return false;
			}
		}

		// Loose files are visible under <Data>\ even with a mod manager's virtual
		// file system, so this covers anything the resource system cannot serve.
		bool ReadFileFromDisk(const std::string& a_resourcePath, std::size_t a_cap, std::vector<std::uint8_t>& a_out)
		{
			std::vector<std::filesystem::path> candidates;
			candidates.emplace_back(std::filesystem::path("Data") / a_resourcePath);
			wchar_t exePath[MAX_PATH]{};
			if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
				candidates.emplace_back(std::filesystem::path(exePath).parent_path() / "Data" / a_resourcePath);
			}

			for (const auto& candidate : candidates) {
				std::ifstream file(candidate, std::ios::binary | std::ios::ate);
				if (!file.is_open()) continue;

				const auto size = file.tellg();
				if (size <= 0 || static_cast<std::uint64_t>(size) > a_cap) continue;

				file.seekg(0, std::ios::beg);
				a_out.resize(static_cast<std::size_t>(size));
				file.read(reinterpret_cast<char*>(a_out.data()), static_cast<std::streamsize>(size));
				if (file.gcount() == static_cast<std::streamsize>(size)) return true;
				a_out.clear();
			}
			return false;
		}

		bool FileExistsOnDisk(const std::string& a_resourcePath)
		{
			std::error_code ec;
			if (std::filesystem::exists(std::filesystem::path("Data") / a_resourcePath, ec)) return true;

			ec.clear();
			wchar_t exePath[MAX_PATH]{};
			if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
				return std::filesystem::exists(std::filesystem::path(exePath).parent_path() / "Data" / a_resourcePath, ec);
			}
			return false;
		}

		bool IsPackfileHead(const std::vector<std::uint8_t>& a_data)
		{
			return a_data.size() >= 8 && Read32(a_data.data()) == kPackfileMagic0 && Read32(a_data.data() + 4) == kPackfileMagic1;
		}

		// Accepts a raw packfile or a zlib-wrapped one (some mod archives hand the
		// stored entry back instead of the decompressed file).
		bool DecodeFileImage(std::vector<std::uint8_t>&& a_data, std::vector<std::uint8_t>& a_out)
		{
			if (IsPackfileHead(a_data)) {
				a_out = std::move(a_data);
				return true;
			}
			if (LooksLikeZlib(a_data.data(), a_data.size())) {
				std::vector<std::uint8_t> inflated;
				if (ZlibDecompressCapped(a_data.data(), a_data.size(), kMaxInflatedAnimation, inflated) && IsPackfileHead(inflated)) {
					a_out = std::move(inflated);
					return true;
				}
			}
			return false;
		}

		// Returns a usable packfile image: resource first, then loose disk.
		bool ReadWholeFile(const std::string& a_path, std::size_t a_cap, std::vector<std::uint8_t>& a_out)
		{
			std::vector<std::uint8_t> raw;
			const bool hasResource = SafeReadWholeResource(a_path, a_cap, raw) && !raw.empty();
			if (hasResource && DecodeFileImage(std::move(raw), a_out)) return true;

			std::vector<std::uint8_t> disk;
			const bool hasDisk = ReadFileFromDisk(a_path, a_cap, disk) && !disk.empty();
			if (hasDisk && DecodeFileImage(std::move(disk), a_out)) return true;

			if (hasResource) {
				LogResourceProblem("resource payload unusable", a_path);
			}
			else if (hasDisk) {
				LogResourceProblem("disk payload unusable", a_path);
			}
			return false;
		}

		// === caches ============================================================

		std::unordered_map<std::string, bool>& ExistenceCache()
		{
			static std::unordered_map<std::string, bool> cache;
			return cache;
		}

		std::unordered_map<std::string, std::optional<float>>& DurationCache()
		{
			static std::unordered_map<std::string, std::optional<float>> cache;
			return cache;
		}

		struct BehaviorInfo
		{
			std::unordered_set<std::string> leaves;
			std::vector<std::string> references;
			bool loaded{ false };
		};

		std::unordered_map<std::string, BehaviorInfo>& BehaviorCache()
		{
			static std::unordered_map<std::string, BehaviorInfo> cache;
			return cache;
		}

		std::unordered_map<std::string, std::string>& ReferenceCache()
		{
			static std::unordered_map<std::string, std::string> cache;
			return cache;
		}

		// === behavior graph scanning ==========================================

		std::string DirectoryOf(const std::string& a_path)
		{
			const auto slash = a_path.find_last_of("\\/");
			return slash == std::string::npos ? std::string{} : a_path.substr(0, slash);
		}

		std::string FileNameOf(const std::string& a_path)
		{
			const auto slash = a_path.find_last_of("\\/");
			return slash == std::string::npos ? a_path : a_path.substr(slash + 1);
		}

		void ProcessBehaviorString(const std::string& a_text, BehaviorInfo& a_info)
		{
			std::string text = a_text;
			std::replace(text.begin(), text.end(), '/', '\\');

			const bool isClip = EndsWithNoCase(text, ".hkt") || EndsWithNoCase(text, ".hkx");
			if (!isClip) return;

			const std::string file = FileNameOf(text);
			const auto dot = file.find_last_of('.');
			if (dot == std::string::npos || dot == 0) return;
			a_info.leaves.insert(ToLower(file.substr(0, dot)));

			// Behavior graphs referenced by hkbBehaviorReferenceGenerator. Clip
			// names can also end in .hkx, so require a Behaviors directory or a
			// "Behavior" file name to avoid loading multi-megabyte animations.
			if (EndsWithNoCase(text, ".hkx") &&
				(text.find("Behaviors\\") != std::string::npos || text.find("behaviors\\") != std::string::npos ||
					ToLower(file).find("behavior") != std::string::npos)) {
				a_info.references.push_back(text);
			}
		}

		void ParseBehaviorStrings(const std::vector<std::uint8_t>& a_data, BehaviorInfo& a_info)
		{
			std::string run;
			run.reserve(128);
			for (std::size_t i = 0; i <= a_data.size(); ++i) {
				const std::uint8_t c = i < a_data.size() ? a_data[i] : 0;
				if (c >= 32 && c < 127) {
					run.push_back(static_cast<char>(c));
					continue;
				}
				if (run.size() >= 4) ProcessBehaviorString(run, a_info);
				run.clear();
			}
		}

		const BehaviorInfo* GetBehaviorInfo(const std::string& a_resourcePath)
		{
			const std::string key = ToLower(a_resourcePath);
			auto& cache = BehaviorCache();
			if (const auto it = cache.find(key); it != cache.end()) return it->second.loaded ? &it->second : nullptr;

			BehaviorInfo info;
			std::vector<std::uint8_t> data;
			if (ReadWholeFile(a_resourcePath, kMaxBehaviorSize, data) && !data.empty()) {
				ParseBehaviorStrings(data, info);
				info.loaded = true;
			}
			else {
				LogResourceProblem("behavior unreadable", a_resourcePath);
			}

			auto& inserted = cache[key];
			inserted = std::move(info);
			return inserted.loaded ? &inserted : nullptr;
		}

		std::string ResolveBehaviorReference(const std::string& a_fromPath, const std::string& a_reference)
		{
			const std::string key = ToLower(a_fromPath) + "|" + ToLower(a_reference);
			auto& cache = ReferenceCache();
			if (const auto it = cache.find(key); it != cache.end()) return it->second;

			std::string reference = a_reference;
			std::replace(reference.begin(), reference.end(), '/', '\\');
			if (StartsWithNoCase(reference, "Meshes\\")) reference.erase(0, 7);

			std::vector<std::string> candidates;
			if (StartsWithNoCase(reference, "Actors\\")) {
				candidates.push_back(reference);
			}
			else {
				const std::string directory = DirectoryOf(a_fromPath);
				const std::string basename = FileNameOf(reference);

				// Siblings of the referring behavior ("Actors\...\_1stPerson\Behaviors").
				const std::string parent = DirectoryOf(directory);
				if (!parent.empty() && !StartsWithNoCase(parent, "Meshes\\")) candidates.push_back(parent + "\\" + reference);
				if (!directory.empty()) {
					candidates.push_back(directory + "\\" + reference);
					candidates.push_back(directory + "\\" + basename);
				}
				if (StartsWithNoCase(a_fromPath, "Meshes\\Actors\\Character\\_1stPerson\\")) {
					candidates.push_back("Actors\\Character\\_1stPerson\\" + reference);
				}
				candidates.push_back(reference);
			}

			std::string resolved;
			for (const auto& candidate : candidates) {
				const std::string normalized = NormalizeResourcePath(candidate);
				if (ResourceExists(normalized)) {
					resolved = normalized;
					break;
				}
			}

			cache[key] = resolved;
			return resolved;
		}

		bool BehaviorDefinesClipImpl(const std::string& a_resourcePath, const std::string& a_leaf, int a_depth, std::unordered_set<std::string>& a_visited)
		{
			if (a_depth > kMaxBehaviorDepth) return false;

			const std::string key = ToLower(a_resourcePath);
			if (!a_visited.insert(key).second) return false;

			const BehaviorInfo* info = GetBehaviorInfo(a_resourcePath);
			if (!info) return false;

			if (info->leaves.contains(a_leaf)) return true;

			for (const auto& reference : info->references) {
				const std::string resolved = ResolveBehaviorReference(a_resourcePath, reference);
				if (!resolved.empty() && BehaviorDefinesClipImpl(resolved, a_leaf, a_depth + 1, a_visited)) return true;
			}
			return false;
		}

		// === animation duration ===============================================

		struct HkxSection
		{
			std::uint32_t absoluteDataStart{ 0 };
			std::uint32_t localFixupsOffset{ 0 };
			std::uint32_t endOffset{ 0 };
		};

		struct HkxLayout
		{
			std::uint64_t payloadStart{ 0 };
			std::uint64_t payloadSize{ 0 };
			std::uint32_t contentsOffset{ 0 };
			bool payloadSizeKnown{ false };
			bool valid{ false };
		};

		HkxLayout ParseHkxLayout(const std::uint8_t* a_data, std::size_t a_size)
		{
			HkxLayout layout;
			if (!a_data || a_size < 0x40) return layout;
			if (Read32(a_data) != kPackfileMagic0 || Read32(a_data + 4) != kPackfileMagic1) return layout;

			const std::uint32_t fileVersion = Read32(a_data + 0x0C);
			const std::uint32_t numSections = Read32(a_data + 0x14);
			const std::uint32_t contentsIndex = Read32(a_data + 0x18);
			const std::uint16_t predicatePadding = Read16(a_data + 0x3E);

			if (numSections < 1 || numSections > 10) return layout;

			const bool version11 = fileVersion >= kPackfileVersion11;
			const std::size_t sectionHeaderSize = version11 ? 0x40 : 0x30;
			const std::size_t sectionHeadersStart = 0x40 + predicatePadding;
			if (sectionHeadersStart + static_cast<std::size_t>(numSections) * sectionHeaderSize > a_size) return layout;

			HkxSection sections[10]{};
			for (std::uint32_t i = 0; i < numSections; ++i) {
				const std::uint8_t* sectionHeader = a_data + sectionHeadersStart + i * sectionHeaderSize;
				sections[i].absoluteDataStart = Read32(sectionHeader + 0x14);
				sections[i].localFixupsOffset = Read32(sectionHeader + 0x18);
				sections[i].endOffset = Read32(sectionHeader + 0x2C);
			}

			const std::uint32_t sectionIndex = contentsIndex < numSections ? contentsIndex : numSections - 1;
			const HkxSection& contents = sections[sectionIndex];

			std::uint64_t payloadStart = contents.absoluteDataStart;
			if (payloadStart == 0 || payloadStart >= kMaxResourceSize) {
				payloadStart = (sectionHeadersStart + static_cast<std::size_t>(numSections) * sectionHeaderSize + 0xF) & ~static_cast<std::size_t>(0xF);
			}

			std::uint64_t payloadSize = contents.localFixupsOffset;
			if (payloadSize == 0 || payloadSize == 0xFFFFFFFFu) payloadSize = contents.endOffset;
			const bool known = payloadSize != 0 && payloadSize != 0xFFFFFFFFu && payloadSize <= kMaxResourceSize;

			layout.payloadStart = payloadStart;
			layout.payloadSize = known ? payloadSize : kMaxResourceSize;
			layout.contentsOffset = Read32(a_data + 0x1C);
			layout.payloadSizeKnown = known;
			layout.valid = true;
			return layout;
		}

		bool TryAnimationAt(const std::uint8_t* a_payload, std::size_t a_size, std::size_t a_offset, float& a_duration)
		{
			if (a_offset + 0x20 > a_size) return false;

			const std::uint8_t* object = a_payload + a_offset;
			const std::int32_t type = ReadI32(object + 0x10);
			const float duration = ReadF32(object + 0x14);
			const std::int32_t tracks = ReadI32(object + 0x18);
			const std::int32_t floats = ReadI32(object + 0x1C);

			if (type < 1 || type > 10) return false;
			if (!(duration > 0.001f && duration < 600.0f)) return false;
			if (tracks < 1 || tracks > 500) return false;
			if (floats < 0 || floats > 200) return false;

			a_duration = duration;
			return true;
		}

		bool FindAnimationDuration(const std::uint8_t* a_payload, std::size_t a_size, std::uint32_t a_contentsOffset, float& a_duration)
		{
			if (a_contentsOffset != 0 && TryAnimationAt(a_payload, a_size, a_contentsOffset, a_duration)) return true;
			for (std::size_t offset = 0; offset + 0x20 <= a_size; offset += 8) {
				if (TryAnimationAt(a_payload, a_size, offset, a_duration)) return true;
			}
			return false;
		}

		std::optional<float> ParsePackfileBuffer(const std::vector<std::uint8_t>& a_file)
		{
			const HkxLayout layout = ParseHkxLayout(a_file.data(), a_file.size());
			if (!layout.valid) return std::nullopt;
			if (layout.payloadStart >= a_file.size()) return std::nullopt;

			const std::size_t available = static_cast<std::size_t>(a_file.size() - layout.payloadStart);
			const std::size_t payloadSize = static_cast<std::size_t>((std::min)(static_cast<std::uint64_t>(available), layout.payloadSize));
			if (payloadSize < 0x20) return std::nullopt;

			float duration = 0.0f;
			if (FindAnimationDuration(a_file.data() + layout.payloadStart, payloadSize, layout.contentsOffset, duration)) return duration;
			return std::nullopt;
		}

		std::optional<float> ReadAnimationDurationUncached(const std::string& a_resourcePath)
		{
			std::vector<std::uint8_t> prefix;
			std::size_t readBytes = 0;
			std::size_t reportedSize = 0;
			const bool opened = SafeReadResourceRange(a_resourcePath, 0, 0x800, readBytes, reportedSize, prefix);

			// Fast path: the animation object sits near the start of the data
			// section, so a small prefix is normally enough.
			if (opened && prefix.size() >= 0x40) {
				const HkxLayout layout = ParseHkxLayout(prefix.data(), prefix.size());
				if (layout.valid) {
					constexpr std::size_t kProbeSizes[] = { 0x20000, 0x100000, 0x400000, 0x800000 };
					std::size_t requested = 0;
					for (const std::size_t probe : kProbeSizes) {
						const std::size_t wanted = static_cast<std::size_t>((std::min)(static_cast<std::uint64_t>(probe), layout.payloadSize));
						if (wanted <= requested) continue;
						requested = wanted;

						std::vector<std::uint8_t> payload;
						std::size_t payloadRead = 0;
						std::size_t ignoredSize = 0;
						if (!SafeReadResourceRange(a_resourcePath, layout.payloadStart, wanted, payloadRead, ignoredSize, payload)) break;
						if (payload.size() < 0x20) break;

						float duration = 0.0f;
						if (FindAnimationDuration(payload.data(), payload.size(), layout.contentsOffset, duration)) return duration;

						if (payload.size() < wanted) break;  // reached the end of the resource
					}
					LogResourceProblem("hkx has no hkaAnimation", a_resourcePath);
					return std::nullopt;
				}
			}

			// Everything else: pull the whole file (resource, loose disk, or a
			// zlib-wrapped archive entry) and parse it in memory.
			std::vector<std::uint8_t> file;
			if (ReadWholeFile(a_resourcePath, kMaxCompressedAnimation, file) && IsPackfileHead(file)) {
				if (const auto duration = ParsePackfileBuffer(file)) return duration;
				LogResourceProblem("hkx has no hkaAnimation (whole file)", a_resourcePath);
				return std::nullopt;
			}

			if (prefix.size() >= 8) {
				if (Read32(prefix.data()) == kTagfileMagic) {
					LogResourceProblem("hkx is a tagfile (unsupported)", a_resourcePath);
					return std::nullopt;
				}
				LogResourceProblem("hkx unreadable", a_resourcePath + " (head " + HexHead(prefix.data(), prefix.size(), 8) +
				                                                    ", read=" + std::to_string(readBytes) +
				                                                    ", resourceSize=" + std::to_string(reportedSize) + ")");
			}
			else {
				LogResourceProblem("hkx unreadable", a_resourcePath + " (read=" + std::to_string(readBytes) +
				                                                    ", resourceSize=" + std::to_string(reportedSize) + ")");
			}
			return std::nullopt;
		}
	}

	std::string NormalizeResourcePath(std::string a_path)
	{
		std::replace(a_path.begin(), a_path.end(), '/', '\\');
		while (!a_path.empty() && a_path.front() == '\\') a_path.erase(a_path.begin());

		if (StartsWithNoCase(a_path, "Data\\")) a_path.erase(0, 5);
		if (!StartsWithNoCase(a_path, "Meshes\\") && !a_path.empty()) a_path.insert(0, "Meshes\\");
		return a_path;
	}

	bool ResourceExists(const std::string& a_resourcePath)
	{
		if (a_resourcePath.empty()) return false;

		const std::lock_guard lock(ReaderMutex());

		const std::string normalized = NormalizeResourcePath(a_resourcePath);
		const std::string key = ToLower(normalized);

		auto& cache = ExistenceCache();
		if (const auto it = cache.find(key); it != cache.end()) return it->second;

		bool exists = SafeProbeResource(normalized);
		if (!exists) exists = FileExistsOnDisk(normalized);

		cache[key] = exists;
		return exists;
	}

	std::optional<float> ReadAnimationDuration(const std::string& a_resourcePath)
	{
		if (a_resourcePath.empty()) return std::nullopt;

		const std::lock_guard lock(ReaderMutex());

		const std::string normalized = NormalizeResourcePath(a_resourcePath);
		const std::string key = ToLower(normalized);

		auto& cache = DurationCache();
		if (const auto it = cache.find(key); it != cache.end()) return it->second;

		const auto duration = ReadAnimationDurationUncached(normalized);
		cache[key] = duration;
		return duration;
	}

	bool BehaviorDefinesClip(const std::string& a_behaviorPath, std::string_view a_leaf)
	{
		if (a_behaviorPath.empty() || a_leaf.empty()) return false;

		const std::lock_guard lock(ReaderMutex());

		const std::string normalized = NormalizeResourcePath(a_behaviorPath);
		const std::string leaf = ToLower(std::string(a_leaf));

		std::unordered_set<std::string> visited;
		return BehaviorDefinesClipImpl(normalized, leaf, 0, visited);
	}
}
