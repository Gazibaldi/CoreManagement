/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <string_view>
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