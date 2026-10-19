#pragma once
#include <string_view>
#include <cstdint>
#include "..\CoreSDK\inc\types.h"

struct SimulationState {
    bool isRestricted = false;
	bool isRestrictedVariable = false;

    int cachedInGameTimeMinutes = 0;
    int accumulatedTimeMs = 0;

    float cachedHealth = 100.0f;
    float cachedStamina = 100.0f;
    float cachedDeadEye = 100.0f;
    int cachedMoney = 0;

    int cachedBathingState = 0;

    bool wasSleepingDuringRestriction = false;
    bool wasAtCampDuringRestriction = false;
    bool wasInMinigameDuringRestriction = false;
    bool wasInMissionDuringRestriction = false;
    bool isBathingDuringRestriction = false;
    bool wasRestrictedLastTick = false;
	
    float PostMissionDecayHoursRemaining = 0.0f;    

    bool hadHorse = false;

    float cachedHorseHealth = 100.0f;
    float cachedHorseStamina = 100.0f;

    int lastTickInGameMinutes = 0;
    bool timescaleDeltaInitialised = false;

    Ped lastKnownPlayerPed = 0;
};
struct GameplayContext {
    bool freezeActive = false;
    bool isFreezeVariable = false;
    bool isInMission = false;
    bool isInMinigame = false;
    bool isSleeping = false;
    bool isAtCamp = false;
    bool isJailed = false;
    bool isBathing = false;
};

// Enforce strict 8-byte structure alignment layout across the execution pipeline
#pragma pack(push, 8)

struct RDR_GUID
{
    std::int32_t data1;
    std::int32_t data2;
    std::int32_t data3;
    std::int32_t data4;

    // C++20 Spaceship operator for clean, reliable equality evaluations
    auto operator<=>(const RDR_GUID&) const = default;

    bool IsInvalid() const
    {
        return data1 == 0 && data2 == 0 && data3 == 0 && data4 == 0;
    }
};

// This is the direct memory representation of s_InventoryItemData expected by the engine
struct s_InventoryItemData
{
    RDR_GUID itemGuid;            // 16 bytes: Unique instance tracker
    std::int32_t unkFlags;     // 4 bytes: State flags / Pointer metadata
    std::int32_t stateData;    // 4 bytes: Quality, condition, or decay state
    std::int32_t trackingHash; // 4 bytes: Run-time hash map identifier
    std::uint32_t slotId;      // 4 bytes: The Joaat Hash of the slot (e.g., SLOTID_SATCHEL)
};

// Used for retrieving runtime status metrics of a specific item slot instance
struct RDR_ITEMINFO
{
    std::int32_t itemCondition;  // 4 bytes: Weapon wear/soot level or item decay modifier
    std::int32_t stylingMapPtr;  // 4 bytes: Runtime pointer index to cosmetic modifications
    std::int32_t runtimeTrack;   // 4 bytes: Core intrinsic status data tracker
    std::int32_t quantity;       // 4 bytes: Total item/ammo count stacked in this exact slot
    std::uint32_t runtimeFlags;  // 4 bytes: Engine-level operational status bitflags
    std::int32_t padding1;       // 4 bytes: Reserved structure alignment padding
    std::int32_t padding2;       // 4 bytes: Reserved structure alignment padding
};

#pragma pack(pop)

struct PlayerPassiveModifier {
    std::string_view name;
    Hash itemHash;
    float healthCoreMod;
    float staminaCoreMod;
    float deadEyeCoreMod;
};

struct CombinedPlayerCoreModifiers {
    float totalHealthReduction = 0.0f;
    float totalStaminaReduction = 0.0f;
    float totalDeadEyeReduction = 0.0f;
};

struct TackModifier {
    std::string_view name;
    Hash itemHash;
    float healthCoreMod;
    float staminaCoreMod;
    bool isPlayerPassive = false; // Defaulted to false inline
};

struct CombinedTackModifiers {
    float totalHealthReduction = 0.0f;
    float totalStaminaReduction = 0.0f;
};