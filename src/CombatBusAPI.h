#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

// Versioned, borrowed-data ABI shared by IIF and provider DLLs.
// Keep this header free of STL containers, ownership types, and C++ classes.
// IIF invokes callbacks synchronously. A registered provider DLL must remain loaded
// for as long as its callback is registered; V2 has no unregister operation.
// Dispatches may occur concurrently, so providers must synchronize their own state.
namespace IIF_API::CombatBus {
	inline constexpr std::uint32_t kVersion2 = 2;
	inline constexpr std::uint32_t kModAttackDamage = 0x23;
	inline constexpr std::uint32_t kModIncomingDamage = 0x24;

	// These values identify independent native EntryPoint invocations. IIF never
	// dispatches one kind's providers while handling the other kind.
	enum class DamageKind : std::uint32_t {
		Outgoing = 1,
		Incoming = 2
	};
	static_assert(sizeof(DamageKind) == 4);

	enum class DamageComponent : std::uint32_t {
		Unknown = 0,
		Health = 1,
		Physical = 2
	};
	static_assert(sizeof(DamageComponent) == 4);

	struct DamageContextV2 {
		std::uint32_t structSize;
		std::uint32_t version;
		// Borrowed Actor/TESObjectWEAP pointers. IIF resolves the object field of the
		// engine's BGSObjectInstance arguments for these two verified HitData paths.
		// Valid only during this synchronous callback; never retain or free them.
		void* attacker;
		void* target;
		void* weapon;
		// Writable float pointer for this particular native invocation. Distinct
		// EntryPoint calls or other call paths are not promised to share an address.
		float* damage;
		// Value read from *damage at IIF callback-dispatch entry, before providers run.
		float originalDamage;
		std::uint32_t entryPoint;
		DamageKind kind;
		// Set only when the native call path proves which writable HitData component
		// this invocation references. Unknown is valid; values are never inferred from
		// the float's magnitude or from a different native call path.
		DamageComponent component;
		std::uint32_t reserved;
	};

	using DamageCallbackV2 = void (*)(const DamageContextV2* context);

	struct ProviderV2 {
		std::uint32_t structSize;
		std::uint32_t version;
		const char* providerId;
		DamageKind kind;
		std::int32_t priority;
		DamageCallbackV2 callback;
	};

	// Registration reads the descriptor synchronously and IIF copies providerId; the
	// caller may release the descriptor/string after this function returns.
	using RegisterProviderV2 = bool (*)(const ProviderV2* provider);

	struct InterfaceV2 {
		std::uint32_t structSize;
		std::uint32_t version;
		RegisterProviderV2 registerDamageProvider;
	};

	static_assert(sizeof(void*) == 8, "CombatBus V2 ABI is defined for Win64");
	static_assert(std::is_standard_layout_v<DamageContextV2>);
	static_assert(std::is_trivial_v<DamageContextV2>);
	static_assert(std::is_trivially_copyable_v<DamageContextV2>);
	static_assert(alignof(DamageContextV2) == 8);
	static_assert(offsetof(DamageContextV2, attacker) == 8);
	static_assert(offsetof(DamageContextV2, target) == 16);
	static_assert(offsetof(DamageContextV2, weapon) == 24);
	static_assert(offsetof(DamageContextV2, damage) == 32);
	static_assert(offsetof(DamageContextV2, originalDamage) == 40);
	static_assert(offsetof(DamageContextV2, entryPoint) == 44);
	static_assert(offsetof(DamageContextV2, kind) == 48);
	static_assert(offsetof(DamageContextV2, component) == 52);
	static_assert(offsetof(DamageContextV2, reserved) == 56);
	static_assert(sizeof(DamageContextV2) == 64);
	static_assert(std::is_standard_layout_v<ProviderV2>);
	static_assert(std::is_trivial_v<ProviderV2>);
	static_assert(std::is_trivially_copyable_v<ProviderV2>);
	static_assert(alignof(ProviderV2) == 8);
	static_assert(offsetof(ProviderV2, providerId) == 8);
	static_assert(offsetof(ProviderV2, kind) == 16);
	static_assert(offsetof(ProviderV2, priority) == 20);
	static_assert(offsetof(ProviderV2, callback) == 24);
	static_assert(sizeof(ProviderV2) == 32);
	static_assert(std::is_standard_layout_v<InterfaceV2>);
	static_assert(std::is_trivial_v<InterfaceV2>);
	static_assert(std::is_trivially_copyable_v<InterfaceV2>);
	static_assert(alignof(InterfaceV2) == 8);
	static_assert(offsetof(InterfaceV2, registerDamageProvider) == 8);
	static_assert(sizeof(InterfaceV2) == 16);
}
