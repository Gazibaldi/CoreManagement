/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <string_view>
#include "..\..\inc\types.h"

enum class CoreIndex : int { Health = 0, Stamina = 1, DeadEye = 2 };
enum class HorseSpeed : int { Trot = 4, Canter = 6, Gallop = 10 };
enum class PlayerSpeed : int { Jog = 3, Sprint = 5 };
enum class BathingState { NotBathing = 0, StandardBathing = 1, DeluxeBathing = 2 };
enum class SleepingState { NotSleeping, Wilderness, Hotel };
enum class StateChangeIntervalType { NotSet, Standard, Restricted };

enum ePedAttribute
{
    PA_HEALTH,
    PA_STAMINA,
    PA_SPECIALABILITY,
    PA_COURAGE,
    PA_AGILITY,
    PA_SPEED,
    PA_ACCELERATION,
    PA_BONDING,
    SA_HUNGER,
    SA_FATIGUED,
    SA_INEBRIATED,
    SA_POISONED,
    SA_BODYHEAT,
    SA_BODYWEIGHT,
    SA_OVERFED,
    SA_SICKNESS,
    SA_DIRTINESS,
    SA_DIRTINESSHAT,
    MTR_STRENGTH,
    MTR_GRIT,
    MTR_INSTINCT,
    PA_UNRULINESS,
    SA_DIRTINESSSKIN
};

struct SimulationState {
    bool isRestricted = false;
	bool monitorTransientVariablesDuringRestriction = false;
	
    int cachedInGameTimeMinutes = 0;
    int accumulatedTimeMs = 0;

	bool isNightTime = false;
	int cachedGameTimescaleMs = 0;

    float cachedHealth = 100.0f;
    float cachedStamina = 100.0f;
    float cachedDeadEye = 100.0f;
    int cachedMoney = 0;

    BathingState restrictedBathingState = BathingState::NotBathing;
    SleepingState restrictedSleepingState = SleepingState::NotSleeping;

    bool wasSleepingDuringRestriction = false;
    bool wasInMinigameDuringRestriction = false;
    bool wasInMissionDuringRestriction = false;
    bool wasInTimeWarpDuringRestriction = false;
    bool wasRestrictedLastTick = false;
	
    float PostMissionDecayHoursRemaining = 0.0f;    

    bool hadHorse = false;

    float cachedHorseHealth = 100.0f;
    float cachedHorseStamina = 100.0f;

    int lastTickInGameMinutes = 0;
    bool timescaleDeltaInitialised = false;
    StateChangeIntervalType stateChangeTickIntervalMsType = StateChangeIntervalType::NotSet;
    
    Ped lastKnownPlayerPed = 0;
    Ped lastKnownHorsePed = 0;
	bool configStructuresInitialised = false;
};

struct GameplayContext {
    bool freezeActive = false;
    bool monitorTransientVarsDuringFreeze = false;
    bool isInMission = false;
    bool EnableStandardMissionDecay = false;
    bool isInMinigame = false;
    bool EnableStandardMinigameDecay = false;
    bool isInShop = false;
    bool isSleeping = false;
    bool isAtCamp = false;
    bool isJailed = false;
    bool isBathing = false;
	bool isInTimeWarp = false;
};

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