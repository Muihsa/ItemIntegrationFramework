#include "../src/CombatBusRegistry.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {
	using IIF_API::CombatBus::DamageContextV2;
	using IIF_API::CombatBus::DamageComponent;
	using IIF_API::CombatBus::DamageKind;
	using IIF_API::CombatBus::ProviderV2;
	using IIF::CombatBus::DispatchResult;
	using IIF::CombatBus::RegistrationResult;
	using IIF::CombatBus::Registry;

	std::vector<std::string> g_calls;
	std::vector<float> g_outgoingObservations;
	std::vector<float> g_incomingObservations;
	std::vector<DamageComponent> g_outgoingComponents;
	std::vector<DamageComponent> g_incomingComponents;
	Registry* g_registryForReentry = nullptr;
	RegistrationResult g_reentrantRegistration = RegistrationResult::Invalid;
	std::atomic<int> g_outgoingCalls{ 0 };
	std::atomic<int> g_incomingCalls{ 0 };
	std::atomic<int> g_parallelCalls{ 0 };
	std::atomic<int> g_crossKindIdIncomingCalls{ 0 };

	void Require(bool condition, const char* message)
	{
		if (!condition) {
			std::cerr << "FAIL: " << message << '\n';
			std::exit(1);
		}
	}

	DamageContextV2 MakeContext(DamageKind kind, std::uint32_t entryPoint, float* damage, float original,
		DamageComponent component = DamageComponent::Unknown)
	{
		return { sizeof(DamageContextV2), IIF_API::CombatBus::kVersion2, nullptr, nullptr, nullptr,
			damage, original, entryPoint, kind, component, 0 };
	}

	void ModifyAsWRF(const DamageContextV2* context)
	{
		g_calls.emplace_back("WRF");
		g_outgoingComponents.push_back(context->component);
		g_outgoingCalls.fetch_add(1, std::memory_order_relaxed);
		*context->damage *= 1.5f;
	}

	void ObserveAsCSF(const DamageContextV2* context)
	{
		g_calls.emplace_back("CSF");
		g_outgoingComponents.push_back(context->component);
		g_outgoingObservations.push_back(*context->damage);
		g_outgoingCalls.fetch_add(1, std::memory_order_relaxed);
	}

	void ObserveAsPAS(const DamageContextV2* context)
	{
		g_calls.emplace_back("PAS");
		g_incomingComponents.push_back(context->component);
		g_incomingObservations.push_back(*context->damage);
		g_incomingCalls.fetch_add(1, std::memory_order_relaxed);
	}

	void ObserveLateIncoming(const DamageContextV2* context)
	{
		Require(context->kind == DamageKind::Incoming, "late provider receives only IncomingDamage");
		g_incomingCalls.fetch_add(1, std::memory_order_relaxed);
	}

	void RegisterIncomingFromCallback(const DamageContextV2*)
	{
		if (!g_registryForReentry) return;
		static constexpr char kLateId[] = "LateIncoming";
		static ProviderV2 lateProvider{ sizeof(ProviderV2), IIF_API::CombatBus::kVersion2, kLateId,
			DamageKind::Incoming, 50, &ObserveLateIncoming };
		g_reentrantRegistration = g_registryForReentry->Register(&lateProvider);
		g_registryForReentry = nullptr;
	}

	void CountParallelDispatch(const DamageContextV2*) { g_parallelCalls.fetch_add(1, std::memory_order_relaxed); }
	void Noop(const DamageContextV2*) {}
	void CountCrossKindIdIncoming(const DamageContextV2* context)
	{
		Require(context->kind == DamageKind::Incoming, "same ID in another kind only receives IncomingDamage");
		g_crossKindIdIncomingCalls.fetch_add(1, std::memory_order_relaxed);
	}
	void RecordTieZ(const DamageContextV2*) { g_calls.emplace_back("z-provider"); }
	void RecordTieA(const DamageContextV2*) { g_calls.emplace_back("a-provider"); }
	void RecordLowerPriority(const DamageContextV2*) { g_calls.emplace_back("priority-first"); }
	void RecordIncomingTieZ(const DamageContextV2*) { g_calls.emplace_back("incoming-z"); }
	void RecordIncomingTieA(const DamageContextV2*) { g_calls.emplace_back("incoming-a"); }
	void RecordIncomingLowerPriority(const DamageContextV2*) { g_calls.emplace_back("incoming-first"); }

	Registry::SnapshotHandle MakeOrphanedSnapshot()
	{
		Registry temporary;
		char id[] = "snapshot-owner";
		ProviderV2 provider{ sizeof(ProviderV2), IIF_API::CombatBus::kVersion2, id,
			DamageKind::Outgoing, 0, &Noop };
		Require(temporary.Register(&provider) == RegistrationResult::Added, "register snapshot lifetime fixture");
		auto snapshot = temporary.Snapshot();
		id[0] = 'X';
		return snapshot;
	}
}

int main()
{
	using namespace IIF_API::CombatBus;

	// Pure logic coverage uses opaque/null object fields; no Actor or weapon is constructed.
	float untouched = 42.0f;
	auto emptyOutgoing = MakeContext(DamageKind::Outgoing, kModAttackDamage, &untouched, untouched);
	float untouchedIncoming = 91.0f;
	auto emptyIncoming = MakeContext(DamageKind::Incoming, kModIncomingDamage, &untouchedIncoming, untouchedIncoming);
	Registry emptyRegistry;
	Require(emptyRegistry.Dispatch(&emptyOutgoing) == DispatchResult::Dispatched, "empty outgoing registry accepts valid context");
	Require(untouched == 42.0f, "no outgoing provider preserves the original value");
	Require(emptyRegistry.Dispatch(&emptyIncoming) == DispatchResult::Dispatched, "empty incoming registry accepts valid context");
	Require(untouchedIncoming == 91.0f, "no incoming provider preserves the original value");
	Require(!emptyOutgoing.attacker && !emptyOutgoing.target && !emptyOutgoing.weapon,
		"fixture context needs no Actor or weapon objects");

	// WRF and CSF both subscribe to OutgoingDamage. Numeric priority then provider ID orders providers.
	Registry combatRegistry;
	char wrfId[] = "WRF";
	ProviderV2 wrf{ sizeof(ProviderV2), kVersion2, wrfId, DamageKind::Outgoing, 100, &ModifyAsWRF };
	ProviderV2 csf{ sizeof(ProviderV2), kVersion2, "CSF", DamageKind::Outgoing, 200, &ObserveAsCSF };
	ProviderV2 pas{ sizeof(ProviderV2), kVersion2, "PAS", DamageKind::Incoming, 100, &ObserveAsPAS };
	Require(combatRegistry.Register(&wrf) == RegistrationResult::Added, "register WRF OutgoingDamage provider");
	Require(combatRegistry.Register(&csf) == RegistrationResult::Added, "register CSF OutgoingDamage provider");
	Require(combatRegistry.Register(&pas) == RegistrationResult::Added, "register future PAS IncomingDamage provider");
	wrfId[0] = 'X';

	const auto sorted = combatRegistry.Snapshot();
	Require(sorted->size() == 3, "immutable snapshot contains all providers");
	Require((*sorted)[0].providerId == "WRF" && (*sorted)[0].priority == 100 && (*sorted)[0].kind == DamageKind::Outgoing,
		"WRF Outgoing priority 100 sorts before CSF");
	Require((*sorted)[1].providerId == "CSF" && (*sorted)[1].priority == 200 && (*sorted)[1].kind == DamageKind::Outgoing,
		"CSF Outgoing priority 200 follows WRF");
	Require((*sorted)[2].providerId == "PAS" && (*sorted)[2].kind == DamageKind::Incoming,
		"Incoming providers occupy their own kind partition");

	float outgoingDamage = 10.0f;
	auto outgoingContext = MakeContext(DamageKind::Outgoing, kModAttackDamage, &outgoingDamage, outgoingDamage,
		DamageComponent::Health);
	g_calls.clear();
	g_outgoingComponents.clear();
	g_outgoingObservations.clear();
	Require(combatRegistry.Dispatch(&outgoingContext) == DispatchResult::Dispatched, "OutgoingDamage dispatch succeeds");
	const float firstOutgoingResult = outgoingDamage;
	Require(firstOutgoingResult == 15.0f, "WRF changes the outgoing damage value");
	Require(g_outgoingObservations.size() == 1 && g_outgoingObservations.back() == 15.0f,
		"CSF observes the value written by WRF");
	Require((g_calls == std::vector<std::string>{ "WRF", "CSF" }), "Outgoing providers run by priority");
	Require((g_outgoingComponents == std::vector<DamageComponent>{ DamageComponent::Health, DamageComponent::Health }),
		"Health component is preserved for all matching Outgoing providers");
	Require(outgoingContext.originalDamage == 10.0f, "originalDamage stays the dispatch-entry snapshot");
	Require(g_incomingCalls.load(std::memory_order_relaxed) == 0,
		"OutgoingDamage never invokes an IncomingDamage provider");

	outgoingDamage = 10.0f;
	g_calls.clear();
	Require(combatRegistry.Dispatch(&outgoingContext) == DispatchResult::Dispatched, "repeat OutgoingDamage dispatch succeeds");
	Require(outgoingDamage == firstOutgoingResult && g_outgoingObservations.back() == 15.0f,
		"repeated dispatch from the same initial value is consistent");
	Require((g_calls == std::vector<std::string>{ "WRF", "CSF" }), "repeated dispatch keeps deterministic order");

	float incomingDamage = 20.0f;
	auto incomingContext = MakeContext(DamageKind::Incoming, kModIncomingDamage, &incomingDamage, incomingDamage,
		DamageComponent::Physical);
	g_calls.clear();
	g_incomingComponents.clear();
	Require(combatRegistry.Dispatch(&incomingContext) == DispatchResult::Dispatched, "IncomingDamage dispatch succeeds");
	Require((g_calls == std::vector<std::string>{ "PAS" }), "IncomingDamage invokes PAS only");
	Require(g_outgoingCalls.load(std::memory_order_relaxed) == 4,
		"IncomingDamage never invokes WRF or CSF OutgoingDamage providers");
	Require(g_incomingObservations.size() == 1 && g_incomingObservations.back() == 20.0f,
		"PAS observes the incoming context's own value");
	Require((g_incomingComponents == std::vector<DamageComponent>{ DamageComponent::Physical }),
		"Physical component is preserved for the matching Incoming provider");

	// Unknown remains a valid, explicit classification when a call path has not
	// proved which damage component the pointer represents.
	float unknownDamage = 7.0f;
	auto unknownContext = MakeContext(DamageKind::Incoming, kModIncomingDamage, &unknownDamage, unknownDamage,
		DamageComponent::Unknown);
	g_incomingComponents.clear();
	Require(combatRegistry.Dispatch(&unknownContext) == DispatchResult::Dispatched,
		"Unknown component is a valid context classification");
	Require((g_incomingComponents == std::vector<DamageComponent>{ DamageComponent::Unknown }),
		"Unknown component is preserved without value-based guessing");

	// Reject malformed contexts and descriptors, including mismatched native EntryPoint IDs.
	ProviderV2 badVersion{ sizeof(ProviderV2), kVersion2 + 1, "bad-version", DamageKind::Outgoing, 0, &Noop };
	ProviderV2 badKind{ sizeof(ProviderV2), kVersion2, "bad-kind", static_cast<DamageKind>(99), 0, &Noop };
	ProviderV2 badCallback{ sizeof(ProviderV2), kVersion2, "bad-callback", DamageKind::Outgoing, 0, nullptr };
	ProviderV2 shortStruct{ sizeof(ProviderV2) - 1, kVersion2, "short-struct", DamageKind::Outgoing, 0, &Noop };
	ProviderV2 emptyId{ sizeof(ProviderV2), kVersion2, "", DamageKind::Outgoing, 0, &Noop };
	ProviderV2 illegalId{ sizeof(ProviderV2), kVersion2, "bad provider", DamageKind::Outgoing, 0, &Noop };
	std::string longId(128, 'x');
	ProviderV2 longProviderId{ sizeof(ProviderV2), kVersion2, longId.c_str(), DamageKind::Outgoing, 0, &Noop };
	Require(combatRegistry.Register(nullptr) == RegistrationResult::Invalid, "reject null provider");
	Require(combatRegistry.Register(&badVersion) == RegistrationResult::Invalid, "reject unknown provider version");
	Require(combatRegistry.Register(&badKind) == RegistrationResult::Invalid, "reject unknown provider kind");
	Require(combatRegistry.Register(&badCallback) == RegistrationResult::Invalid, "reject null callback");
	Require(combatRegistry.Register(&shortStruct) == RegistrationResult::Invalid, "reject undersized provider descriptor");
	Require(combatRegistry.Register(&emptyId) == RegistrationResult::Invalid, "reject empty provider ID");
	Require(combatRegistry.Register(&illegalId) == RegistrationResult::Invalid, "reject illegal provider ID characters");
	Require(combatRegistry.Register(&longProviderId) == RegistrationResult::Invalid, "reject provider ID longer than 127 bytes");
	ProviderV2 duplicateWRF{ sizeof(ProviderV2), kVersion2, "WRF", DamageKind::Outgoing, 100, &Noop };
	Require(combatRegistry.Register(&duplicateWRF) == RegistrationResult::Duplicate,
		"reject duplicate (kind, provider ID) pair");
	ProviderV2 sameIdentityOtherKind{ sizeof(ProviderV2), kVersion2, "WRF", DamageKind::Incoming, 100,
		&CountCrossKindIdIncoming };
	Require(combatRegistry.Register(&sameIdentityOtherKind) == RegistrationResult::Added,
		"allow one framework ID to register once for each distinct DamageKind");
	g_incomingComponents.clear();
	Require(combatRegistry.Dispatch(&incomingContext) == DispatchResult::Dispatched,
		"dispatch with a same-ID provider under the other kind");
	Require(g_crossKindIdIncomingCalls.load(std::memory_order_relaxed) == 1,
		"same ID under another kind runs only on its matching event kind");
	ProviderV2 nullId{ sizeof(ProviderV2), kVersion2, nullptr, DamageKind::Outgoing, 0, &Noop };
	Require(combatRegistry.Register(&nullId) == RegistrationResult::Invalid, "reject null provider ID");

	auto badEntryPoint = incomingContext;
	badEntryPoint.entryPoint = kModAttackDamage;
	Require(combatRegistry.Dispatch(&badEntryPoint) == DispatchResult::InvalidContext,
		"reject IncomingDamage context with outgoing native EntryPoint");
	auto badContextKind = outgoingContext;
	badContextKind.kind = static_cast<DamageKind>(99);
	Require(combatRegistry.Dispatch(&badContextKind) == DispatchResult::InvalidContext, "reject unknown context kind");
	auto badComponent = outgoingContext;
	badComponent.component = static_cast<DamageComponent>(99);
	Require(combatRegistry.Dispatch(&badComponent) == DispatchResult::InvalidContext,
		"reject an out-of-range DamageComponent value");
	auto nullDamage = outgoingContext;
	nullDamage.damage = nullptr;
	Require(combatRegistry.Dispatch(&nullDamage) == DispatchResult::InvalidContext,
		"reject a context without a mutable damage pointer");
	auto undersizedContext = outgoingContext;
	undersizedContext.structSize = sizeof(DamageContextV2) - 1;
	Require(combatRegistry.Dispatch(&undersizedContext) == DispatchResult::InvalidContext,
		"reject an undersized context for the updated V2 layout");
	auto badContextVersion = outgoingContext;
	badContextVersion.version = kVersion2 + 1;
	Require(combatRegistry.Dispatch(&badContextVersion) == DispatchResult::InvalidContext, "reject unknown context version");

	// Equal-priority order is stable by ID; priority is evaluated independently per kind.
	Registry tieRegistry;
	ProviderV2 tiedZ{ sizeof(ProviderV2), kVersion2, "z-provider", DamageKind::Outgoing, 10, &RecordTieZ };
	ProviderV2 tiedA{ sizeof(ProviderV2), kVersion2, "a-provider", DamageKind::Outgoing, 10, &RecordTieA };
	ProviderV2 lowerPriority{ sizeof(ProviderV2), kVersion2, "priority-first", DamageKind::Outgoing, -5, &RecordLowerPriority };
	ProviderV2 incomingTieZ{ sizeof(ProviderV2), kVersion2, "z-incoming", DamageKind::Incoming, 10, &RecordIncomingTieZ };
	ProviderV2 incomingTieA{ sizeof(ProviderV2), kVersion2, "a-incoming", DamageKind::Incoming, 10, &RecordIncomingTieA };
	ProviderV2 incomingLowerPriority{ sizeof(ProviderV2), kVersion2, "first-incoming", DamageKind::Incoming, -5,
		&RecordIncomingLowerPriority };
	Require(tieRegistry.Register(&tiedZ) == RegistrationResult::Added, "register lexical tie Z");
	Require(tieRegistry.Register(&tiedA) == RegistrationResult::Added, "register lexical tie A");
	Require(tieRegistry.Register(&lowerPriority) == RegistrationResult::Added, "register lower numeric priority");
	Require(tieRegistry.Register(&incomingTieZ) == RegistrationResult::Added, "register Incoming lexical tie Z");
	Require(tieRegistry.Register(&incomingTieA) == RegistrationResult::Added, "register Incoming lexical tie A");
	Require(tieRegistry.Register(&incomingLowerPriority) == RegistrationResult::Added,
		"register Incoming lower numeric priority");
	g_calls.clear();
	Require(tieRegistry.Dispatch(&outgoingContext) == DispatchResult::Dispatched, "dispatch priority and ID fixture");
	Require((g_calls == std::vector<std::string>{ "priority-first", "a-provider", "z-provider" }),
		"lower numeric priority then ascending ID determines order");
	g_calls.clear();
	Require(tieRegistry.Dispatch(&incomingContext) == DispatchResult::Dispatched, "dispatch Incoming priority and ID fixture");
	Require((g_calls == std::vector<std::string>{ "incoming-first", "incoming-a", "incoming-z" }),
		"Incoming providers independently sort by numeric priority then ascending ID");

	// Callback registration would deadlock if Dispatch held the registry mutex.
	Registry reentrantRegistry;
	ProviderV2 registrar{ sizeof(ProviderV2), kVersion2, "Registrar", DamageKind::Outgoing, 0, &RegisterIncomingFromCallback };
	Require(reentrantRegistry.Register(&registrar) == RegistrationResult::Added, "register reentrant provider");
	g_registryForReentry = &reentrantRegistry;
	g_reentrantRegistration = RegistrationResult::Invalid;
	g_incomingCalls.store(0, std::memory_order_relaxed);
	float reentrantDamage = 3.0f;
	auto reentrantOutgoing = MakeContext(DamageKind::Outgoing, kModAttackDamage, &reentrantDamage, reentrantDamage);
	Require(reentrantRegistry.Dispatch(&reentrantOutgoing) == DispatchResult::Dispatched, "dispatch with registration callback completes");
	Require(g_reentrantRegistration == RegistrationResult::Added, "callback can register without Registry mutex held");
	Require(g_incomingCalls.load(std::memory_order_relaxed) == 0,
		"registration during Outgoing callback does not invoke the new Incoming provider");
	Require(reentrantRegistry.Dispatch(&reentrantOutgoing) == DispatchResult::Dispatched, "next outgoing dispatch completes");
	Require(g_incomingCalls.load(std::memory_order_relaxed) == 0, "new Incoming provider is excluded from Outgoing dispatch");
	float lateIncomingDamage = 8.0f;
	auto lateIncoming = MakeContext(DamageKind::Incoming, kModIncomingDamage, &lateIncomingDamage, lateIncomingDamage);
	Require(reentrantRegistry.Dispatch(&lateIncoming) == DispatchResult::Dispatched, "next Incoming dispatch completes");
	Require(g_incomingCalls.load(std::memory_order_relaxed) == 1, "new provider appears in the next matching-kind snapshot");

	// A held immutable snapshot owns provider IDs and outlives its Registry.
	auto orphanedSnapshot = MakeOrphanedSnapshot();
	Require(orphanedSnapshot->size() == 1 && orphanedSnapshot->front().providerId == "snapshot-owner",
		"snapshot owns provider ID beyond source string and Registry lifetime");
	orphanedSnapshot->front().callback(&emptyOutgoing);

	// Snapshot publication is atomic while concurrent dispatch and registration proceed.
	Registry concurrentRegistry;
	std::atomic<bool> registrationsDone{ false };
	std::thread registrarThread([&] {
		for (int index = 0; index < 64; ++index) {
			const std::string id = "parallel-" + std::to_string(index);
			ProviderV2 provider{ sizeof(ProviderV2), kVersion2, id.c_str(), DamageKind::Outgoing, index, &CountParallelDispatch };
			if (concurrentRegistry.Register(&provider) != RegistrationResult::Added) std::abort();
		}
		registrationsDone.store(true, std::memory_order_release);
	});
	float concurrentDamage = 1.0f;
	auto concurrentContext = MakeContext(DamageKind::Outgoing, kModAttackDamage, &concurrentDamage, concurrentDamage);
	do {
		Require(concurrentRegistry.Dispatch(&concurrentContext) == DispatchResult::Dispatched,
			"dispatch during concurrent registration succeeds");
	} while (!registrationsDone.load(std::memory_order_acquire));
	registrarThread.join();
	Require(concurrentRegistry.Snapshot()->size() == 64, "all concurrent providers are in the published snapshot");
	Require(g_parallelCalls.load(std::memory_order_relaxed) > 0, "concurrent dispatch invoked published providers");

	std::cout << "CombatBusRegistry fixtures passed: event isolation, ordering, ownership, snapshot lifetime, reentry, and concurrent publication.\n";
	return 0;
}

