#include "../src/ItemCardData.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(std::is_standard_layout_v<IIF_API::ItemCardValueSnapshot>);
static_assert(std::is_standard_layout_v<IIF_API::ItemCardDataEntry>);
static_assert(std::is_standard_layout_v<IIF_API::ItemCardDataMessage>);
static_assert(std::is_trivially_copyable_v<IIF_API::ItemCardValueSnapshot>);
static_assert(std::is_trivially_copyable_v<IIF_API::ItemCardDataEntry>);
static_assert(std::is_trivially_copyable_v<IIF_API::ItemCardDataMessage>);

namespace {
	void Require(bool condition, const char* message) {
		if (!condition) {
			std::cerr << "FAIL: " << message << '\n';
			std::exit(1);
		}
	}
}

int main() {
	using IIF::ItemCardData::Entry;
	using IIF::ItemCardData::BindEntries;
	using IIF_API::ItemCardValueType;

	std::vector<Entry> source;
	source.reserve(1);

	Entry numeric;
	numeric.id = std::string(96, 'N');
	numeric.label = "Damage";
	numeric.originalValue.type = ItemCardValueType::kNumber;
	numeric.originalValue.number = 12.3;
	numeric.finalValue.type = ItemCardValueType::kNumber;
	numeric.finalValue.number = 14.0;
	numeric.displayType = 0;
	numeric.flags = IIF_API::kItemCardDataFlag_Injected;
	source.push_back(std::move(numeric));

	Entry string;
	string.id = "Stats_APCost";
	string.valueText = "15 AP";
	string.val1 = "12 AP";
	string.val2 = "15 AP";
	string.originalValue.type = ItemCardValueType::kString;
	string.originalValue.text = "12 AP";
	string.finalValue.type = ItemCardValueType::kString;
	string.finalValue.text = "15 AP";
	string.displayType = 2;
	source.push_back(std::move(string));

	Entry emptyString;
	emptyString.id = "Empty";
	emptyString.originalValue.type = ItemCardValueType::kString;
	emptyString.finalValue.type = ItemCardValueType::kString;
	source.push_back(std::move(emptyString));

	Entry missing;
	missing.id = "Missing";
	missing.originalValue.text = "not exposed for kNone";
	source.push_back(std::move(missing));

	auto entries = BindEntries(source);
	Require(entries.size() == source.size(), "all final-layout entries are bound");
	Require(entries[0].id == source[0].id.c_str(), "long ID remains owned by the source entry");
	Require(entries[0].label == source[0].label.c_str(), "label binds to stable source storage");
	Require(entries[0].originalValue.type == ItemCardValueType::kNumber, "numeric original type is preserved");
	Require(entries[0].originalValue.number == 12.3, "pre-modifier numeric value is preserved");
	Require(entries[0].finalValue.number == 14.0, "post-modifier numeric value is preserved");
	Require(entries[0].originalValue.text == nullptr && entries[0].finalValue.text == nullptr,
		"numeric values do not expose string pointers");
	Require(entries[0].displayType == 0 && entries[0].flags == IIF_API::kItemCardDataFlag_Injected,
		"display metadata is preserved");

	Require(entries[1].originalValue.text == source[1].originalValue.text.c_str(),
		"short string pointers are bound only after source vector growth is complete");
	Require(std::string(entries[1].originalValue.text) == "12 AP", "original string value is readable");
	Require(std::string(entries[1].finalValue.text) == "15 AP", "final string value is readable");
	Require(entries[1].originalValue.number == 0.0 && entries[1].finalValue.number == 0.0,
		"string values do not expose stale numeric data");
	Require(std::string(entries[1].valueText) == "15 AP" && std::string(entries[1].val1) == "12 AP" &&
		std::string(entries[1].val2) == "15 AP", "supplementary display strings are preserved");

	Require(entries[2].originalValue.type == ItemCardValueType::kString, "empty string retains string type");
	Require(entries[2].originalValue.text != nullptr && entries[2].originalValue.text[0] == '\0',
		"empty string is distinct from a missing value");
	Require(entries[3].originalValue.type == ItemCardValueType::kNone, "unsupported or absent value becomes kNone");
	Require(entries[3].originalValue.text == nullptr, "kNone never exposes string storage");

	auto movedEntries = std::move(entries);
	Require(movedEntries[1].finalValue.text == source[1].finalValue.text.c_str(),
		"moving the POD array does not change string ownership");

	IIF_API::ItemCardDataMessage message;
	Require(message.version == IIF_API::kItemCardDataVersion, "message defaults to the supported ABI version");
	Require(message.entrySize == sizeof(IIF_API::ItemCardDataEntry), "message describes the entry stride");

	std::cout << "ItemCardData fixtures passed: numeric/string snapshots, empty/missing values, metadata, and pointer ownership.\n";
	return 0;
}
