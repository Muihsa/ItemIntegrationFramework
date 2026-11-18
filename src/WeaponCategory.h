#pragma once

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

// 武器子类判定：唯一依据是引擎自己的武器动画类型字段
// （WEAP DNAM + 0x36，运行时 TESObjectWEAP::weaponData.type，WEAPON_TYPE）。
// Ammo、Capacity、FNAM、MASE、关键词、射程都只是伴随特征，存在反例，不能参与判定。
// 本头文件不依赖 CommonLibF4，便于独立编译测试。
namespace IIF::WeaponCategory
{
	// RE/W/WEAPON_TYPE.h（载入时 0x0C–0xFE 会被钳制为 0x00，0xFF 保留）
	enum Type : std::uint8_t
	{
		kHandToHand = 0x0,
		kOneHandSword = 0x1,
		kOneHandDagger = 0x2,
		kOneHandAxe = 0x3,
		kOneHandMace = 0x4,
		kTwoHandSword = 0x5,
		kTwoHandAxe = 0x6,
		kBow = 0x7,
		kStaff = 0x8,
		kGun = 0x9,
		kGrenade = 0xA,
		kMine = 0xB,
		kNone = 0xFF
	};

	enum class Kind
	{
		kUnclassified,
		kFirearm,
		kMelee,
		kThrown
	};

	// 0x00–0x06（徒手 + 单手/双手剑、匕首、斧、锤）为近战，0x09 为火器，
	// 0x0A/0x0B（手雷/地雷）为投掷物。Bow/Staff 与 kNone 不归入三类。
	[[nodiscard]] inline Kind Classify(std::uint8_t a_type) noexcept
	{
		switch (a_type) {
		case kHandToHand:
		case kOneHandSword:
		case kOneHandDagger:
		case kOneHandAxe:
		case kOneHandMace:
		case kTwoHandSword:
		case kTwoHandAxe:
			return Kind::kMelee;
		case kGun:
			return Kind::kFirearm;
		case kGrenade:
		case kMine:
			return Kind::kThrown;
		default:
			return Kind::kUnclassified;
		}
	}

	// 忽略大小写与分隔符，使 "Fire-arm"、"MELEE"、"thrown_weapon" 都能命中。
	[[nodiscard]] inline std::string NormalizeToken(std::string_view a_token)
	{
		std::string out;
		out.reserve(a_token.size());
		for (const char c : a_token) {
			const auto uc = static_cast<unsigned char>(c);
			if (std::isalnum(uc)) out.push_back(static_cast<char>(std::tolower(uc)));
		}
		return out;
	}

	// 把条件 ID 解析成武器子类。返回 false 表示它不是子类名（例如 FormID、关键词），
	// 调用方应继续按原有逻辑解析。
	[[nodiscard]] inline bool TryParseKind(std::string_view a_token, Kind& a_kind) noexcept
	{
		const std::string token = NormalizeToken(a_token);
		if (token == "firearm" || token == "firearms" || token == "gun" || token == "guns" || token == "guntype") {
			a_kind = Kind::kFirearm;
			return true;
		}
		if (token == "melee" || token == "meleeweapon" || token == "meleetype" || token == "coldweapon") {
			a_kind = Kind::kMelee;
			return true;
		}
		if (token == "thrown" || token == "thrownweapon" || token == "throwable" ||
			token == "grenade" || token == "grenades" || token == "mine" || token == "mines") {
			a_kind = Kind::kThrown;
			return true;
		}
		return false;
	}
}
