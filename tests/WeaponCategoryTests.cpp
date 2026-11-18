#include "../src/WeaponCategory.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	void Require(bool condition, const char* message) {
		if (!condition) {
			std::cerr << "FAIL: " << message << '\n';
			std::exit(1);
		}
	}

	void ExpectKind(std::string_view token, IIF::WeaponCategory::Kind expected) {
		IIF::WeaponCategory::Kind kind{};
		Require(IIF::WeaponCategory::TryParseKind(token, kind), "weapon subtype token is recognised");
		Require(kind == expected, "weapon subtype token maps to the expected class");
	}
}

int main() {
	using IIF::WeaponCategory::Classify;
	using IIF::WeaponCategory::Kind;

	// WEAPON_TYPE 0x00–0x06: 徒手 + 单手/双手冷兵器
	for (int type = 0x00; type <= 0x06; ++type) {
		Require(Classify(static_cast<std::uint8_t>(type)) == Kind::kMelee,
			"all hand-to-hand and cold-weapon animation types are melee");
	}
	Require(Classify(0x09) == Kind::kFirearm, "Gun is a firearm");
	Require(Classify(0x0A) == Kind::kThrown, "Grenade is thrown");
	Require(Classify(0x0B) == Kind::kThrown, "Mine is thrown");

	// Bow/Staff 未在 FO4 中使用，kNone 与越界值不属于三类
	Require(Classify(0x07) == Kind::kUnclassified, "Bow stays outside the three classes");
	Require(Classify(0x08) == Kind::kUnclassified, "Staff stays outside the three classes");
	Require(Classify(0x0C) == Kind::kUnclassified, "out-of-range weapon type is not classified");
	Require(Classify(0xFE) == Kind::kUnclassified, "out-of-range weapon type is not classified");
	Require(Classify(0xFF) == Kind::kUnclassified, "kNone is not classified");

	// 子类名（大小写与分隔符无关）
	ExpectKind("Firearm", Kind::kFirearm);
	ExpectKind("firearms", Kind::kFirearm);
	ExpectKind("GUN", Kind::kFirearm);
	ExpectKind("gun_type", Kind::kFirearm);
	ExpectKind("Fire-arm", Kind::kFirearm);
	ExpectKind("Melee", Kind::kMelee);
	ExpectKind("meleeWeapon", Kind::kMelee);
	ExpectKind("MELEE-TYPE", Kind::kMelee);
	ExpectKind("cold_weapon", Kind::kMelee);
	ExpectKind("Thrown", Kind::kThrown);
	ExpectKind("thrownWeapon", Kind::kThrown);
	ExpectKind("Throwable", Kind::kThrown);
	ExpectKind("Grenade", Kind::kThrown);
	ExpectKind("MINES", Kind::kThrown);

	// 普通条件 ID 不能被当成子类名吞掉
	for (std::string_view token : { "Weapon", "WEAP", "WeaponType", "Armor", "Ammo", "Alchemy",
		 "Pistol", "Fallout4.esm|0x123", "", "  ", "GunNut" }) {
		Kind kind{};
		Require(!IIF::WeaponCategory::TryParseKind(token, kind), "non-subtype token is left to the existing resolver");
	}

	std::cout << "WeaponCategory fixtures passed: engine WEAPON_TYPE grouping and subtype token aliases.\n";
	return 0;
}
