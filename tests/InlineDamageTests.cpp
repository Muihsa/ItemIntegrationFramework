#include "../src/InlineDamageFormat.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
	void Require(bool condition, const char* message) {
		if (!condition) {
			std::cerr << "FAIL: " << message << '\n';
			std::exit(1);
		}
	}

	void RequireText(const std::string& actual, const char* expected, const char* message) {
		if (actual != expected) {
			std::cerr << "FAIL: " << message << " (expected '" << expected << "', got '" << actual << "')\n";
			std::exit(1);
		}
	}
}

int main() {
	using IIF::InlineDamage::Build;
	using IIF::InlineDamage::FormatNumber;

	// Requirement: total 100 damage over 8 projectiles must render as "13 x 8".
	RequireText(Build("{damage} x {count}", 100.0, 8.0, 0), "13 x 8", "default shotgun format rounds 12.5 up");

	// One projectile: the formatter still describes the value; callers skip the rewrite.
	RequireText(Build("{damage} x {count}", 100.0, 1.0, 0), "100 x 1", "single projectile formats without rounding surprises");

	// Explicit decimals keep the per-shot fraction.
	RequireText(Build("{damage} x {count}", 100.0, 8.0, 1), "12.5 x 8", "one decimal keeps 12.5");

	// Auto decimals (-1) keep the fraction as well.
	RequireText(Build("{damage} x {count}", 100.0, 8.0, -1), "12.5 x 8", "auto decimals keep 12.5");

	// Custom layouts and the original total are supported.
	RequireText(Build("{damage}/{count} ({total})", 100.0, 8.0, 0), "13/8 (100)", "custom format exposes all placeholders");

	// Ties round away from zero like the in-game numbers.
	RequireText(Build("{damage} x {count}", 9.0, 2.0, 0), "5 x 2", "4.5 rounds to 5");

	// Repeated tokens are all replaced.
	RequireText(Build("{damage} ({damage})", 100.0, 8.0, 0), "13 (13)", "repeated placeholder is replaced everywhere");

	// Invalid projectile counts produce no text so the caller keeps the vanilla row.
	Require(Build("{damage} x {count}", 100.0, 0.0, 0).empty(), "zero projectiles produce no text");
	Require(Build("{damage} x {count}", 100.0, -1.0, 0).empty(), "negative projectiles produce no text");

	// Empty format falls back to the documented default.
	RequireText(Build("", 100.0, 8.0, 0), "13 x 8", "empty format falls back to default");

	RequireText(FormatNumber(13.0, -1), "13", "auto formatting keeps whole numbers integral");
	RequireText(FormatNumber(12.5, -1), "12.5", "auto formatting keeps one decimal");

	std::cout << "InlineDamage fixtures passed: per-shot rounding, placeholders, decimals, and invalid counts.\n";
	return 0;
}
