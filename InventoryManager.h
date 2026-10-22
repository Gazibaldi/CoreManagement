/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <array>
#include <string>
#include "..\CoreSDK\inc\natives.h"
#include "CMTypes.h"

Hash GetKey(const char* key);

struct RDR_GUID
{
    alignas(8) int data1;
    alignas(8) int data2;
    alignas(8) int data3;
    alignas(8) int data4;

    auto operator<=>(const RDR_GUID&) const = default;

    bool IsInvalid() const {
        return data1 == 0 && data2 == 0 && data3 == 0 && data4 == 0;
    }

	std::string ToString() const {
		return std::to_string(data1) + "-" + std::to_string(data2) + "-" + std::to_string(data3) + "-" + std::to_string(data4);
	}
};

struct RDR_SLOTINFO
{
    alignas(8) RDR_GUID guid;    // 16 bytes: Unique instance tracker
    alignas(8) int unkFlags;     // 4 bytes: State flags / Pointer metadata
    alignas(8) int stateData;    // 4 bytes: Quality, condition, or decay state
    alignas(8) int trackingHash; // 4 bytes: Run-time hash map identifier
    alignas(8) Hash slotId;       // 4 bytes: The Joaat Hash of the slot (e.g., SLOTID_SATCHEL)

    bool IsInvalid() const {
        return guid.IsInvalid() && unkFlags == 0 && stateData == 0 && trackingHash == 0 && slotId == 0;
    }
};

struct RDR_ITEMINFO
{
    alignas(8) int itemCondition; // 4 bytes: Weapon wear/soot level or item decay modifier
    alignas(8) int stylingMapPtr; // 4 bytes: Runtime pointer index to cosmetic modifications
	alignas(8) Hash itemGroup;     // 4 bytes: The Joaat Hash of the item group (e.g., CLOTHING, HORSE, UPGRADE)
    alignas(8) int quantity;      // 4 bytes: Total item/ammo count stacked in this exact slot
    alignas(8) int runtimeFlags;  // 4 bytes: Engine-level operational status bitflags
    alignas(8) int padding1;      // 4 bytes: Reserved structure alignment padding
    alignas(8) int padding2;      // 4 bytes: Reserved structure alignment padding
};

class InventoryManager {
    Hash m_NullSlotHash;
    Hash m_CharacterHash;
    Hash m_ActiveHorseSlotHash;
    Hash m_SatchelSlotHash;
    Hash m_WardrobeSlotHash;
    Hash m_WardrobeContainerHash;

    std::array<PlayerPassiveModifier, 4> m_PlayerPassiveModifiers{};
    std::array<TackModifier, 12> m_StirrupModifiers{};
    std::array<TackModifier, 93> m_SaddleModifiers{};

    [[nodiscard]] RDR_GUID GetPlayerInventoryItemGUID(Hash item, RDR_GUID guid, Hash slotId) noexcept;
    [[nodiscard]] RDR_GUID GetPlayerInventoryGUID() noexcept;
    [[nodiscard]] Hash GetItemGroup(Hash itemHash) noexcept;
    [[nodiscard]] RDR_SLOTINFO GetItemSlotInfo(Hash itemHash) noexcept;

    [[nodiscard]] bool DoesPlayerHaveItem(Hash itemHash, const std::string& itemName) noexcept;
    [[nodiscard]] bool DoesHorseHaveItem(Ped horsePed, Hash itemHash, const std::string& itemName) noexcept;

public:
    void ComputeHashes() noexcept;
    [[nodiscard]] CombinedPlayerCoreModifiers GetActivePlayerPassiveModifiers() noexcept;
    [[nodiscard]] CombinedTackModifiers GetActiveHorseTackModifiers(Ped horsePed) noexcept;

};