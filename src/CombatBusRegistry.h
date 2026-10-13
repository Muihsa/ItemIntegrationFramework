#pragma once

#include "CombatBusAPI.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace IIF::CombatBus {
	struct RegisteredProvider {
		std::string providerId;
		IIF_API::CombatBus::DamageKind kind;
		std::int32_t priority;
		IIF_API::CombatBus::DamageCallbackV2 callback;
	};

	enum class RegistrationResult {
		Added,
		Duplicate,
		Invalid
	};

	enum class DispatchResult {
		Dispatched,
		InvalidContext
	};

	class Registry final {
	public:
		using ProviderList = std::vector<RegisteredProvider>;
		using SnapshotHandle = std::shared_ptr<const ProviderList>;

		Registry() : _snapshot(std::make_shared<const ProviderList>()) {}

		[[nodiscard]] RegistrationResult Register(const IIF_API::CombatBus::ProviderV2* provider)
		{
			using namespace IIF_API::CombatBus;
			if (!provider || provider->structSize < sizeof(ProviderV2) || provider->version != kVersion2 ||
				!provider->providerId || provider->providerId[0] == '\0' || !provider->callback ||
				(provider->kind != DamageKind::Outgoing && provider->kind != DamageKind::Incoming)) {
				return RegistrationResult::Invalid;
			}

			std::size_t idLength = 0;
			while (idLength <= kMaxProviderIdLength && provider->providerId[idLength] != '\0') ++idLength;
			if (idLength == 0 || idLength > kMaxProviderIdLength) return RegistrationResult::Invalid;
			for (std::size_t index = 0; index < idLength; ++index) {
				const char value = provider->providerId[index];
				const bool alphaNumeric = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
					(value >= '0' && value <= '9');
				if (!alphaNumeric && value != '-' && value != '_' && value != '.') return RegistrationResult::Invalid;
			}
			const std::string_view id{ provider->providerId, idLength };

			std::scoped_lock lock{ _mutex };
			const auto current = _snapshot.load(std::memory_order_acquire);
			const auto existing = std::find_if(current->begin(), current->end(), [&](const auto& value) {
				return value.kind == provider->kind && value.providerId == id;
			});
			if (existing != current->end()) return RegistrationResult::Duplicate;

			// Registration is cold-path work: rebuild and sort a new immutable snapshot,
			// then atomically publish it. Active dispatchers retain the prior snapshot.
			auto next = std::make_shared<ProviderList>(*current);
			next->push_back({ std::string{id}, provider->kind, provider->priority, provider->callback });
			std::sort(next->begin(), next->end(), [](const auto& left, const auto& right) {
				if (left.kind != right.kind) return left.kind < right.kind;
				if (left.priority != right.priority) return left.priority < right.priority;
				return left.providerId < right.providerId;
			});
			_snapshot.store(std::move(next), std::memory_order_release);
			return RegistrationResult::Added;
		}

		[[nodiscard]] SnapshotHandle Snapshot() const noexcept
		{
			return _snapshot.load(std::memory_order_acquire);
		}

		// Snapshot acquisition does not take the Registry mutex. This does not claim
		// that atomic<shared_ptr> is hardware lock-free. The immutable shared snapshot
		// owns IDs and remains alive through callback calls.
		// Provider DLLs must stay loaded because V2 has no unregister operation.
		// Registrations from callbacks appear on the next dispatch. Concurrent dispatches
		// are supported; providers synchronize their own state. Callbacks must not throw
		// across the DLL boundary or recursively dispatch the same context without a guard.
		[[nodiscard]] DispatchResult Dispatch(const IIF_API::CombatBus::DamageContextV2* context) const
		{
			using namespace IIF_API::CombatBus;
			if (!context || context->structSize < sizeof(DamageContextV2) || context->version != kVersion2 ||
				!context->damage ||
				(context->component != DamageComponent::Unknown && context->component != DamageComponent::Health &&
					context->component != DamageComponent::Physical) ||
				(context->kind == DamageKind::Outgoing && context->entryPoint != kModAttackDamage) ||
				(context->kind == DamageKind::Incoming && context->entryPoint != kModIncomingDamage) ||
				(context->kind != DamageKind::Outgoing && context->kind != DamageKind::Incoming)) {
				return DispatchResult::InvalidContext;
			}

			const auto providers = Snapshot();
			for (const auto& provider : *providers) {
				if (provider.kind == context->kind) provider.callback(context);
			}
			return DispatchResult::Dispatched;
		}

	private:
		static constexpr std::size_t kMaxProviderIdLength = 127;
		mutable std::mutex _mutex;
		std::atomic<SnapshotHandle> _snapshot;
	};
}
