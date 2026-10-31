/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#include "..\CoreSDK\inc\types.h"
#include "..\CoreSDK\inc\natives.h"
#include "InventoryManager.h"
#include "Logger.h"
#include "CMTypes.h"
#include "Functions.h"

void InventoryManager::ComputeHashes() noexcept {
    m_NullSlotHash = GetKey("SLOTID_NONE");
    m_CharacterHash = GetKey("CHARACTER");
    m_SatchelSlotHash = GetKey("SLOTID_SATCHEL");
    m_WardrobeContainerHash = GetKey("WARDROBE");
    m_WardrobeSlotHash = GetKey("SLOTID_WARDROBE");

    m_PlayerPassiveModifiers = { {
            { "PROVISION_TRINKET_OWL_FEATHER",        GetKey("PROVISION_TRINKET_OWL_FEATHER"), 0.15f, 0.15f, 0.15f }, // Fits in SLOTID_SATCHEL
            { "PROVISION_TALISMAN_BEAR_CLAW",         GetKey("PROVISION_TALISMAN_BEAR_CLAW"), 0.10f, 0.00f, 0.00f }, // Fits in WARDROBE
            { "PROVISION_TALISMAN_BUFFALO_HORN",      GetKey("PROVISION_TALISMAN_BUFFALO_HORN"), 0.00f, 0.10f, 0.00f }, // Fits in SLOTID_SATCHEL
            { "PROVISION_TALISMAN_ALLIGATOR_TOOTH",   GetKey("PROVISION_TALISMAN_ALLIGATOR_TOOTH"), 0.00f, 0.00f, 0.10f } // Fits in WARDROBE
    } };
}

RDR_GUID InventoryManager::GetPlayerInventoryItemGUID(Hash itemHash, RDR_GUID guid, Hash slotId) noexcept {
    RDR_GUID outGuid{};
    
    INVENTORY::INVENTORY_GET_GUID_FROM_ITEMID(1, (Any*)&guid, itemHash, slotId, (Any*)&outGuid);

	LOG_DEV("Retrieved inventory item GUID for item hash {}: {}", itemHash, outGuid.ToString());
    
    return outGuid;
}

RDR_GUID InventoryManager::GetPlayerInventoryGUID() noexcept {
    RDR_GUID emptyGuid{};

    RDR_GUID invGuid = GetPlayerInventoryItemGUID(m_CharacterHash, emptyGuid, m_NullSlotHash);

    if (invGuid.IsInvalid()) {
		LOG_DEV("Failed to retrieve player inventory GUID.");
		return RDR_GUID{};
    }

	LOG_DEV("Retrieved player inventory GUID: {}", invGuid.ToString());

    return invGuid;
}

Hash InventoryManager::GetItemGroup(Hash itemHash) noexcept {
    RDR_ITEMINFO info{};

    if (!ITEMDATABASE::_ITEMDATABASE_IS_KEY_VALID(itemHash, 0)) {
		LOG_DEV("Item hash is not valid in the item database. Hash: {}", itemHash);
        return 0;
    }

    if (!ITEMDATABASE::ITEMDATABASE_FILLOUT_ITEM_INFO(itemHash, (Any*)&info)) {
		LOG_DEV("Failed to fill out item info for hash: {}", itemHash);
        return 0;
    }

	LOG_DEV("Retrieved item group for hash {}: {}", itemHash, info.itemGroup);
    
    return info.itemGroup;
}

RDR_SLOTINFO InventoryManager::GetItemSlotInfo(Hash itemHash) noexcept {
    RDR_SLOTINFO slotInfo{};

	RDR_GUID invGuid = GetPlayerInventoryGUID();

    if (!invGuid.IsInvalid()) {
        slotInfo.guid = invGuid;
        
        Hash group = GetItemGroup(itemHash);
        switch (group)
        {
		case 0xC2286F01: // CLOTHING (WARDROBE)
            LOG_DEV("Item (hash: {}) fits wardrobe container (hash: {})", itemHash, m_WardrobeContainerHash);
            slotInfo.guid = GetPlayerInventoryItemGUID(m_WardrobeContainerHash, slotInfo.guid, m_WardrobeSlotHash);
            slotInfo.slotId = INVENTORY::_GET_DEFAULT_ITEM_SLOT_INFO(itemHash, m_WardrobeContainerHash);
            break;
		case 0xDE7B48B3: // SATCHEL
            LOG_DEV("Item (hash: {}) fits in satchel slot (hash: {})", itemHash, m_SatchelSlotHash);
            slotInfo.slotId = m_SatchelSlotHash;
            break;
        default:
            break;
        }
    }

    return slotInfo;
}

bool InventoryManager::DoesPlayerHaveItem(Hash itemHash, const std::string& itemName) noexcept {    
    RDR_SLOTINFO slotInfo = GetItemSlotInfo(itemHash);

    if (slotInfo.IsInvalid()) {
		LOG_DEV("Failed to retrieve slot info for item: {} (hash: {})", itemName, itemHash);
        return false;
    }

    RDR_GUID itemGuid = GetPlayerInventoryItemGUID(itemHash, slotInfo.guid, slotInfo.slotId);

    if(itemGuid.IsInvalid()) {
		LOG_DEV("Player does not have item: {} (hash: {}) in their inventory", itemName, itemHash);
        return false;
    }
    else {
        // Sanity check to ensure the GUID is valid before returning
        bool result = INVENTORY::_INVENTORY_IS_GUID_VALID((Any*)&itemGuid);

		if(!result)
            LOG_DEV("Item {} (hash: {}) is in player inventory but did not pass the GUID validity check", itemName, itemHash);
        else
            LOG(LogLevel::Verbose, "Player has item: {} (hash: {}) in their inventory. Adding core passive modifiers", itemName, itemHash);

        return result;
    }
}

[[nodiscard]] CombinedPlayerCoreModifiers InventoryManager::GetActivePlayerPassiveModifiers() noexcept {
    CombinedPlayerCoreModifiers modifiers{};

    for (const auto& passive : m_PlayerPassiveModifiers) {
		LOG(LogLevel::Verbose, "Checking for player passive item: {}", passive.name);
        LOG_DEV("Checking for player passive item: {} (hash: {})", passive.name, passive.itemHash);
        if (DoesPlayerHaveItem(passive.itemHash, std::string(passive.name))) {
            modifiers.totalHealthReduction += passive.healthCoreMod;
            modifiers.totalStaminaReduction += passive.staminaCoreMod;
            modifiers.totalDeadEyeReduction += passive.deadEyeCoreMod;
        }
    }

    return modifiers;
}