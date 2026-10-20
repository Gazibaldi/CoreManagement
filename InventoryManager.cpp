/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#include "InventoryManager.h"
#include "Logger.h"

Hash GetKey(const char* key) {
    Hash hash = MISC::GET_HASH_KEY(key);
    WriteLog(LogLevel::Dev, "Precomputed hash for key '" + std::string(key) + "': " + std::to_string(hash));

    return hash;
}

void InventoryManager::ComputeHashes() noexcept {
    m_NullSlotHash = GetKey("SLOTID_NONE");
    m_CharacterHash = GetKey("CHARACTER");
    
    m_ActiveHorseSlotHash = GetKey("SLOTID_ACTIVE_HORSE");
    m_SatchelSlotHash = GetKey("SLOTID_SATCHEL");
	m_WardrobeSlotHash = GetKey("SLOTID_WARDROBE");
	m_WardrobeContainerHash = GetKey("WARDROBE");
	m_UpgradeSlotHash = GetKey("SLOTID_UPGRADES");

    m_PlayerPassiveModifiers = { {
            { "PROVISION_TRINKET_OWL_FEATHER",          GetKey("PROVISION_TRINKET_OWL_FEATHER"), 0.15f, 0.15f, 0.15f },
            { "PROVISION_TALISMAN_BEAR_CLAW",         GetKey("PROVISION_TALISMAN_BEAR_CLAW"), 0.10f, 0.00f, 0.00f },
            { "PROVISION_TRINKET_BISON_HORN",        GetKey("PROVISION_TRINKET_BISON_HORN"), 0.00f, 0.10f, 0.00f },
            { "PROVISION_TALISMAN_ALLIGATOR_TOOTH",   GetKey("PROVISION_TALISMAN_ALLIGATOR_TOOTH"), 0.00f, 0.00f, 0.10f }
        } };

    m_StirrupModifiers = { {
        { "UPGRADE_HORSE_STIRRUPS_FILLIES",         GetKey("UPGRADE_HORSE_STIRRUPS_FILLIES"), 0.00f, 0.00f },
        { "UPGRADE_HORSE_STIRRUPS_BAROQUE",         GetKey("UPGRADE_HORSE_STIRRUPS_BAROQUE"), 0.00f, 0.05f },
        { "UPGRADE_HORSE_STIRRUPS_SLIM_LINE_IRON",  GetKey("UPGRADE_HORSE_STIRRUPS_SLIM_LINE_IRON"), 0.00f, 0.10f },
        { "UPGRADE_HORSE_STIRRUPS_SLIM_LINE",       GetKey("UPGRADE_HORSE_STIRRUPS_SLIM_LINE"), 0.00f, 0.15f },
        { "UPGRADE_HORSE_STIRRUPS_DEEP_FLANGE",     GetKey("UPGRADE_HORSE_STIRRUPS_DEEP_FLANGE"), 0.00f, 0.20f },
        { "UPGRADE_HORSE_STIRRUPS_IRON_ROUND",      GetKey("UPGRADE_HORSE_STIRRUPS_IRON_ROUND"), 0.00f, 0.25f },
        { "UPGRADE_HORSE_STIRRUPS_OXBOW",           GetKey("UPGRADE_HORSE_STIRRUPS_OXBOW"), 0.00f, 0.30f },
        { "UPGRADE_HORSE_STIRRUPS_OXBOW_TAPADERA",  GetKey("UPGRADE_HORSE_STIRRUPS_OXBOW_TAPADERA"), 0.00f, 0.35f },
        { "UPGRADE_HORSE_STIRRUPS_BELL",            GetKey("UPGRADE_HORSE_STIRRUPS_BELL"), 0.00f, 0.40f },
        { "UPGRADE_HORSE_STIRRUPS_SAFETY",          GetKey("UPGRADE_HORSE_STIRRUPS_SAFETY"), 0.00f, 0.40f },
        { "UPGRADE_HORSE_STIRRUPS_BELL_TAPADERA",   GetKey("UPGRADE_HORSE_STIRRUPS_BELL_TAPADERA"), 0.00f, 0.45f },
        { "UPGRADE_HORSE_STIRRUPS_HOODED",          GetKey("UPGRADE_HORSE_STIRRUPS_HOODED"), 0.00f, 0.50f }
    } };

    m_SaddleModifiers = { {
            // ==========================================
            // STANDARD / UPGRADED STABLE SADDLES
            // ==========================================
            { "SADDLE_KNELLER_DAKOTA_STOCK",            GetKey("SADDLE_KNELLER_DAKOTA_STOCK"), 0.04f, 0.08f },
            { "SADDLE_KNELLER_DAKOTA_IMPROVED",         GetKey("SADDLE_KNELLER_DAKOTA_IMPROVED"), 0.12f, 0.16f },

            { "SADDLE_KNELLER_MOTHER_HUBBARD_STOCK",    GetKey("SADDLE_KNELLER_MOTHER_HUBBARD_STOCK"), 0.06f, 0.04f },
            { "SADDLE_KNELLER_MOTHER_HUBBARD_IMPROVED", GetKey("SADDLE_KNELLER_MOTHER_HUBBARD_IMPROVED"), 0.14f, 0.12f },

            { "SADDLE_LUMLEY_MCLELLAND_STOCK",          GetKey("SADDLE_LUMLEY_MCLELLAND_STOCK"), 0.04f, 0.06f },
            { "SADDLE_LUMLEY_MCLELLAND_IMPROVED",       GetKey("SADDLE_LUMLEY_MCLELLAND_IMPROVED"), 0.14f, 0.18f },

            { "SADDLE_LUMLEY_RANCH_CUTTER_STOCK",       GetKey("SADDLE_LUMLEY_RANCH_CUTTER_STOCK"), 0.08f, 0.04f },
            { "SADDLE_LUMLEY_RANCH_CUTTER_IMPROVED",    GetKey("SADDLE_LUMLEY_RANCH_CUTTER_IMPROVED"), 0.14f, 0.12f },

            { "SADDLE_STENGER_ROPER_STOCK",             GetKey("SADDLE_STENGER_ROPER_STOCK"), 0.10f, 0.04f },
            { "SADDLE_STENGER_ROPER_IMPROVED",          GetKey("SADDLE_STENGER_ROPER_IMPROVED"), 0.18f, 0.12f },

            { "SADDLE_GERDEN_TRAIL_STOCK",              GetKey("SADDLE_GERDEN_TRAIL_STOCK"), 0.04f, 0.10f },
            { "SADDLE_GERDEN_TRAIL_IMPROVED",           GetKey("SADDLE_GERDEN_TRAIL_IMPROVED"), 0.12f, 0.18f },

            { "SADDLE_GERDEN_VAQUERO_STOCK",            GetKey("SADDLE_GERDEN_VAQUERO_STOCK"), 0.06f, 0.08f },
            { "SADDLE_GERDEN_VAQUERO_IMPROVED",         GetKey("SADDLE_GERDEN_VAQUERO_IMPROVED"), 0.14f, 0.16f },

            // ==========================================
            // TRAPPER CRAFTED SADDLES
            // ==========================================
            { "SADDLE_BEAVER_ROPING",                   GetKey("SADDLE_BEAVER_ROPING"), 0.26f, 0.20f },
            { "SADDLE_ALLIGATOR_CUTTER",                GetKey("SADDLE_ALLIGATOR_CUTTER"), 0.24f, 0.20f },
            { "SADDLE_BOAR_MOTHER_HUBBARD",             GetKey("SADDLE_BOAR_MOTHER_HUBBARD"), 0.22f, 0.20f },
            { "SADDLE_COUGAR_MCLELLAND",                GetKey("SADDLE_COUGAR_MCLELLAND"), 0.20f, 0.22f },
            { "SADDLE_PANTHER_TRAIL",                   GetKey("SADDLE_PANTHER_TRAIL"), 0.20f, 0.26f },
            { "SADDLE_RATTLESNAKE_VAQUERO",             GetKey("SADDLE_RATTLESNAKE_VAQUERO"), 0.22f, 0.24f },
            { "SADDLE_BEAR_DAKOTA",                     GetKey("SADDLE_BEAR_DAKOTA"), 0.20f, 0.24f },

            // ==========================================
            // SPECIAL EDITION / ULTIMATE EDITION SADDLES
            // ==========================================
            { "SADDLE_NUEVO_PARAISO",                   GetKey("SADDLE_NUEVO_PARAISO"), 0.12f, 0.12f },
            { "SADDLE_HIGH_PLAINS_CUTTING",             GetKey("SADDLE_HIGH_PLAINS_CUTTING"), 0.12f, 0.12f },

            // ==========================================
            // PASSIVE INVENTORY ITEMS / TALISMANS
            // ==========================================
            { "PROVISION_TALISMAN_BOAR_TUSK",           GetKey("PROVISION_TALISMAN_BOAR_TUSK"), 0.10f, 0.10f, true }
        } };
}

[[nodiscard]] RDR_GUID InventoryManager::GetPlayerInventoryItemGUID(Hash itemHash, RDR_GUID guid, Hash slotId) noexcept {
    RDR_GUID outGuid{};
    
    INVENTORY::INVENTORY_GET_GUID_FROM_ITEMID(1, (Any*)&guid, itemHash, slotId, (Any*)&outGuid);

	WriteLog(LogLevel::Dev, "Retrieved player inventory item GUID for item hash " + std::to_string(itemHash) + ": " + outGuid.ToString());
    
    return outGuid;
}

// Gets the players inventory GUID
[[nodiscard]] RDR_GUID InventoryManager::GetPlayerInventoryGUID() noexcept {
    RDR_GUID emptyGuid{};

    RDR_GUID invGuid = GetPlayerInventoryItemGUID(m_CharacterHash, emptyGuid, m_NullSlotHash);

    if (invGuid.IsInvalid()) {
		WriteLog(LogLevel::Dev, "Failed to retrieve player inventory GUID.");
		return RDR_GUID{};
    }

	WriteLog(LogLevel::Dev, "Retrieved player inventory GUID: " + invGuid.ToString());

    return invGuid;
}

[[nodiscard]] Hash InventoryManager::GetItemGroup(Hash itemHash) noexcept {
    RDR_ITEMINFO info{};

    if (!ITEMDATABASE::_ITEMDATABASE_IS_KEY_VALID(itemHash, 0)) {
		WriteLog(LogLevel::Dev, "Item hash is not valid in the item database. Hash: " + std::to_string(itemHash));
        return 0;
    }

    if (!ITEMDATABASE::ITEMDATABASE_FILLOUT_ITEM_INFO(itemHash, (Any*)&info)) {
		WriteLog(LogLevel::Dev, "Failed to fill out item info for hash: " + std::to_string(itemHash));
        return 0;
    }

	WriteLog(LogLevel::Dev, "Retrieved item group for hash " + std::to_string(itemHash) + ": " + std::to_string(info.itemGroup));
    
    return info.itemGroup;
}

[[nodiscard]] RDR_SLOTINFO InventoryManager::GetItemSlotInfo(Hash itemHash) noexcept {
    RDR_SLOTINFO slotInfo{};

	RDR_GUID invGuid = GetPlayerInventoryGUID();

    if (!invGuid.IsInvalid()) {
        slotInfo.guid = invGuid;
        slotInfo.slotId = m_SatchelSlotHash;

        Hash group = GetItemGroup(itemHash);
        switch (group)
        {
        case 0xC2286F01: // CLOTHING
            if (!INVENTORY::_INVENTORY_FITS_SLOT_ID(itemHash, m_WardrobeSlotHash)) {
                WriteLog(LogLevel::Dev, "Item does not fit in wardrobe slot. Attempting to retrieve GUID from wardrobe container (hash: " + std::to_string(itemHash) + ")");
                slotInfo.guid = GetPlayerInventoryItemGUID(m_WardrobeContainerHash, slotInfo.guid, m_WardrobeSlotHash);
                slotInfo.slotId = INVENTORY::_GET_DEFAULT_ITEM_SLOT_INFO(itemHash, m_WardrobeContainerHash);
            }
            else {
                WriteLog(LogLevel::Dev, "Item fits in wardrobe slot. Using wardrobe slot hash (hash: " + std::to_string(itemHash) + ")");
                slotInfo.slotId = m_WardrobeSlotHash;
            }
            break;
        case 0x95A6F147: // HORSE 
            WriteLog(LogLevel::Dev, "Item is a horse item. Using active horse slot hash (hash: " + std::to_string(itemHash) + ")");
            slotInfo.slotId = m_ActiveHorseSlotHash;
            break;
        case 0x80FB92CD: // UPGRADE
            if (INVENTORY::_INVENTORY_FITS_SLOT_ID(itemHash, m_UpgradeSlotHash)) {
                WriteLog(LogLevel::Dev, "Item fits in upgrade slot. Using upgrade slot hash (hash: " + std::to_string(itemHash) + ")");
                slotInfo.slotId = m_UpgradeSlotHash;
            }
            break;
        default:
            WriteLog(LogLevel::Dev, "Item group is unrecognized. Attempting to determine appropriate slot.");

            if (INVENTORY::_INVENTORY_FITS_SLOT_ID(itemHash, m_SatchelSlotHash)) {
                WriteLog(LogLevel::Dev, "Item fits in satchel slot. Using satchel slot hash (hash: " + std::to_string(itemHash) + ")");
                slotInfo.slotId = m_SatchelSlotHash;
            }
            else if (INVENTORY::_INVENTORY_FITS_SLOT_ID(itemHash, m_WardrobeSlotHash)) {
                WriteLog(LogLevel::Dev, "Item fits in wardrobe slot. Using wardrobe slot hash (hash: " + std::to_string(itemHash) + ")");
                slotInfo.slotId = m_WardrobeSlotHash;
            }
            else {
                WriteLog(LogLevel::Dev, "Item does not fit in any recognized slot. Using default slot info (hash: " + std::to_string(itemHash) + ")");
                slotInfo.slotId = INVENTORY::_GET_DEFAULT_ITEM_SLOT_INFO(itemHash, m_CharacterHash);
            }
            break;
        }
    }

    return slotInfo;
}

[[nodiscard]] bool InventoryManager::DoesPlayerHaveItem(Hash itemHash, const std::string& itemName) noexcept {
    RDR_SLOTINFO slotInfo = GetItemSlotInfo(itemHash);

    if (slotInfo.IsInvalid()) {
		WriteLog(LogLevel::Dev, "Failed to retrieve slot info for item: " + itemName + " (hash: " + std::to_string(itemHash) + ")");
        return false;
    }

    RDR_GUID invGuid = GetPlayerInventoryItemGUID(itemHash, slotInfo.guid, slotInfo.slotId);

    if(invGuid.IsInvalid()) {
		WriteLog(LogLevel::Dev, "Player does not have item: " + itemName + " (hash: " + std::to_string(itemHash) + ")");
        return false;
    }

	RDR_GUID outGuid{};

	bool result = INVENTORY::_INVENTORY_IS_GUID_VALID((Any*)&slotInfo.guid);
        
    WriteLog(LogLevel::Dev, "Item " + itemName + (result ? " found" : " not found") + " in inventory.");
	
    return result;
}

[[nodiscard]] bool InventoryManager::DoesHorseHaveItem(Ped horsePed, Hash itemHash, const std::string& itemName) noexcept {
    RDR_SLOTINFO slotInfo = GetItemSlotInfo(itemHash);

    if (slotInfo.IsInvalid()) {
        WriteLog(LogLevel::Dev, "Failed to retrieve slot info for item: " + itemName + " (hash: " + std::to_string(itemHash) + ")");
        return false;
    }

    RDR_GUID invGuid = GetPlayerInventoryItemGUID(itemHash, slotInfo.guid, slotInfo.slotId);

    if (invGuid.IsInvalid()) {
        WriteLog(LogLevel::Dev, "Player does not have item: " + itemName + " (hash: " + std::to_string(itemHash) + ")");
        return false;
    }

    RDR_GUID outGuid{};

    WriteLog(LogLevel::Dev, "Checking for item: " + itemName + " (hash: " + std::to_string(itemHash) + ")");

    if (!INVENTORY::_INVENTORY_IS_GUID_VALID((Any*)&slotInfo.guid)) {
        WriteLog(LogLevel::Dev, "GUID is invalid for player inventory. Item " + itemName + " not found.");
        return false;
    }

    bool result = INVENTORY::INVENTORY_GET_GUID_FROM_ITEMID(1, (Any*)&slotInfo.guid, itemHash, slotInfo.slotId, (Any*)&outGuid);

    WriteLog(LogLevel::Dev, "Item " + itemName + (result ? " found" : " not found") + " in inventory.");
    return result;
}

[[nodiscard]] CombinedPlayerCoreModifiers InventoryManager::GetActivePlayerPassiveModifiers() noexcept {
    CombinedPlayerCoreModifiers modifiers{};

    for (const auto& passive : m_PlayerPassiveModifiers) {
        // All talismans/trinkets live in the wardrobe data tree branch
        if (DoesPlayerHaveItem(passive.itemHash, std::string(passive.name))) {
            modifiers.totalHealthReduction += passive.healthCoreMod;
            modifiers.totalStaminaReduction += passive.staminaCoreMod;
            modifiers.totalDeadEyeReduction += passive.deadEyeCoreMod;
        }
    }

    return modifiers;
}

[[nodiscard]] CombinedTackModifiers InventoryManager::GetActiveHorseTackModifiers(Ped horsePed) noexcept {
    CombinedTackModifiers modifiers{};

    // Scan for Stirrups
    for (const auto& stirrup : m_StirrupModifiers) {
        if (DoesHorseHaveItem(horsePed, stirrup.itemHash, std::string(stirrup.name))) {
            modifiers.totalStaminaReduction += stirrup.staminaCoreMod;
            break;
        }
    }

    // Scan for Saddles & Horse Player Passives
    for (const auto& saddle : m_SaddleModifiers) {
        bool hasItem = false;

        if (saddle.isPlayerPassive)
            hasItem = DoesPlayerHaveItem(saddle.itemHash, std::string(saddle.name));
        else
            hasItem = DoesHorseHaveItem(horsePed, saddle.itemHash, std::string(saddle.name));

        if (hasItem) {
            modifiers.totalHealthReduction += saddle.healthCoreMod;
            modifiers.totalStaminaReduction += saddle.staminaCoreMod;
        }
    }

    return modifiers;
}