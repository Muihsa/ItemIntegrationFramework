#include "pch.h"
#include "UIHooks.h"
#include "ImGuiManager.h"
#include "JsonReader.h"
#include "Localizer.h"
#include "IIF_API.h"
#include <MinHook.h>

#define PLUGIN_NAME "ItemIntegrationFramework"
#define PLUGIN_VERSION_MAJOR 1
#define PLUGIN_VERSION_MINOR 0
#define PLUGIN_VERSION_PATCH 5

static bool g_imguiEditorInstalled = false;
static bool g_scaleformTranslationsRegistered = false;

static void RegisterScaleformTranslations(const char* reason) {
	if (g_scaleformTranslationsRegistered) return;

	auto* scaleformManager = RE::BSScaleformManager::GetSingleton();
	if (!scaleformManager) return;

	auto* translator = scaleformManager->GetTranslator();
	if (!translator) return;

	translator->AddTranslationsMod(PLUGIN_NAME);
	g_scaleformTranslationsRegistered = true;
	REX::INFO("[IIF] Scaleform translations registered. reason={}", reason ? reason : "unknown");
}

static void TryInstallImGuiEditor(const char* reason) {
	if (g_imguiEditorInstalled) return;
	if (IIF::UI::ImGuiManager::GetSingleton().Install()) {
		g_imguiEditorInstalled = true;
		REX::INFO("[IIF] ImGui editor installed. reason={}", reason ? reason : "unknown");
	}
	else {
		REX::WARN("[IIF] ImGui editor install deferred; swapchain was not ready. reason={}", reason ? reason : "unknown");
	}
}

// Shared combat modifier dispatch for dependent plugins.
namespace IIF::CombatBus {
	static std::vector<IIF_API::DamageModifierCallback> g_damageModifiers;
	static std::vector<IIF_API::ArmorModifierCallback> g_armorModifiers;

	void RegisterDamageModifier(IIF_API::DamageModifierCallback cb) { if (cb) g_damageModifiers.push_back(cb); }
	void RegisterArmorModifier(IIF_API::ArmorModifierCallback cb) { if (cb) g_armorModifiers.push_back(cb); }

	using HasPerkEntries_t = bool(*)(RE::Actor*, std::uint8_t);
	static HasPerkEntries_t _HasPerkEntries_Original = nullptr;
	bool HasPerkEntries_Hook(RE::Actor* a_this, std::uint8_t a_entryPoint) {
		if (a_entryPoint == 0x23 || a_entryPoint == 0x55) return true;
		return _HasPerkEntries_Original(a_this, a_entryPoint);
	}

	using HandleEntryPoint_t = void(*)(std::uint32_t, RE::Actor*, void*, void*, void*, void*, void*, void*);
	static HandleEntryPoint_t _HandleEntryPoint_Original = nullptr;

	void HandleEntryPoint_Hook(std::uint32_t a_entryPoint, RE::Actor* a_perkOwner, void* a3, void* a4, void* a5, void* a6, void* a7, void* a8) {
		_HandleEntryPoint_Original(a_entryPoint, a_perkOwner, a3, a4, a5, a6, a7, a8);

		if (!a_perkOwner || !a_perkOwner->IsPlayerRef()) return;

		if (a_entryPoint == 0x23) {
			auto weapon = static_cast<RE::TESObjectWEAP*>(a3);
			auto target = static_cast<RE::Actor*>(a4);
			float* damagePtr = static_cast<float*>(a5);

			if (!target) return;

			if (weapon && damagePtr) {
				for (auto& cb : g_damageModifiers) cb(a_perkOwner, weapon, damagePtr);
			}
		}
		else if (a_entryPoint == 0x55) {
			float* ratingPtr = static_cast<float*>(a3);
			if (ratingPtr) {
				for (auto& cb : g_armorModifiers) cb(a_perkOwner, ratingPtr);
			}
		}
	}

	void Install() {
		if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) return;
		REL::Relocation<std::uintptr_t> actorVtbl{ RE::VTABLE::Actor[0] };
		_HasPerkEntries_Original = reinterpret_cast<HasPerkEntries_t>(actorVtbl.write_vfunc(0x10B, reinterpret_cast<std::uintptr_t>(HasPerkEntries_Hook)));
		REL::Relocation<std::uintptr_t> entryPointAddr{ RE::ID::BGSEntryPoint::HandleEntryPoint };
		MH_CreateHook((void*)entryPointAddr.address(), (void*)&HandleEntryPoint_Hook, (void**)&_HandleEntryPoint_Original);
		MH_EnableHook(MH_ALL_HOOKS);
		REX::INFO("[IIF CombatBus] Shared combat hooks installed.");
	}
}

static void RegisterCPPCard(const IIF_API::CPPCardRegistration* reg) {
	if (!reg || !reg->id) return;

	auto& entry = IIF::JsonReader::g_cppOverrides[reg->id];
	entry.id = reg->id;

	entry.defaultPriority = reg->defaultPriority;
	entry.defaultAnchorTargets.clear();
	entry.defaultAnchorModes.clear();
	if (reg->defaultAnchorTarget) entry.defaultAnchorTargets.push_back(reg->defaultAnchorTarget);
	if (reg->defaultAnchorMode)   entry.defaultAnchorModes.push_back(reg->defaultAnchorMode);

	if (!entry.isOverridden) {
		entry.userPriority = reg->defaultPriority;
		entry.userAnchorTargets = entry.defaultAnchorTargets;
		entry.userAnchorModes  = entry.defaultAnchorModes;
	}
	if (reg->fallbackAnchorTarget) {
		entry.defaultAnchorTargets.push_back(reg->fallbackAnchorTarget);
		if (!entry.isOverridden) entry.userAnchorTargets.push_back(reg->fallbackAnchorTarget);
	}
	if (reg->fallbackAnchorMode) {
		entry.defaultAnchorModes.push_back(reg->fallbackAnchorMode);
		if (!entry.isOverridden) entry.userAnchorModes.push_back(reg->fallbackAnchorMode);
	}

	entry.isRegistered = true;
}

static void OnIIF_F4SEMessage(F4SE::MessagingInterface::Message* a_msg) {
	if (!a_msg) return;
	TryInstallImGuiEditor("f4se_message");
	RegisterScaleformTranslations("f4se_message");
	if (a_msg->type == F4SE::MessagingInterface::kGameLoaded) {
		IIF_API::IIF_Interface api;
		api.RegisterCPPCard = RegisterCPPCard;
		api.RegisterDamageModifier = IIF::CombatBus::RegisterDamageModifier;
		api.RegisterArmorModifier = IIF::CombatBus::RegisterArmorModifier;

		F4SE::GetMessagingInterface()->Dispatch(IIF_API::kMessage_ExchangeInterface, &api, sizeof(api), nullptr);
		REX::INFO("[IIF] 注册表接口与战斗总线已在 kGameLoaded 安全广播！等待附属模组挂载！");
	}
}

namespace OGSupport {
	static F4SE::Impl::F4SEInterface RestoreLoadInterface;
	[[nodiscard]] inline static const char* F4SEAPI F4SEGetSaveFolderName() noexcept { return "Fallout4"; }
	void Init(const F4SE::LoadInterface* a_f4se) {
		if (a_f4se->RuntimeVersion() <= F4SE::RUNTIME_1_10_163) {
			memcpy(&RestoreLoadInterface, a_f4se, 48);
			(((F4SE::Impl::F4SEInterface*)(&RestoreLoadInterface))->GetSaveFolderName) = F4SEGetSaveFolderName;
			F4SE::Init((const F4SE::LoadInterface*)(&RestoreLoadInterface));
		}
		else F4SE::Init(a_f4se);
	}
}

extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info) {
	if (!a_f4se || !a_info) return false;
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = PLUGIN_NAME;
	a_info->version = PLUGIN_VERSION_MAJOR;
	if (a_f4se->IsEditor()) return false;
	return true;
}

static bool Initialize(const F4SE::LoadInterface* a_f4se) {
	static std::once_flag once;
	std::call_once(once, [&]() {
		OGSupport::Init(a_f4se);
		REL::GetTrampoline().create(128);
		IIF::L10n::LoadLanguage();
		IIF::JsonReader::LoadConfigs();

		// CombatBus installation
		IIF::CombatBus::Install();
		IIF::Hooks::Install();
		TryInstallImGuiEditor("plugin_load");

		if (auto msg = F4SE::GetMessagingInterface()) {
			msg->RegisterListener(OnIIF_F4SEMessage);
		}
		});
	return true;
}

extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se) { return Initialize(a_f4se); }


