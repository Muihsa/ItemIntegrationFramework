#pragma once
#include "IIF_ItemCardDataAPI.h"

#include <string>
#include <vector>

namespace IIF::ItemCardData
{
	struct ValueSnapshot {
		IIF_API::ItemCardValueType type{ IIF_API::ItemCardValueType::kNone };
		double number{ 0.0 };
		std::string text;
	};

	struct Entry {
		std::string id;
		std::string label;
		std::string valueText;
		std::string val1;
		std::string val2;
		ValueSnapshot originalValue;
		ValueSnapshot finalValue;
		std::int32_t displayType{ 0 };
		std::uint32_t flags{ 0 };
	};

	inline IIF_API::ItemCardValueSnapshot BindValue(const ValueSnapshot& a_value) {
		IIF_API::ItemCardValueSnapshot result;
		result.type = a_value.type;
		result.number = a_value.number;
		if (a_value.type == IIF_API::ItemCardValueType::kString) result.text = a_value.text.c_str();
		return result;
	}

	inline std::vector<IIF_API::ItemCardDataEntry> BindEntries(const std::vector<Entry>& a_entries) {
		std::vector<IIF_API::ItemCardDataEntry> result;
		result.reserve(a_entries.size());
		for (const auto& entry : a_entries) {
			IIF_API::ItemCardDataEntry bound;
			bound.id = entry.id.c_str();
			bound.label = entry.label.c_str();
			bound.valueText = entry.valueText.c_str();
			bound.val1 = entry.val1.c_str();
			bound.val2 = entry.val2.c_str();
			bound.originalValue = BindValue(entry.originalValue);
			bound.finalValue = BindValue(entry.finalValue);
			bound.displayType = entry.displayType;
			bound.flags = entry.flags;
			result.push_back(bound);
		}
		return result;
	}
}
