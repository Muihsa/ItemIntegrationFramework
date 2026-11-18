#include "../src/FallbackRules.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {
	void Require(bool condition, const char* message) {
		if (!condition) {
			std::cerr << "FAIL: " << message << '\n';
			std::exit(1);
		}
	}
}

int main() {
	using IIF::Fallback::Config;

	// Disabled or empty text never applies, even when both triggers are configured.
	Config off;
	off.whenNoData = true;
	off.hasLessOrEqual = true;
	off.lessOrEqual = 0.0;
	Require(!off.Active(), "disabled config is inactive");
	Require(!off.AppliesToMissingData(), "disabled config never fills missing data");
	Require(!off.AppliesToValue(0.0), "disabled config never replaces values");

	Config emptyText;
	emptyText.enabled = true;
	Require(!emptyText.Active(), "enabled config without text is inactive");
	Require(!emptyText.AppliesToMissingData(), "empty text never fills missing data");
	Require(!emptyText.AppliesToValue(-5.0), "empty text never replaces values");

	// Missing-data fallback is on by default once enabled and a value is present.
	Config noData;
	noData.enabled = true;
	noData.value = "--";
	Require(noData.Active(), "enabled config with text is active");
	Require(noData.AppliesToMissingData(), "missing-data fallback defaults to on");
	Require(!noData.AppliesToValue(0.0), "threshold fallback stays off until configured");

	// Explicitly disabling the missing-data trigger keeps the old drop-the-card behavior.
	noData.whenNoData = false;
	Require(!noData.AppliesToMissingData(), "missing-data fallback can be disabled");
	noData.whenNoData = true;

	// Threshold trigger: values equal to or below the threshold are replaced.
	noData.hasLessOrEqual = true;
	noData.lessOrEqual = 0.0;
	Require(noData.AppliesToValue(0.0), "zero triggers the threshold fallback");
	Require(noData.AppliesToValue(-1.0), "negative values trigger the threshold fallback");
	Require(!noData.AppliesToValue(0.5), "values above the threshold keep their own text");

	// A tiny positive threshold catches float dust like 0.0000001.
	noData.lessOrEqual = 0.0000001;
	Require(noData.AppliesToValue(0.0000001), "threshold is inclusive");
	Require(!noData.AppliesToValue(0.0000002), "values above a tiny threshold keep their own text");

	// NaN must never match: NaN comparisons are false anyway, guard against regressions.
	Require(!noData.AppliesToValue(std::numeric_limits<double>::quiet_NaN()), "NaN never triggers the fallback");

	// ValuesMapping cards: draw the fallback card only when the mapping itself is loaded
	// and none of its entries matched the item (the BWS "no maintenance OMOD" case).
	noData.hasLessOrEqual = false;
	Require(noData.AppliesToMappingMiss(false, true), "unmatched mapping with resolved forms uses the fallback");
	Require(!noData.AppliesToMappingMiss(true, true), "a matched mapping keeps its own value");
	Require(!noData.AppliesToMappingMiss(false, false), "a mapping whose forms never resolve stays hidden");
	noData.whenNoData = false;
	Require(!noData.AppliesToMappingMiss(false, true), "mapping fallback honours whenNoData=false");
	noData.whenNoData = true;

	std::cout << "Fallback fixtures passed: enable gating, missing-data trigger, mapping miss, inclusive threshold, NaN guard.\n";
	return 0;
}
