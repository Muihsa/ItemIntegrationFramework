#include "pch.h"
#include "UIHooks.h"
#include "JsonReader.h"
#include "IIF_API.h"
#include "ItemCardData.h"
#include "InlineDamageFormat.h"

#include <cstdio>
#include <cstdlib>

#define XBYAK_NO_OP_NAMES 
#include <xbyak/xbyak.h>

namespace IIF::Hooks
{
	using namespace RE;

	REL::Relocation<void**> g_itemMenuDataMgr{ RE::ID::BGSInventoryInterface::Singleton };

	using GetSelectedItem_t = BGSInventoryItem * (*)(void* instance, std::uint32_t& handleID);
	REL::Relocation<GetSelectedItem_t> GetSelectedItem_Internal{ RE::ID::BGSInventoryInterface::RequestInventoryItem };

	BGSInventoryItem* GetSelectedItem(std::uint32_t& handleID) {
		if (g_itemMenuDataMgr.get() && *g_itemMenuDataMgr.get()) return GetSelectedItem_Internal(*g_itemMenuDataMgr.get(), handleID);
		return nullptr;
	}

	BSTArray<PipboyObject*>* GetPipboyInv() {
		auto* mgr = PipboyDataManager::GetSingleton();
		if (!mgr) return nullptr;
		return &mgr->inventoryData.sortedItems;
	}

	struct PipboyArray : public RE::PipboyValue { RE::BSTArray<RE::PipboyValue*> elements; };
	struct ScaleArgs { Scaleform::GFx::Value* res; Scaleform::GFx::Movie* mov; Scaleform::GFx::Value* ths; Scaleform::GFx::Value* unk; Scaleform::GFx::Value* args; std::uint32_t numArgs; std::uint32_t pad2C; std::uint32_t optionID; };

	std::vector<InternalCard> g_pendingCards;
	InlineDamageRequest g_inlineDamage;

	// 👑 保留：存放外部 Mod 发来的修改请求
	struct ModRequest {
		std::string targetID;
		double multiplier;
	};
	std::vector<ModRequest> g_pendingMods;

	static void ReceiverCallback(void* context, const IIF_API::CardRequest* req) {
		(void)context;
		if (!req) return;
		InternalCard ic{};
		if (req->text) ic.text = req->text;

		auto it = JsonReader::g_cppOverrides.find(ic.text);
		if (it != JsonReader::g_cppOverrides.end()) {
			ic.priority = it->second.userPriority;
			ic.anchorTargets = it->second.userAnchorTargets;
			ic.anchorModes = it->second.userAnchorModes;
		}
		else {
			ic.priority = 800; ic.anchorTargets.push_back("BOTTOM"); ic.anchorModes.push_back("after");
		}

		if (req->label) ic.label = req->label;
		ic.highlightLabel = req->highlightLabel;
		if (req->value) ic.value = req->value;
		ic.showDifference = !req->hideDifference; ic.invertDiffColor = req->invertDiffColor;
		ic.displayType = req->displayType;
		ic.hasBackground = req->hasBackground; ic.backgroundColor = req->backgroundColor;
		if (req->sortValueFrom) ic.sortValueFrom = req->sortValueFrom;
		ic.fillPct = req->fillPct; ic.shieldPct = req->shieldPct; ic.fillColor = req->fillColor;
		ic.thresholdPct = req->thresholdPct; ic.thresholdPct2 = req->thresholdPct2; ic.thresholdColor = req->thresholdColor;
		ic.showBar = req->showBar; ic.showValue = req->showValue;
		if (req->valueText) ic.valueText = req->valueText;
		if (req->valueAlign) ic.valueAlign = req->valueAlign;
		ic.valueColor = req->valueColor; ic.valueStandard = req->valueStandard; ic.valueBad = req->valueBad; ic.valueGood = req->valueGood;
		if (req->icon1) ic.icon1 = req->icon1; ic.icon1IsText = req->icon1IsText;
		if (req->val1) ic.val1 = req->val1; ic.val1Bad = req->val1Bad; ic.val1Good = req->val1Good; ic.val1Standard = req->val1Standard;
		if (req->align1) ic.align1 = req->align1;
		if (req->icon2) ic.icon2 = req->icon2; ic.icon2IsText = req->icon2IsText;
		if (req->val2) ic.val2 = req->val2; ic.val2Bad = req->val2Bad; ic.val2Good = req->val2Good; ic.val2Standard = req->val2Standard;
		if (req->align2) ic.align2 = req->align2;
		ic.hasRawValue = false; ic.hasDifference = false;
		g_pendingCards.push_back(ic);
	}

	// 👑 保留：接收其他 Mod 传来的打折请求
	static void ModifierCallback(void* context, const char* targetID, double multiplier) {
		(void)context;
		if (targetID) g_pendingMods.push_back({ targetID, multiplier });
	}

	ItemCardData::ValueSnapshot ReadValueSnapshot(const Scaleform::GFx::Value& a_card) {
		ItemCardData::ValueSnapshot result;
		if (!a_card.IsObject()) return result;

		Scaleform::GFx::Value value;
		if (!a_card.GetMember("value", &value)) return result;
		if (value.IsNumber()) {
			result.type = IIF_API::ItemCardValueType::kNumber;
			result.number = value.GetNumber();
		}
		else if (value.IsInt()) {
			result.type = IIF_API::ItemCardValueType::kNumber;
			result.number = static_cast<double>(value.GetInt());
		}
		else if (value.IsUInt()) {
			result.type = IIF_API::ItemCardValueType::kNumber;
			result.number = static_cast<double>(value.GetUInt());
		}
		else if (value.IsString()) {
			result.type = IIF_API::ItemCardValueType::kString;
			if (const char* text = value.GetString()) result.text = text;
		}
		return result;
	}

	std::string ReadStringMember(const Scaleform::GFx::Value& a_card, const char* a_name) {
		if (!a_card.IsObject()) return {};
		Scaleform::GFx::Value value;
		if (!a_card.GetMember(a_name, &value) || !value.IsString()) return {};
		const char* text = value.GetString();
		return text ? text : "";
	}

	std::int32_t ReadDisplayType(const Scaleform::GFx::Value& a_card) {
		if (!a_card.IsObject()) return 0;
		Scaleform::GFx::Value value;
		if (!a_card.GetMember("displayType", &value)) return 0;
		if (value.IsInt()) return value.GetInt();
		if (value.IsUInt()) return static_cast<std::int32_t>(value.GetUInt());
		if (value.IsNumber()) return static_cast<std::int32_t>(value.GetNumber());
		return 0;
	}

	bool IsInjectedCard(const Scaleform::GFx::Value& a_card) {
		if (!a_card.IsObject()) return false;
		Scaleform::GFx::Value value;
		return a_card.GetMember("IIF_Injected", &value) && value.IsBoolean() && value.GetBoolean();
	}

	void BroadcastItemCardData(const IIF_API::UpdateMessage& a_update, const std::vector<ItemCardData::Entry>& a_snapshots) {
		if (a_snapshots.empty()) return;
		auto messaging = F4SE::GetMessagingInterface();
		if (!messaging) return;

		auto entries = ItemCardData::BindEntries(a_snapshots);
		IIF_API::ItemCardDataMessage message;
		message.structSize = sizeof(message);
		message.itemForm = a_update.itemForm;
		message.inventoryItem = a_update.inventoryItem;
		message.instanceData = a_update.instanceData;
		message.entries = entries.data();
		message.stackIndex = a_update.stackIndex;
		message.itemHandleID = a_update.itemHandleID;
		message.entryCount = static_cast<std::uint32_t>(entries.size());
		message.entrySize = sizeof(IIF_API::ItemCardDataEntry);
		messaging->Dispatch(IIF_API::kMessage_ItemCardDataReady, &message, sizeof(message), nullptr);
	}

	bool TryReadCardNumber(Scaleform::GFx::Value& a_card, double& a_out)
	{
		if (!a_card.IsObject()) return false;
		Scaleform::GFx::Value value;
		if (!a_card.GetMember("value", &value)) return false;
		if (value.IsNumber()) { a_out = value.GetNumber(); return true; }
		if (value.IsInt()) { a_out = static_cast<double>(value.GetInt()); return true; }
		if (value.IsUInt()) { a_out = static_cast<double>(value.GetUInt()); return true; }
		if (value.IsString()) {
			const char* text = value.GetString();
			if (!text || !*text) return false;
			char* end = nullptr;
			const double parsed = std::strtod(text, &end);
			if (end == text) return false;
			a_out = parsed;
			return true;
		}
		return false;
	}

	bool IsDamageCardId(const std::string& a_id)
	{
		return a_id == "$dmg" || a_id == "Damage" || a_id == "DAMAGE" || a_id == "伤害";
	}

	// 只广播的卡片不会插入信息卡列表，这里按与注入卡相同的字段口径整理成广播条目。
	ItemCardData::ValueSnapshot ReadBroadcastOnlyValue(const InternalCard& a_card) {
		ItemCardData::ValueSnapshot result;
		if (a_card.hasRawValue) {
			result.type = IIF_API::ItemCardValueType::kNumber;
			result.number = a_card.rawValue;
		}
		else if (!a_card.value.empty()) {
			result.type = IIF_API::ItemCardValueType::kString;
			result.text = a_card.value;
		}
		return result;
	}

	void AppendBroadcastOnlyEntries(std::vector<ItemCardData::Entry>& a_snapshots, const std::vector<InternalCard>& a_cards) {
		for (const auto& card : a_cards) {
			ItemCardData::Entry entry;
			entry.id = card.text;
			entry.label = card.label;
			entry.valueText = card.valueText;
			entry.val1 = card.val1;
			entry.val2 = card.val2;
			entry.originalValue = ReadBroadcastOnlyValue(card);
			entry.finalValue = entry.originalValue;
			// 与可见卡片一样，其它 Mod 的 modifyCard 请求也作用在广播数值上。
			if (entry.finalValue.type == IIF_API::ItemCardValueType::kNumber) {
				for (const auto& mod : g_pendingMods) {
					if (!mod.targetID.empty() && entry.id == mod.targetID) {
						entry.finalValue.number *= mod.multiplier;
					}
				}
			}
			entry.displayType = card.displayType;
			entry.flags = IIF_API::kItemCardDataFlag_BroadcastOnly;
			a_snapshots.push_back(std::move(entry));
		}
	}

	void RebuildUIArray(Scaleform::GFx::Value& a_array, std::uint32_t newCardCount, const IIF_API::UpdateMessage& a_update, const std::vector<InternalCard>& a_broadcastOnly) {
		std::uint32_t totalSize = a_array.GetArraySize();
		if (totalSize < newCardCount) {
			// 数组形状异常时不动 UI，但只广播的条目仍要发给监听方。
			std::vector<ItemCardData::Entry> snapshots;
			AppendBroadcastOnlyEntries(snapshots, a_broadcastOnly);
			BroadcastItemCardData(a_update, snapshots);
			return;
		}
		std::uint32_t origSize = totalSize - newCardCount;

		// ==========================================
		// 👇 下面这部分完全是你的原版，原汁原味！
		// ==========================================
		std::vector<Scaleform::GFx::Value> vanillaCards;
		for (std::uint32_t i = 0; i < origSize; ++i) {
			Scaleform::GFx::Value e;
			if (a_array.GetMember(std::to_string(i).c_str(), &e)) vanillaCards.push_back(e);
		}

		std::vector<Scaleform::GFx::Value> injectedCards;
		for (std::uint32_t i = origSize; i < totalSize; ++i) {
			Scaleform::GFx::Value e;
			if (a_array.GetMember(std::to_string(i).c_str(), &e)) injectedCards.push_back(e);
		}

		auto GetCardID = [](Scaleform::GFx::Value& vc) -> std::string {
			if (!vc.IsObject()) return "";
			Scaleform::GFx::Value dmgType;
			if (vc.GetMember("damageType", &dmgType)) {
				int dt = -1;
				if (dmgType.IsNumber()) dt = static_cast<int>(dmgType.GetNumber());
				else if (dmgType.IsInt()) dt = dmgType.GetInt();
				else if (dmgType.IsUInt()) dt = static_cast<int>(dmgType.GetUInt());
				if (dt == 10) return "$ammo";
			}
			Scaleform::GFx::Value txtVal;
			if (vc.GetMember("text", &txtVal) && txtVal.IsString()) {
				return txtVal.GetString();
			}
			return "";
			};

		struct Zone {
			std::vector<size_t> before;
			std::vector<size_t> replace;
			std::vector<size_t> after;
		};
		std::unordered_map<std::string, Zone> zones;

		for (size_t i = 0; i < g_pendingCards.size(); ++i) {
			const auto& ic = g_pendingCards[i];
			std::string activeTarget = "BOTTOM";
			std::string activeMode = "after";

			for (size_t tIdx = 0; tIdx < ic.anchorTargets.size(); ++tIdx) {
				std::string target = ic.anchorTargets[tIdx];
				std::string mode = "after";
				if (!ic.anchorModes.empty()) mode = (tIdx < ic.anchorModes.size()) ? ic.anchorModes[tIdx] : ic.anchorModes.back();

				bool valid = false;
				if (target == "TOP" || target == "BOTTOM") valid = true;
				else {
					for (auto& vc : vanillaCards) {
						if (target == GetCardID(vc)) { valid = true; break; }
					}
					if (!valid) {
						for (auto& pc : g_pendingCards) { if (pc.text == target) { valid = true; break; } }
					}
				}
				if (valid) { activeTarget = target; activeMode = mode; break; }
			}

			if (activeMode == "before") zones[activeTarget].before.push_back(i);
			else if (activeMode == "replace") zones[activeTarget].replace.push_back(i);
			else zones[activeTarget].after.push_back(i);
		}

		auto sortByPriority = [](size_t a, size_t b) { return g_pendingCards[a].priority < g_pendingCards[b].priority; };
		for (auto& pair : zones) {
			std::sort(pair.second.before.begin(), pair.second.before.end(), sortByPriority);
			std::sort(pair.second.replace.begin(), pair.second.replace.end(), sortByPriority);
			std::sort(pair.second.after.begin(), pair.second.after.end(), sortByPriority);
		}

		std::vector<Scaleform::GFx::Value> finalLayout;
		std::unordered_set<std::string> visitedZones;

		std::function<void(const std::string&, Scaleform::GFx::Value*)> AppendZone = [&](const std::string& targetId, Scaleform::GFx::Value* vCard) {
			bool hasValidId = !targetId.empty();
			auto it = hasValidId ? zones.find(targetId) : zones.end();
			bool firstVisit = hasValidId && !visitedZones.count(targetId);

			if (vCard == nullptr) {
				// 递归访问 zone 链（展开 before/replace/after 注入卡）：已访问过就别重复展开
				if (!firstVisit) return;
				visitedZones.insert(targetId);
				if (it != zones.end()) {
					for (size_t idx : it->second.before) {
						finalLayout.push_back(injectedCards[idx]);
						AppendZone(g_pendingCards[idx].text, nullptr);
					}
					for (size_t idx : it->second.replace) {
						finalLayout.push_back(injectedCards[idx]);
						AppendZone(g_pendingCards[idx].text, nullptr);
					}
					for (size_t idx : it->second.after) {
						finalLayout.push_back(injectedCards[idx]);
						AppendZone(g_pendingCards[idx].text, nullptr);
					}
				}
				return;
			}

			// vCard != nullptr：主循环传进来一张 vanilla 卡
			if (firstVisit && it != zones.end()) {
				visitedZones.insert(targetId);
				for (size_t idx : it->second.before) {
					finalLayout.push_back(injectedCards[idx]);
					AppendZone(g_pendingCards[idx].text, nullptr);
				}
				if (!it->second.replace.empty()) {
					// replace 模式：用注入卡替代第一张匹配 vanilla 卡，不 push vCard
					for (size_t idx : it->second.replace) {
						finalLayout.push_back(injectedCards[idx]);
						AppendZone(g_pendingCards[idx].text, nullptr);
					}
				}
				else {
					finalLayout.push_back(*vCard);
				}
				for (size_t idx : it->second.after) {
					finalLayout.push_back(injectedCards[idx]);
					AppendZone(g_pendingCards[idx].text, nullptr);
				}
			}
			else {
				// 同 ID 的第二/第三张 vanilla 卡（能量武器多伤害的 Energy/Cryo/Rad $dmg 行等），
				// 或者根本没挂任何注入卡的 vanilla 卡 —— 无论哪种都要原样保留
				finalLayout.push_back(*vCard);
			}
			};

		bool topAppended = false;
		for (auto& vc : vanillaCards) {
			std::string vId = GetCardID(vc);
			if (vId == "$dmg" || vId == "Damage" || vId == "DAMAGE" || vId == "$Melee" || vId == "伤害" || vId == "$Armor") {
				AppendZone(vId, &vc);
			}
			else {
				if (!topAppended) {
					AppendZone("TOP", nullptr);
					topAppended = true;
				}
				AppendZone(vId, &vc);
			}
		}
		if (!topAppended) AppendZone("TOP", nullptr);
		AppendZone("BOTTOM", nullptr);

		// ========================================================
		// 👑 【绝对物理隔离嵌入点】：在这里，你的锚点映射(zones)已经全部原样完成！
		// finalLayout里已经是排好序的终极卡片列表了！
		// 我们在这里修改数值，绝对不可能干扰到你前面的任何排序提取！
		// ========================================================
		std::vector<ItemCardData::Entry> snapshots;
		snapshots.reserve(finalLayout.size());
		bool inlineApplied = false;
		for (auto& e : finalLayout) {
			ItemCardData::Entry snapshot;
			snapshot.id = GetCardID(e);
			snapshot.originalValue = ReadValueSnapshot(e);
			for (const auto& mod : g_pendingMods) {
				if (!mod.targetID.empty() && snapshot.id == mod.targetID) {
					Scaleform::GFx::Value valObj;
					if (e.GetMember("value", &valObj)) {
						double num = 0.0;
						bool validNum = false;
						if (valObj.IsNumber()) { num = valObj.GetNumber(); validNum = true; }
						else if (valObj.IsInt()) { num = static_cast<double>(valObj.GetInt()); validNum = true; }
						else if (valObj.IsUInt()) { num = static_cast<double>(valObj.GetUInt()); validNum = true; }

						if (validNum) {
							e.SetMember("value", Scaleform::GFx::Value(static_cast<std::int32_t>(num * mod.multiplier)));
						}
					}
				}
			}

			// NumProjectiles 植入显示：把原版伤害行改写成「单发伤害 x 弹丸数」。
			// 只动显示文本 valueText，value 数字保持原版总伤害，红绿差值仍按总伤害比较。
			if (!inlineApplied && g_inlineDamage.active && g_inlineDamage.projectiles > 1.0f && IsDamageCardId(snapshot.id)) {
				double totalDamage = 0.0;
				if (TryReadCardNumber(e, totalDamage) && totalDamage > 0.0) {
					const std::string inlineText = InlineDamage::Build(g_inlineDamage.format, totalDamage, static_cast<double>(g_inlineDamage.projectiles), g_inlineDamage.roundDigits);
					if (!inlineText.empty()) {
						e.SetMember("valueText", Scaleform::GFx::Value(inlineText.c_str()));
						inlineApplied = true;
					}
				}
			}

			snapshot.finalValue = ReadValueSnapshot(e);
			snapshot.label = ReadStringMember(e, "label");
			snapshot.valueText = ReadStringMember(e, "valueText");
			snapshot.val1 = ReadStringMember(e, "val1");
			snapshot.val2 = ReadStringMember(e, "val2");
			snapshot.displayType = ReadDisplayType(e);
			if (IsInjectedCard(e)) snapshot.flags |= IIF_API::kItemCardDataFlag_Injected;
			snapshots.push_back(std::move(snapshot));
		}

		AppendBroadcastOnlyEntries(snapshots, a_broadcastOnly);

		BroadcastItemCardData(a_update, snapshots);
		if (!g_pendingCards.empty() || !g_pendingMods.empty() || inlineApplied) {
			a_array.RemoveElements(0, totalSize);
			for (auto& e : finalLayout) { a_array.PushBack(e); }
		}
	}

	void DispatchAndInject(Scaleform::GFx::Movie* movie, Scaleform::GFx::Value& a_array, RE::TESForm* form, RE::BGSInventoryItem* item, RE::TBO_InstanceData* instance, std::uint32_t stackIdx, std::uint32_t handleID) {
		g_pendingCards.clear();
		g_pendingMods.clear(); // 👑 清空历史修改记录
		g_inlineDamage = InlineDamageRequest{};

		IIF_API::UpdateMessage msg;
		msg.itemForm = form; msg.inventoryItem = item; msg.instanceData = instance; msg.gfxArray = &a_array;
		msg.movie = movie; msg.stackIndex = stackIdx; msg.itemHandleID = handleID; msg.context = nullptr;
		msg.addCard = ReceiverCallback;
		msg.modifyCard = ModifierCallback; // 👑 开放修改回调

		auto messaging = F4SE::GetMessagingInterface();
		if (messaging) messaging->Dispatch(IIF_API::kMessage_UpdateItemCard, &msg, sizeof(msg), nullptr);

		IIF::JsonReader::EvaluateItemRules(form, item, instance, stackIdx);

		// 只广播的规则完全不进入信息卡：先从待注入列表里筛出来，单独走广播通道。
		std::vector<InternalCard> broadcastOnlyCards;
		if (!g_pendingCards.empty()) {
			auto firstBroadcastOnly = std::remove_if(g_pendingCards.begin(), g_pendingCards.end(), [&](const InternalCard& a_card) {
				if (!a_card.broadcastOnly) return false;
				broadcastOnlyCards.push_back(a_card);
				return true;
			});
			g_pendingCards.erase(firstBroadcastOnly, g_pendingCards.end());
		}

		std::uint32_t newCardCount = static_cast<std::uint32_t>(g_pendingCards.size());
		for (const auto& card : g_pendingCards) {
			Scaleform::GFx::Value reqEntry;
			Scaleform::GFx::Value valObj;
			if (card.hasRawValue) valObj = card.rawValue; else if (!card.value.empty()) valObj = card.value.c_str();

			InventoryUserUIUtils::AddItemCardInfoEntry(a_array, reqEntry, card.text.c_str(), valObj);

			if (card.showDifference && card.hasDifference) reqEntry.SetMember("difference", Scaleform::GFx::Value(card.difference));

			reqEntry.SetMember("IIF_Injected", Scaleform::GFx::Value(true));
			if (!card.label.empty()) reqEntry.SetMember("label", Scaleform::GFx::Value(card.label.c_str()));
			if (!card.suffix.empty()) reqEntry.SetMember("suffix", Scaleform::GFx::Value(card.suffix.c_str()));
			if (!card.valueText.empty()) reqEntry.SetMember("valueText", Scaleform::GFx::Value(card.valueText.c_str()));
			if (!card.sortValueFrom.empty()) reqEntry.SetMember("sortValueFrom", Scaleform::GFx::Value(card.sortValueFrom.c_str()));

			reqEntry.SetMember("highlightLabel", Scaleform::GFx::Value(card.highlightLabel));
			reqEntry.SetMember("invertDiffColor", Scaleform::GFx::Value(card.invertDiffColor));
			reqEntry.SetMember("valueStandard", Scaleform::GFx::Value(card.valueStandard));
			reqEntry.SetMember("displayType", Scaleform::GFx::Value(card.displayType));
			reqEntry.SetMember("hasBackground", Scaleform::GFx::Value(card.hasBackground));

			if (card.hasBackground && card.backgroundColor != 0)
				reqEntry.SetMember("backgroundColor", Scaleform::GFx::Value(card.backgroundColor));

			if (card.displayType == 1) {
				reqEntry.SetMember("fillPct", Scaleform::GFx::Value(card.fillPct));
				reqEntry.SetMember("shieldPct", Scaleform::GFx::Value(card.shieldPct));
				reqEntry.SetMember("fillColor", Scaleform::GFx::Value(card.fillColor));
				if (card.thresholdPct >= 0.0f) reqEntry.SetMember("thresholdPct", Scaleform::GFx::Value(card.thresholdPct));
				if (card.thresholdPct2 >= 0.0f) reqEntry.SetMember("thresholdPct2", Scaleform::GFx::Value(card.thresholdPct2));
				if (card.thresholdColor != 0) reqEntry.SetMember("thresholdColor", Scaleform::GFx::Value(card.thresholdColor));
				reqEntry.SetMember("showBar", Scaleform::GFx::Value(card.showBar));
				reqEntry.SetMember("showValue", Scaleform::GFx::Value(card.showValue));
				if (!card.valueAlign.empty()) reqEntry.SetMember("valueAlign", Scaleform::GFx::Value(card.valueAlign.c_str()));
				if (card.valueColor != 0) reqEntry.SetMember("valueColor", Scaleform::GFx::Value(card.valueColor));
				reqEntry.SetMember("valBad", Scaleform::GFx::Value(card.valueBad));
				reqEntry.SetMember("valGood", Scaleform::GFx::Value(card.valueGood));
			}
			else if (card.displayType == 2) {
				reqEntry.SetMember("icon1", Scaleform::GFx::Value(card.icon1.c_str()));
				reqEntry.SetMember("icon1IsText", Scaleform::GFx::Value(card.icon1IsText));
				reqEntry.SetMember("val1", Scaleform::GFx::Value(card.val1.c_str()));
				reqEntry.SetMember("val1Bad", Scaleform::GFx::Value(card.val1Bad));
				reqEntry.SetMember("val1Good", Scaleform::GFx::Value(card.val1Good));
				reqEntry.SetMember("val1Standard", Scaleform::GFx::Value(card.val1Standard));
				if (!card.align1.empty()) reqEntry.SetMember("align1", Scaleform::GFx::Value(card.align1.c_str()));

				reqEntry.SetMember("icon2", Scaleform::GFx::Value(card.icon2.c_str()));
				reqEntry.SetMember("icon2IsText", Scaleform::GFx::Value(card.icon2IsText));
				reqEntry.SetMember("val2", Scaleform::GFx::Value(card.val2.c_str()));
				reqEntry.SetMember("val2Bad", Scaleform::GFx::Value(card.val2Bad));
				reqEntry.SetMember("val2Good", Scaleform::GFx::Value(card.val2Good));
				reqEntry.SetMember("val2Standard", Scaleform::GFx::Value(card.val2Standard));
				if (!card.align2.empty()) reqEntry.SetMember("align2", Scaleform::GFx::Value(card.align2.c_str()));
			}
		}
		RebuildUIArray(a_array, newCardCount, msg, broadcastOnlyCards);
	}

	using PBInv_t = void(*)(void*, ScaleArgs*); static PBInv_t _PBInv = nullptr;
	void PBInv_Hook(void* t, ScaleArgs* a) {
		_PBInv(t, a);
		if (a->optionID == 0xD && a->numArgs >= 2 && a->args[1].IsArray()) {
			std::uint32_t idx = a->args[0].IsInt() ? a->args[0].GetInt() : (a->args[0].IsUInt() ? a->args[0].GetUInt() : static_cast<std::uint32_t>(a->args[0].GetNumber()));
			auto inv = GetPipboyInv();
			if (inv && idx < inv->size()) {
				auto hd = (*inv)[idx]; if (!hd) return;
				auto it = hd->memberMap.find("handleID");
				if (it != hd->memberMap.end() && it->second) {
					std::uint32_t hID = *reinterpret_cast<std::uint32_t*>(reinterpret_cast<uintptr_t>(it->second) + 0x18);
					if (auto item = GetSelectedItem(hID); item && item->object) {
						std::uint32_t sID = 0; auto sIt = hd->memberMap.find("StackID");
						if (sIt != hd->memberMap.end() && sIt->second) {
							auto arr = static_cast<PipboyArray*>(sIt->second);                        // 🛡️ 防护：elements[0] 可能为 nullptr，读取前必须检查
                        if (arr && arr->elements.size() > 0 && arr->elements[0]) sID = *reinterpret_cast<std::uint32_t*>(reinterpret_cast<uintptr_t>(arr->elements[0]) + 0x18);
						}
						auto inst = item->GetInstanceData(sID);
						DispatchAndInject(a->mov, a->args[1], item->object, item, inst, sID, hID);
					}
				}
			}
		}
	}

	using Pop_t = void(*)(Scaleform::GFx::Value*, RE::BGSInventoryItem*, std::uint32_t, void*, void*, void*);
	static Pop_t _Pop = nullptr;
	void Pop_Hook(Scaleform::GFx::Value* arr, RE::BGSInventoryItem* it, std::uint32_t sID, void* a4, void* a5, void* a6) {
		_Pop(arr, it, sID, a4, a5, a6);
		if (!arr || !it || !it->object) return;
		Scaleform::GFx::Value* tgt = arr; Scaleform::GFx::Value tmp;
		if (!arr->IsArray() && arr->HasMember("ItemCardInfoList")) {
			arr->GetMember("ItemCardInfoList", &tmp);
			if (tmp.IsArray()) tgt = &tmp; else return;
		}
		auto inst = it->GetInstanceData(sID);
		DispatchAndInject(nullptr, *tgt, it->object, it, inst, sID, 0);
	}

	void Install() {
		auto& tramp = REL::GetTrampoline();
		REL::Relocation<uintptr_t> pbV{ RE::VTABLE::PipboyMenu[0] };
		_PBInv = reinterpret_cast<PBInv_t>(pbV.write_vfunc(1, reinterpret_cast<uintptr_t>(PBInv_Hook)));

		REL::Relocation<uintptr_t> tgt{ RE::ID::InventoryUserUIUtils::PopulateItemCardInfo_Helper };
		uintptr_t hookAddress = tgt.address();
		uint8_t* ptr = reinterpret_cast<uint8_t*>(hookAddress);

		if (ptr[0] == 0xE9) {
			int32_t rel32 = *reinterpret_cast<int32_t*>(ptr + 1);
			uintptr_t otherModHook = hookAddress + 5 + rel32;
			_Pop = reinterpret_cast<Pop_t>(otherModHook);
			tramp.write_jmp<5>(hookAddress, reinterpret_cast<uintptr_t>(Pop_Hook));
		}
		else {
			void* buf = tramp.allocate(64);
			struct Code : Xbyak::CodeGenerator {
				Code(void* b, uintptr_t ret) : Xbyak::CodeGenerator(64, b) {
					mov(rax, rsp);
					// 👑 还原你一开始原版的 Xbyak 指令
					mov(ptr[rax + 0x20], r9);
					mov(r11, ret);
					jmp(r11);
				}
			};
			Code code(buf, hookAddress + 7);
			_Pop = reinterpret_cast<Pop_t>(buf);

			tramp.write_jmp<5>(hookAddress, reinterpret_cast<uintptr_t>(Pop_Hook));

			// 👑 安全的 NOP 填充，代替已不存在的 REL::safe_fill
			DWORD oldProtect;
			VirtualProtect(reinterpret_cast<void*>(hookAddress + 5), 2, PAGE_EXECUTE_READWRITE, &oldProtect);
			reinterpret_cast<uint8_t*>(hookAddress + 5)[0] = 0x90;
			reinterpret_cast<uint8_t*>(hookAddress + 5)[1] = 0x90;
			VirtualProtect(reinterpret_cast<void*>(hookAddress + 5), 2, oldProtect, &oldProtect);
		}
	}
}
