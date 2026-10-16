#pragma once
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

    bool wasHorseLeadingLastTick = false;
	bool wasHorseHitchedLastTick = false;
    
    float accumulatedHorseLeadHealthReward = 0.0f;
    int accumulatedHorseHitchedImmobility = 0;

    int lastTickInGameMinutes = 0;
    bool timescaleDeltaInitialised = false;

    Ped lastKnownPlayerPed = 0;
};

struct TransientState {
	int bathingResetTickCount = 0;
	bool isBathingStateActive = false;

    int jailedResetTickCount = 0;
    bool isJailedStateActive = false;
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
