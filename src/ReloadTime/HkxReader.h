#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace IIF::ReloadTime
{
	// Turns a game-relative path into a Data-relative BSResource path:
	// forward slashes become backslashes and the "Meshes\" prefix is added when
	// missing (animation and behavior assets all live under Meshes).
	std::string NormalizeResourcePath(std::string a_path);

	// Probes the loose files plus registered BA2 archives (cached).
	bool ResourceExists(const std::string& a_resourcePath);

	// Total duration of the hkaAnimation inside a Havok packfile, in seconds.
	// Returns nullopt when the resource is missing or cannot be parsed.
	std::optional<float> ReadAnimationDuration(const std::string& a_resourcePath);

	// True when the behavior graph defines a clip generator for a_leaf
	// ("WPNReload"), following hkbBehaviorReferenceGenerator references.
	bool BehaviorDefinesClip(const std::string& a_behaviorPath, std::string_view a_leaf);
}
