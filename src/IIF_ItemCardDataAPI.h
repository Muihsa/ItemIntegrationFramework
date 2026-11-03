#pragma once
#include <cstdint>

namespace IIF_API
{
    constexpr std::uint32_t kMessage_ItemCardDataReady = 0x49494602;
    constexpr std::uint32_t kItemCardDataVersion = 1;

    enum class ItemCardValueType : std::uint32_t {
        kNone = 0,
        kNumber = 1,
        kString = 2,
    };

    struct ItemCardValueSnapshot {
        ItemCardValueType type{ ItemCardValueType::kNone };
        double number{ 0.0 };
        const char* text{ nullptr };
    };

    enum ItemCardDataFlags : std::uint32_t {
        kItemCardDataFlag_Injected = 1u << 0,
    };

    struct ItemCardDataEntry {
        const char* id{ nullptr };
        const char* label{ nullptr };
        const char* valueText{ nullptr };
        const char* val1{ nullptr };
        const char* val2{ nullptr };
        ItemCardValueSnapshot originalValue{};
        ItemCardValueSnapshot finalValue{};
        std::int32_t displayType{ 0 };
        std::uint32_t flags{ 0 };
    };

    struct ItemCardDataMessage {
        std::uint32_t structSize{ 0 };
        std::uint32_t version{ kItemCardDataVersion };
        void* itemForm{ nullptr };
        void* inventoryItem{ nullptr };
        void* instanceData{ nullptr };
        const ItemCardDataEntry* entries{ nullptr };
        std::uint32_t stackIndex{ 0 };
        std::uint32_t itemHandleID{ 0 };
        std::uint32_t entryCount{ 0 };
        std::uint32_t entrySize{ sizeof(ItemCardDataEntry) };
    };
}
