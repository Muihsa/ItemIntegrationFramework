#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

// Pure text formatting for the NumProjectiles inline damage row. Kept in a
// separate header so it can be tested without the game SDK.
namespace IIF::InlineDamage
{
	inline std::string FormatNumber(double a_value, int a_digits)
	{
		char buf[64];
		if (a_digits < 0) {
			if (std::abs(a_value - std::round(a_value)) < 0.0005) std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::lround(a_value)));
			else std::snprintf(buf, sizeof(buf), "%.1f", a_value);
		}
		else {
			std::snprintf(buf, sizeof(buf), "%.*f", (std::min)(a_digits, 6), a_value);
		}
		return std::string(buf);
	}

	inline void ReplaceToken(std::string& a_text, const std::string& a_token, const std::string& a_value)
	{
		size_t pos = 0;
		while ((pos = a_text.find(a_token, pos)) != std::string::npos) {
			a_text.replace(pos, a_token.size(), a_value);
			pos += a_value.empty() ? a_token.size() : a_value.size();
		}
	}

	// {damage} = per-shot damage (rounded to a_digits), {count} = projectile count,
	// {total} = original total damage. Returns an empty string for a non-positive
	// projectile count so callers keep the vanilla damage row untouched.
	inline std::string Build(const std::string& a_format, double a_total, double a_projectiles, int a_digits)
	{
		if (a_projectiles <= 0.0) return {};

		double perShot = a_total / a_projectiles;
		if (a_digits >= 0) {
			const double scale = std::pow(10.0, (std::min)(a_digits, 6));
			perShot = std::round(perShot * scale) / scale;
		}

		std::string text = a_format.empty() ? std::string("{damage} x {count}") : a_format;
		ReplaceToken(text, "{damage}", FormatNumber(perShot, a_digits));
		ReplaceToken(text, "{count}", FormatNumber(a_projectiles, 0));
		ReplaceToken(text, "{total}", FormatNumber(a_total, -1));
		return text;
	}
}
