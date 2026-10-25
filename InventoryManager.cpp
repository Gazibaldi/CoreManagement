/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#include "..\CoreSDK\inc\types.h"
#include "..\CoreSDK\inc\natives.h"
#include "InventoryManager.h"
#include "Logger.h"
#include "CMTypes.h"

Hash GetKey(const char* key) {
    Hash hash = MISC::GET_HASH_KEY(key);
    LOG_DEV("Precomputed hash for key '{}': {}", key, hash);

    return hash;
}

void InventoryManager::ComputeHashes() noexcept {
    m_NullSlotHash = GetKey("SLOTID_NONE");
    m_CharacterHash = GetKey("CHARACTER");
    m_SatchelSlotHash = GetKey("SLOTID_SATCHEL");
    m_WardrobeContainerHash = GetKey("WARDROBE");
    m_WardrobeSlotHash = GetKey("SLOTID_WARDROBE");
	m_ActiveHorseSlotHash = GetKey("SLOTID_ACTIVE_HORSE");

    m_PlayerPassiveModifiers = { {
            { "PROVISION_TRINKET_OWL_FEATHER",        GetKey("PROVISION_TRINKET_OWL_FEATHER"), 0.15f, 0.15f, 0.15f }, // Fits in SLOTID_SATCHEL
            { "PROVISION_TALISMAN_BEAR_CLAW",         GetKey("PROVISION_TALISMAN_BEAR_CLAW"), 0.10f, 0.00f, 0.00f }, // Fits in WARDROBE
            { "PROVISION_TALISMAN_BUFFALO_HORN",      GetKey("PROVISION_TALISMAN_BUFFALO_HORN"), 0.00f, 0.10f, 0.00f }, // Fits in SLOTID_SATCHEL
            { "PROVISION_TALISMAN_ALLIGATOR_TOOTH",   GetKey("PROVISION_TALISMAN_ALLIGATOR_TOOTH"), 0.00f, 0.00f, 0.10f } // Fits in WARDROBE
    } };


    // TODO: Need to get confirmation of the correct mapping to entry to name
    m_StirrupModifiers = { {
        { "HORSE_EQUIPMENT_STIRRUP_NEW_000",         GetKey("HORSE_EQUIPMENT_STIRRUP_NEW_000"), 0.00f, 0.00f }, // FILLIES
		{ "HORSE_EQUIPMENT_STIRRUP_NEW_001",         GetKey("HORSE_EQUIPMENT_STIRRUP_NEW_001"), 0.00f, 0.05f }, // BAROQUE
		{ "HORSE_EQUIPMENT_STIRRUP_NEW_002",         GetKey("HORSE_EQUIPMENT_STIRRUP_NEW_002"), 0.00f, 0.10f }, // SLIM LINE IRON
		{ "HORSE_EQUIPMENT_STIRRUP_NEW_003",         GetKey("HORSE_EQUIPMENT_STIRRUP_NEW_003"), 0.00f, 0.15f }, // SLIM LINE
		{ "HORSE_EQUIPMENT_STIRRUP_NEW_004",         GetKey("HORSE_EQUIPMENT_STIRRUP_NEW_004"), 0.00f, 0.20f }, // DEEP ROPER
		{ "HORSE_EQUIPMENT_STIRRUP_NEW_005",         GetKey("HORSE_EQUIPMENT_STIRRUP_NEW_006"), 0.00f, 0.30f }, // BELLED OXBOW
		{ "HORSE_EQUIPMENT_STIRRUP_NEW_006",         GetKey("HORSE_EQUIPMENT_STIRRUP_NEW_007"), 0.00f, 0.35f }, // OXBOW
		{ "HORSE_EQUIPMENT_STIRRUP_NEW_007",         GetKey("HORSE_EQUIPMENT_STIRRUP_NEW_008"), 0.00f, 0.40f }, // BELL
		{ "HORSE_EQUIPMENT_STIRRUP_NEW_008",         GetKey("HORSE_EQUIPMENT_STIRRUP_NEW_009"), 0.00f, 0.40f }, // SAFETY
		{ "HORSE_EQUIPMENT_STIRRUP_NEW_009",         GetKey("HORSE_EQUIPMENT_STIRRUP_NEW_010"), 0.00f, 0.45f }, // TAPADERA
		{ "HORSE_EQUIPMENT_STIRRUP_NEW_010",         GetKey("HORSE_EQUIPMENT_STIRRUP_NEW_011"), 0.00f, 0.50f }  // HOODED
    } };

    m_SaddleModifiers = { {
        // ==========================================
        // TRAPPER CRAFTED SADDLES
        // ==========================================
		{ "HORSE_EQUIPMENT_MOTHERHUBBARD_01_SPECIAL_NEW_SADDLE_000",         GetKey("HORSE_EQUIPMENT_MOTHERHUBBARD_01_SPECIAL_NEW_SADDLE_000"), 0.22f, 0.20f }, // BOAR MOTHER HUBBARD
        { "HORSE_EQUIPMENT_WESTERN_01_SPECIAL_NEW_SADDLE_000",               GetKey("HORSE_EQUIPMENT_WESTERN_01_SPECIAL_NEW_SADDLE_000"), 0.26f, 0.20f }, // BEAVER ROPING
        { "HORSE_EQUIPMENT_WESTERN_02_SPECIAL_NEW_SADDLE_000",               GetKey("HORSE_EQUIPMENT_WESTERN_02_SPECIAL_NEW_SADDLE_000"), 0.20f, 0.26f }, // PANTHER TRAIL
        { "HORSE_EQUIPMENT_WESTERN_03_SPECIAL_NEW_SADDLE_000",               GetKey("HORSE_EQUIPMENT_WESTERN_03_SPECIAL_NEW_SADDLE_000"), 0.24f, 0.20f }, // ALLIGATOR CUTTER
		{ "HORSE_EQUIPMENT_WESTERN_04_SPECIAL_NEW_SADDLE_000",               GetKey("HORSE_EQUIPMENT_WESTERN_04_SPECIAL_NEW_SADDLE_000"), 0.20f, 0.24f }, // BEAR DAKOTA
        { "HORSE_EQUIPMENT_MCCLELLAN_01_SPECIAL_NEW_SADDLE_000",             GetKey("HORSE_EQUIPMENT_MCCLELLAN_01_SPECIAL_NEW_SADDLE_000"), 0.20f, 0.22f }, // COUGAR MCLELLAND
		{ "HORSE_EQUIPMENT_CHARRO_01_SPECIAL_NEW_SADDLE_000",                GetKey("HORSE_EQUIPMENT_CHARRO_01_SPECIAL_NEW_SADDLE_000"), 0.22f, 0.24f }, // RATTLESNAKE VAQUERO
        
        // ==========================================
        // STANDARD / UPGRADED STABLE SADDLES
        // ==========================================
        // Kneller Mother Hubbard Saddle (Stock)
        { "HORSE_EQUIPMENT_MOTHERHUBBARD_01_STOCK_NEW_SADDLE_000",            GetKey("HORSE_EQUIPMENT_MOTHERHUBBARD_01_STOCK_NEW_SADDLE_000"), 0.06f, 0.04f },
        { "HORSE_EQUIPMENT_MOTHERHUBBARD_01_STOCK_NEW_SADDLE_001",            GetKey("HORSE_EQUIPMENT_MOTHERHUBBARD_01_STOCK_NEW_SADDLE_001"), 0.06f, 0.04f },
        { "HORSE_EQUIPMENT_MOTHERHUBBARD_01_STOCK_NEW_SADDLE_002",            GetKey("HORSE_EQUIPMENT_MOTHERHUBBARD_01_STOCK_NEW_SADDLE_002"), 0.06f, 0.04f },
        { "HORSE_EQUIPMENT_MOTHERHUBBARD_01_STOCK_NEW_SADDLE_003",            GetKey("HORSE_EQUIPMENT_MOTHERHUBBARD_01_STOCK_NEW_SADDLE_003"), 0.06f, 0.04f },
        { "HORSE_EQUIPMENT_MOTHERHUBBARD_01_STOCK_NEW_SADDLE_004",            GetKey("HORSE_EQUIPMENT_MOTHERHUBBARD_01_STOCK_NEW_SADDLE_004"), 0.06f, 0.04f },
        { "HORSE_EQUIPMENT_MOTHERHUBBARD_01_STOCK_NEW_SADDLE_005",            GetKey("HORSE_EQUIPMENT_MOTHERHUBBARD_01_STOCK_NEW_SADDLE_005"), 0.06f, 0.04f },

        // Kneller Mother Hubbard Saddle (Improved)
        { "HORSE_EQUIPMENT_MOTHERHUBBARD_01_IMPROVED_NEW_SADDLE_000",         GetKey("HORSE_EQUIPMENT_MOTHERHUBBARD_01_IMPROVED_NEW_SADDLE_000"), 0.14f, 0.12f },
        { "HORSE_EQUIPMENT_MOTHERHUBBARD_01_IMPROVED_NEW_SADDLE_001",         GetKey("HORSE_EQUIPMENT_MOTHERHUBBARD_01_IMPROVED_NEW_SADDLE_001"), 0.14f, 0.12f },
        { "HORSE_EQUIPMENT_MOTHERHUBBARD_01_IMPROVED_NEW_SADDLE_002",         GetKey("HORSE_EQUIPMENT_MOTHERHUBBARD_01_IMPROVED_NEW_SADDLE_002"), 0.14f, 0.12f },
        { "HORSE_EQUIPMENT_MOTHERHUBBARD_01_IMPROVED_NEW_SADDLE_003",         GetKey("HORSE_EQUIPMENT_MOTHERHUBBARD_01_IMPROVED_NEW_SADDLE_003"), 0.14f, 0.12f },
        { "HORSE_EQUIPMENT_MOTHERHUBBARD_01_IMPROVED_NEW_SADDLE_004",         GetKey("HORSE_EQUIPMENT_MOTHERHUBBARD_01_IMPROVED_NEW_SADDLE_004"), 0.14f, 0.12f },
        { "HORSE_EQUIPMENT_MOTHERHUBBARD_01_IMPROVED_NEW_SADDLE_005",         GetKey("HORSE_EQUIPMENT_MOTHERHUBBARD_01_IMPROVED_NEW_SADDLE_005"), 0.14f, 0.12f },

        // Stenger Roping Saddle(Stock)
        { "HORSE_EQUIPMENT_WESTERN_01_STOCK_NEW_SADDLE_000",                  GetKey("HORSE_EQUIPMENT_WESTERN_01_STOCK_NEW_SADDLE_000"), 0.10f, 0.04f },
        { "HORSE_EQUIPMENT_WESTERN_01_STOCK_NEW_SADDLE_001",                  GetKey("HORSE_EQUIPMENT_WESTERN_01_STOCK_NEW_SADDLE_001"), 0.10f, 0.04f },
        { "HORSE_EQUIPMENT_WESTERN_01_STOCK_NEW_SADDLE_002",                  GetKey("HORSE_EQUIPMENT_WESTERN_01_STOCK_NEW_SADDLE_002"), 0.10f, 0.04f },
        { "HORSE_EQUIPMENT_WESTERN_01_STOCK_NEW_SADDLE_003",                  GetKey("HORSE_EQUIPMENT_WESTERN_01_STOCK_NEW_SADDLE_003"), 0.10f, 0.04f },
        { "HORSE_EQUIPMENT_WESTERN_01_STOCK_NEW_SADDLE_004",                  GetKey("HORSE_EQUIPMENT_WESTERN_01_STOCK_NEW_SADDLE_004"), 0.10f, 0.04f },
        { "HORSE_EQUIPMENT_WESTERN_01_STOCK_NEW_SADDLE_005",                  GetKey("HORSE_EQUIPMENT_WESTERN_01_STOCK_NEW_SADDLE_005"), 0.10f, 0.04f },

        // Stenger Roping Saddle(Impoved)
        { "HORSE_EQUIPMENT_WESTERN_01_IMPROVED_NEW_SADDLE_000",               GetKey("HORSE_EQUIPMENT_WESTERN_01_IMPROVED_NEW_SADDLE_000"), 0.18f, 0.12f },
        { "HORSE_EQUIPMENT_WESTERN_01_IMPROVED_NEW_SADDLE_001",               GetKey("HORSE_EQUIPMENT_WESTERN_01_IMPROVED_NEW_SADDLE_001"), 0.18f, 0.12f },
        { "HORSE_EQUIPMENT_WESTERN_01_IMPROVED_NEW_SADDLE_002",               GetKey("HORSE_EQUIPMENT_WESTERN_01_IMPROVED_NEW_SADDLE_002"), 0.18f, 0.12f },
        { "HORSE_EQUIPMENT_WESTERN_01_IMPROVED_NEW_SADDLE_003",               GetKey("HORSE_EQUIPMENT_WESTERN_01_IMPROVED_NEW_SADDLE_003"), 0.18f, 0.12f },
        { "HORSE_EQUIPMENT_WESTERN_01_IMPROVED_NEW_SADDLE_004",               GetKey("HORSE_EQUIPMENT_WESTERN_01_IMPROVED_NEW_SADDLE_004"), 0.18f, 0.12f },
        { "HORSE_EQUIPMENT_WESTERN_01_IMPROVED_NEW_SADDLE_005",               GetKey("HORSE_EQUIPMENT_WESTERN_01_IMPROVED_NEW_SADDLE_005"), 0.18f, 0.12f },

        // Gerden Trail Saddle (Stock)
        { "HORSE_EQUIPMENT_WESTERN_02_STOCK_NEW_SADDLE_000",                  GetKey("HORSE_EQUIPMENT_WESTERN_02_STOCK_NEW_SADDLE_000"), 0.04f, 0.10f },
        { "HORSE_EQUIPMENT_WESTERN_02_STOCK_NEW_SADDLE_001",                  GetKey("HORSE_EQUIPMENT_WESTERN_02_STOCK_NEW_SADDLE_001"), 0.04f, 0.10f },
        { "HORSE_EQUIPMENT_WESTERN_02_STOCK_NEW_SADDLE_002",                  GetKey("HORSE_EQUIPMENT_WESTERN_02_STOCK_NEW_SADDLE_002"), 0.04f, 0.10f },
        { "HORSE_EQUIPMENT_WESTERN_02_STOCK_NEW_SADDLE_003",                  GetKey("HORSE_EQUIPMENT_WESTERN_02_STOCK_NEW_SADDLE_003"), 0.04f, 0.10f },
        { "HORSE_EQUIPMENT_WESTERN_02_STOCK_NEW_SADDLE_004",                  GetKey("HORSE_EQUIPMENT_WESTERN_02_STOCK_NEW_SADDLE_004"), 0.04f, 0.10f },
        { "HORSE_EQUIPMENT_WESTERN_02_STOCK_NEW_SADDLE_005",                  GetKey("HORSE_EQUIPMENT_WESTERN_02_STOCK_NEW_SADDLE_005"), 0.04f, 0.10f },

        // Gerden Trail Saddle (Improved)
        { "HORSE_EQUIPMENT_WESTERN_02_IMPROVED_NEW_SADDLE_000",               GetKey("HORSE_EQUIPMENT_WESTERN_02_IMPROVED_NEW_SADDLE_000"), 0.18f, 0.12f },
        { "HORSE_EQUIPMENT_WESTERN_02_IMPROVED_NEW_SADDLE_001",               GetKey("HORSE_EQUIPMENT_WESTERN_02_IMPROVED_NEW_SADDLE_001"), 0.18f, 0.12f },
        { "HORSE_EQUIPMENT_WESTERN_02_IMPROVED_NEW_SADDLE_002",               GetKey("HORSE_EQUIPMENT_WESTERN_02_IMPROVED_NEW_SADDLE_002"), 0.18f, 0.12f },
        { "HORSE_EQUIPMENT_WESTERN_02_IMPROVED_NEW_SADDLE_003",               GetKey("HORSE_EQUIPMENT_WESTERN_02_IMPROVED_NEW_SADDLE_003"), 0.18f, 0.12f },
        { "HORSE_EQUIPMENT_WESTERN_02_IMPROVED_NEW_SADDLE_004",               GetKey("HORSE_EQUIPMENT_WESTERN_02_IMPROVED_NEW_SADDLE_004"), 0.18f, 0.12f },
        { "HORSE_EQUIPMENT_WESTERN_02_IMPROVED_NEW_SADDLE_005",               GetKey("HORSE_EQUIPMENT_WESTERN_02_IMPROVED_NEW_SADDLE_005"), 0.18f, 0.12f },

        // Lumley Ranch Cutter Saddle (Stock)
        { "HORSE_EQUIPMENT_WESTERN_03_STOCK_NEW_SADDLE_000",                  GetKey("HORSE_EQUIPMENT_WESTERN_03_STOCK_NEW_SADDLE_000"), 0.10f, 0.04f },
        { "HORSE_EQUIPMENT_WESTERN_03_STOCK_NEW_SADDLE_001",                  GetKey("HORSE_EQUIPMENT_WESTERN_03_STOCK_NEW_SADDLE_001"), 0.10f, 0.04f },
        { "HORSE_EQUIPMENT_WESTERN_03_STOCK_NEW_SADDLE_002",                  GetKey("HORSE_EQUIPMENT_WESTERN_03_STOCK_NEW_SADDLE_002"), 0.10f, 0.04f },
        { "HORSE_EQUIPMENT_WESTERN_03_STOCK_NEW_SADDLE_003",                  GetKey("HORSE_EQUIPMENT_WESTERN_03_STOCK_NEW_SADDLE_003"), 0.10f, 0.04f },
        { "HORSE_EQUIPMENT_WESTERN_03_STOCK_NEW_SADDLE_004",                  GetKey("HORSE_EQUIPMENT_WESTERN_03_STOCK_NEW_SADDLE_004"), 0.10f, 0.04f },
        { "HORSE_EQUIPMENT_WESTERN_03_STOCK_NEW_SADDLE_005",                  GetKey("HORSE_EQUIPMENT_WESTERN_03_STOCK_NEW_SADDLE_005"), 0.10f, 0.04f },

        // Lumley Ranch Cutter Saddle (Improved)
        { "HORSE_EQUIPMENT_WESTERN_03_IMPROVED_NEW_SADDLE_000",               GetKey("HORSE_EQUIPMENT_WESTERN_03_IMPROVED_NEW_SADDLE_000"), 0.12f, 0.18f },
        { "HORSE_EQUIPMENT_WESTERN_03_IMPROVED_NEW_SADDLE_001",               GetKey("HORSE_EQUIPMENT_WESTERN_03_IMPROVED_NEW_SADDLE_001"), 0.12f, 0.18f },
        { "HORSE_EQUIPMENT_WESTERN_03_IMPROVED_NEW_SADDLE_002",               GetKey("HORSE_EQUIPMENT_WESTERN_03_IMPROVED_NEW_SADDLE_002"), 0.12f, 0.18f },
        { "HORSE_EQUIPMENT_WESTERN_03_IMPROVED_NEW_SADDLE_003",               GetKey("HORSE_EQUIPMENT_WESTERN_03_IMPROVED_NEW_SADDLE_003"), 0.12f, 0.18f },
        { "HORSE_EQUIPMENT_WESTERN_03_IMPROVED_NEW_SADDLE_004",               GetKey("HORSE_EQUIPMENT_WESTERN_03_IMPROVED_NEW_SADDLE_004"), 0.12f, 0.18f },
        { "HORSE_EQUIPMENT_WESTERN_03_IMPROVED_NEW_SADDLE_005",               GetKey("HORSE_EQUIPMENT_WESTERN_03_IMPROVED_NEW_SADDLE_005"), 0.12f, 0.18f },

        // Kneller Dakota Saddle (Stock)
        { "HORSE_EQUIPMENT_WESTERN_04_STOCK_NEW_SADDLE_000",                  GetKey("HORSE_EQUIPMENT_WESTERN_04_STOCK_NEW_SADDLE_000"), 0.04f, 0.08f },
        { "HORSE_EQUIPMENT_WESTERN_04_STOCK_NEW_SADDLE_001",                  GetKey("HORSE_EQUIPMENT_WESTERN_04_STOCK_NEW_SADDLE_001"), 0.04f, 0.08f },
        { "HORSE_EQUIPMENT_WESTERN_04_STOCK_NEW_SADDLE_002",                  GetKey("HORSE_EQUIPMENT_WESTERN_04_STOCK_NEW_SADDLE_002"), 0.04f, 0.08f },
        { "HORSE_EQUIPMENT_WESTERN_04_STOCK_NEW_SADDLE_003",                  GetKey("HORSE_EQUIPMENT_WESTERN_04_STOCK_NEW_SADDLE_003"), 0.04f, 0.08f },
        { "HORSE_EQUIPMENT_WESTERN_04_STOCK_NEW_SADDLE_004",                  GetKey("HORSE_EQUIPMENT_WESTERN_04_STOCK_NEW_SADDLE_004"), 0.04f, 0.08f },
        { "HORSE_EQUIPMENT_WESTERN_04_STOCK_NEW_SADDLE_005",                  GetKey("HORSE_EQUIPMENT_WESTERN_04_STOCK_NEW_SADDLE_005"), 0.04f, 0.08f },

        // Kneller Dakota Saddle (Improved)
        { "HORSE_EQUIPMENT_WESTERN_04_IMPROVED_NEW_SADDLE_000",               GetKey("HORSE_EQUIPMENT_WESTERN_04_IMPROVED_NEW_SADDLE_000"), 0.12f, 0.16f },
        { "HORSE_EQUIPMENT_WESTERN_04_IMPROVED_NEW_SADDLE_001",               GetKey("HORSE_EQUIPMENT_WESTERN_04_IMPROVED_NEW_SADDLE_001"), 0.12f, 0.16f },
        { "HORSE_EQUIPMENT_WESTERN_04_IMPROVED_NEW_SADDLE_002",               GetKey("HORSE_EQUIPMENT_WESTERN_04_IMPROVED_NEW_SADDLE_002"), 0.12f, 0.16f },
        { "HORSE_EQUIPMENT_WESTERN_04_IMPROVED_NEW_SADDLE_003",               GetKey("HORSE_EQUIPMENT_WESTERN_04_IMPROVED_NEW_SADDLE_003"), 0.12f, 0.16f },
        { "HORSE_EQUIPMENT_WESTERN_04_IMPROVED_NEW_SADDLE_004",               GetKey("HORSE_EQUIPMENT_WESTERN_04_IMPROVED_NEW_SADDLE_004"), 0.12f, 0.16f },
        { "HORSE_EQUIPMENT_WESTERN_04_IMPROVED_NEW_SADDLE_005",               GetKey("HORSE_EQUIPMENT_WESTERN_04_IMPROVED_NEW_SADDLE_005"), 0.12f, 0.16f },

        // Lumley McClelland Saddle (Stock)
        { "HORSE_EQUIPMENT_MCCLELLAN_01_STOCK_NEW_SADDLE_000",                GetKey("HORSE_EQUIPMENT_MCCLELLAN_01_STOCK_NEW_SADDLE_000"), 0.04f, 0.06f },
        { "HORSE_EQUIPMENT_MCCLELLAN_01_STOCK_NEW_SADDLE_001",                GetKey("HORSE_EQUIPMENT_MCCLELLAN_01_STOCK_NEW_SADDLE_001"), 0.04f, 0.06f },
        { "HORSE_EQUIPMENT_MCCLELLAN_01_STOCK_NEW_SADDLE_002",                GetKey("HORSE_EQUIPMENT_MCCLELLAN_01_STOCK_NEW_SADDLE_002"), 0.04f, 0.06f },
        { "HORSE_EQUIPMENT_MCCLELLAN_01_STOCK_NEW_SADDLE_003",                GetKey("HORSE_EQUIPMENT_MCCLELLAN_01_STOCK_NEW_SADDLE_003"), 0.04f, 0.06f },
        { "HORSE_EQUIPMENT_MCCLELLAN_01_STOCK_NEW_SADDLE_004",                GetKey("HORSE_EQUIPMENT_MCCLELLAN_01_STOCK_NEW_SADDLE_004"), 0.04f, 0.06f },
        { "HORSE_EQUIPMENT_MCCLELLAN_01_STOCK_NEW_SADDLE_005",                GetKey("HORSE_EQUIPMENT_MCCLELLAN_01_STOCK_NEW_SADDLE_005"), 0.04f, 0.06f },

        // Lumley McClelland Saddle (Improved)
        { "HORSE_EQUIPMENT_MCCLELLAN_01_IMPROVED_NEW_SADDLE_000",            GetKey("HORSE_EQUIPMENT_MCCLELLAN_01_IMPROVED_NEW_SADDLE_000"), 0.14f, 0.18f },
        { "HORSE_EQUIPMENT_MCCLELLAN_01_IMPROVED_NEW_SADDLE_001",            GetKey("HORSE_EQUIPMENT_MCCLELLAN_01_IMPROVED_NEW_SADDLE_001"), 0.14f, 0.18f },
        { "HORSE_EQUIPMENT_MCCLELLAN_01_IMPROVED_NEW_SADDLE_002",            GetKey("HORSE_EQUIPMENT_MCCLELLAN_01_IMPROVED_NEW_SADDLE_002"), 0.14f, 0.18f },
        { "HORSE_EQUIPMENT_MCCLELLAN_01_IMPROVED_NEW_SADDLE_003",            GetKey("HORSE_EQUIPMENT_MCCLELLAN_01_IMPROVED_NEW_SADDLE_003"), 0.14f, 0.18f },
        { "HORSE_EQUIPMENT_MCCLELLAN_01_IMPROVED_NEW_SADDLE_004",            GetKey("HORSE_EQUIPMENT_MCCLELLAN_01_IMPROVED_NEW_SADDLE_004"), 0.14f, 0.18f },
        { "HORSE_EQUIPMENT_MCCLELLAN_01_IMPROVED_NEW_SADDLE_005",            GetKey("HORSE_EQUIPMENT_MCCLELLAN_01_IMPROVED_NEW_SADDLE_005"), 0.14f, 0.18f },

        // Gerden Vaquero Saddle (Stock)
        { "HORSE_EQUIPMENT_CHARRO_01_STOCK_NEW_SADDLE_000",                  GetKey("HORSE_EQUIPMENT_CHARRO_01_STOCK_NEW_SADDLE_000"), 0.06f, 0.08f },
        { "HORSE_EQUIPMENT_CHARRO_01_STOCK_NEW_SADDLE_001",                  GetKey("HORSE_EQUIPMENT_CHARRO_01_STOCK_NEW_SADDLE_001"), 0.06f, 0.08f },
        { "HORSE_EQUIPMENT_CHARRO_01_STOCK_NEW_SADDLE_002",                  GetKey("HORSE_EQUIPMENT_CHARRO_01_STOCK_NEW_SADDLE_002"), 0.06f, 0.08f },
        { "HORSE_EQUIPMENT_CHARRO_01_STOCK_NEW_SADDLE_003",                  GetKey("HORSE_EQUIPMENT_CHARRO_01_STOCK_NEW_SADDLE_003"), 0.06f, 0.08f },
        { "HORSE_EQUIPMENT_CHARRO_01_STOCK_NEW_SADDLE_004",                  GetKey("HORSE_EQUIPMENT_CHARRO_01_STOCK_NEW_SADDLE_004"), 0.06f, 0.08f },
        { "HORSE_EQUIPMENT_CHARRO_01_STOCK_NEW_SADDLE_005",                  GetKey("HORSE_EQUIPMENT_CHARRO_01_STOCK_NEW_SADDLE_005"), 0.06f, 0.08f },

        // Gerden Vaquero Saddle (Improved)
        { "HORSE_EQUIPMENT_CHARRO_01_IMPROVED_NEW_SADDLE_000",               GetKey("HORSE_EQUIPMENT_CHARRO_01_IMPROVED_NEW_SADDLE_000"), 0.14f, 0.16f },
        { "HORSE_EQUIPMENT_CHARRO_01_IMPROVED_NEW_SADDLE_001",               GetKey("HORSE_EQUIPMENT_CHARRO_01_IMPROVED_NEW_SADDLE_001"), 0.14f, 0.16f },
        { "HORSE_EQUIPMENT_CHARRO_01_IMPROVED_NEW_SADDLE_002",               GetKey("HORSE_EQUIPMENT_CHARRO_01_IMPROVED_NEW_SADDLE_002"), 0.14f, 0.16f },
        { "HORSE_EQUIPMENT_CHARRO_01_IMPROVED_NEW_SADDLE_003",               GetKey("HORSE_EQUIPMENT_CHARRO_01_IMPROVED_NEW_SADDLE_003"), 0.14f, 0.16f },
        { "HORSE_EQUIPMENT_CHARRO_01_IMPROVED_NEW_SADDLE_004",               GetKey("HORSE_EQUIPMENT_CHARRO_01_IMPROVED_NEW_SADDLE_004"), 0.14f, 0.16f },
        { "HORSE_EQUIPMENT_CHARRO_01_IMPROVED_NEW_SADDLE_005",               GetKey("HORSE_EQUIPMENT_CHARRO_01_IMPROVED_NEW_SADDLE_005"), 0.14f, 0.16f },

        // ==========================================
        // SPECIAL EDITION / ULTIMATE EDITION SADDLES
        // ==========================================
        { "SADDLE_NUEVO_PARAISO",                   GetKey("SADDLE_NUEVO_PARAISO"), 0.12f, 0.12f }, //TODO: the key is wrong

        // ==========================================
        // PASSIVE INVENTORY ITEMS / TALISMANS
        // ==========================================
        { "PROVISION_TALISMAN_BOAR_TUSK",           GetKey("PROVISION_TALISMAN_BOAR_TUSK"), 0.10f, 0.10f, true } // Fits in WARDROBE
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
		case 0x8A291148: // HORSE
            LOG_DEV("Item (hash: {}) fits horse wardrobe container (hash: {})", itemHash, m_WardrobeContainerHash);
            slotInfo.guid = GetPlayerInventoryItemGUID(m_WardrobeContainerHash, slotInfo.guid, m_ActiveHorseSlotHash);
            slotInfo.slotId = INVENTORY::_GET_DEFAULT_ITEM_SLOT_INFO(itemHash, m_WardrobeContainerHash);
            break;
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
            LOG(LogLevel::Verbose, "Item {} (hash: {}) is in player inventory but did not pass the GUID validity check", itemName, itemHash);
        else
            LOG(LogLevel::Verbose, "Player has item: {} (hash: {}) in their inventory. Adding core passive modifiers", itemName, itemHash);

        return result;
    }
}

bool InventoryManager::DoesHorseHaveItem(Ped horsePed, Hash itemHash, const std::string& itemName) noexcept {
    RDR_SLOTINFO slotInfo = GetItemSlotInfo(itemHash);

    if (slotInfo.IsInvalid()) {
        LOG_DEV("Failed to retrieve slot info for item: {} (hash: {})", itemName, itemHash);
        return false;
    }

    RDR_GUID itemGuid = GetPlayerInventoryItemGUID(itemHash, slotInfo.guid, slotInfo.slotId);

	LOG_DEV("Inventory item: {} (hash: {}) has GUID: {}", itemName, itemHash, itemGuid.ToString());

    if (itemGuid.IsInvalid()) {
        LOG(LogLevel::Verbose, "Horse does not have item: {} (hash: {}) in their inventory", itemName, itemHash);
        return false;
    }
    else {
        // Sanity check to ensure the GUID is valid before returning
        bool result = INVENTORY::_INVENTORY_IS_GUID_VALID((Any*)&itemGuid);

        if (!result)
            LOG(LogLevel::Verbose, "Item {} (hash: {}) is in horse inventory but did not pass the GUID validity check", itemName, itemHash);
        else
            LOG(LogLevel::Verbose, "Horse has item: {} (hash: {}) in their inventory. Adding core passive modifiers", itemName, itemHash);

        return result;
    }
}

[[nodiscard]] CombinedPlayerCoreModifiers InventoryManager::GetActivePlayerPassiveModifiers() noexcept {
    CombinedPlayerCoreModifiers modifiers{};

    for (const auto& passive : m_PlayerPassiveModifiers) {
		LOG(LogLevel::Verbose, "Checking for player passive item: {} (hash: {})", passive.name, passive.itemHash);
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

	return modifiers;


	// TODO: this just checks for items in the inventory not if they are equipped. Need to check if the horse has the item equipped instead of just in inventory.
    // Scan for Stirrups
    for (const auto& stirrup : m_StirrupModifiers) {
		LOG(LogLevel::Verbose, "Checking for horse stirrup item: {} (hash: {})", stirrup.name, stirrup.itemHash);
        if (DoesHorseHaveItem(horsePed, stirrup.itemHash, std::string(stirrup.name))) {
            modifiers.totalStaminaReduction += stirrup.staminaCoreMod;
            break;
        }
    }

    // Scan for Saddles & Horse Player Passives
    for (const auto& saddle : m_SaddleModifiers) {
		LOG(LogLevel::Verbose, "Checking for horse saddle item: {} (hash: {})", saddle.name, saddle.itemHash);
        bool hasItem = false;

        if (saddle.isPlayerPassive)
            hasItem = DoesPlayerHaveItem(saddle.itemHash, std::string(saddle.name));
        else
        {
            hasItem = DoesHorseHaveItem(horsePed, saddle.itemHash, std::string(saddle.name));
        }

        if (hasItem) {
            modifiers.totalHealthReduction += saddle.healthCoreMod;
            modifiers.totalStaminaReduction += saddle.staminaCoreMod;
        }
    }

    return modifiers;
}