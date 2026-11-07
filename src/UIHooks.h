#pragma once
#include "pch.h"

namespace IIF::Hooks
{
	struct InternalCard {
		std::string text; std::string label; bool highlightLabel; std::string value;
		bool showDifference; bool invertDiffColor;
		bool hasDifference; float difference;
		bool hasRawValue; double rawValue; std::string suffix;
		int priority; int displayType; bool hasBackground; std::uint32_t backgroundColor;
		std::vector<std::string> anchorTargets; std::vector<std::string> anchorModes; std::string sortValueFrom;
		float fillPct{ -1.0f }; float shieldPct{ 0.0f }; std::uint32_t fillColor{ 0 };
		float thresholdPct{ -1.0f }; float thresholdPct2{ -1.0f }; std::uint32_t thresholdColor{ 0 };
		bool showBar; bool showValue; std::string valueText; std::string valueAlign; std::uint32_t valueColor;
		bool valueStandard; bool valueBad; bool valueGood;
		std::string icon1; bool icon1IsText; std::string val1; bool val1Bad; bool val1Good; bool val1Standard; std::string align1;
		std::string icon2; bool icon2IsText; std::string val2; bool val2Bad; bool val2Good; bool val2Standard; std::string align2;
		// 只广播、不插入信息卡（JSON 规则的 broadcastOnly）。
		bool broadcastOnly{ false };
	};

	// 由 JsonReader::EvaluateItemRules 在同一次刷新内填好，RebuildUIArray 负责把它写进原版伤害卡。
	struct InlineDamageRequest {
		bool active{ false };
		float projectiles{ 0.0f };
		std::string format{ "{damage} x {count}" };
		int roundDigits{ 0 };
	};

	extern InlineDamageRequest g_inlineDamage;
	extern std::vector<InternalCard> g_pendingCards;

	void Install();
}
