#pragma once
#include <string>

namespace IIF::Fallback
{
	// 信息卡兜底显示：数据源取不到值、字典映射全部未命中，
	// 或取到的数值等于/小于设定阈值时，用固定文本代替数值，而不是直接放弃整张卡。
	struct Config {
		bool enabled{ false };
		std::string value;
		// 数值兜底阈值；hasLessOrEqual == false 表示不按数值触发。
		bool hasLessOrEqual{ false };
		double lessOrEqual{ 0.0 };
		// 没取到数据（缺动画、缺 OMOD、指针无效、ValuesMapping 无命中）时是否用兜底。
		bool whenNoData{ true };

		bool Active() const { return enabled && !value.empty(); }

		bool AppliesToMissingData() const { return Active() && whenNoData; }

		// ValuesMapping 规则的兜底：映射全部未命中时是否仍然出卡。
		// a_anyMappingResolved == false 表示映射里的 FormID 一个都没解析出来
		// （插件没装/FormID 写错），此时不出兜底卡，避免刷满整个物品列表。
		bool AppliesToMappingMiss(bool a_mappingMatched, bool a_anyMappingResolved) const {
			return !a_mappingMatched && a_anyMappingResolved && AppliesToMissingData();
		}

		bool AppliesToValue(double a_value) const {
			if (!Active() || !hasLessOrEqual) return false;
			if (a_value != a_value) return false;  // NaN
			return a_value <= lessOrEqual;
		}
	};
}
