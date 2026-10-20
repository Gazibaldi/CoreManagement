/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/
#include <tuple>
#include "..\CoreSDK\inc\natives.h"
#include "HorseManager.h"
#include "Logger.h"
#include "Config.h"

HorseManager g_HorseManager;

const float HORSE_MAX_DISTANCE_FROM_PLAYER = 400.0f; // metres

float HorseManager::GetHorseDistanceFromPlayer(Ped playerPed, Ped horsePed) noexcept {
    Vector3 playerCoords = ENTITY::GET_ENTITY_COORDS(playerPed, true, true);
    Vector3 horseCoords = ENTITY::GET_ENTITY_COORDS(horsePed, true, true);

    // Calculate distance in metres (using 3D distance)
    float distance = MISC::GET_DISTANCE_BETWEEN_COORDS(
        playerCoords.x, playerCoords.y, playerCoords.z,
        horseCoords.x, horseCoords.y, horseCoords.z,
        true
    );

    return distance;
}

std::tuple<Ped, bool> HorseManager::GetLedHorseFromPlayer(Ped playerPed) noexcept {
    if (!TASK::_IS_PED_LEADING_HORSE(playerPed)) return { 0, false };

    Ped ledHorse = TASK::_GET_LED_HORSE_FROM_PED(playerPed);

    WriteLog(LogLevel::Dev, "Checking if horse is being led.");

    return { ledHorse, TASK::_IS_PED_BEING_LED(ledHorse) };
}

[[nodiscard]] std::tuple<Ped, bool> HorseManager::GetActivePlayerHorse(Ped playerPed, ActiveHorseType horseType) noexcept {

    Ped activeHorse = PED::GET_MOUNT(playerPed);

    if (horseType == ActiveHorseType::Mounted) {
        if (activeHorse != 0 &&
            ENTITY::DOES_ENTITY_EXIST(activeHorse) &&
            !ENTITY::IS_ENTITY_DEAD(activeHorse)) {
            WriteLog(LogLevel::Dev, "Player is mounted on a horse.");
            return { activeHorse, true };
        }
        else {
            WriteLog(LogLevel::Dev, "Player is not mounted on a horse.");
            return { 0, false };
        }
    }

    // If the player is not mounted, check for their active horse (the one they have saddled)
    // We disregard the mounted horse if the horseType is Active, as we only want to check for the active saddled horse in that case.
    // But if the horseType is Both, we will fall back to the mounted horse if they don't have an active saddled horse.
    if (horseType == ActiveHorseType::Active || activeHorse == 0)
        activeHorse = PLAYER::_GET_ACTIVE_HORSE_FOR_PLAYER(PLAYER::PLAYER_ID());

    bool hasHorse = (activeHorse != 0 &&
        ENTITY::DOES_ENTITY_EXIST(activeHorse) &&
        !ENTITY::IS_ENTITY_DEAD(activeHorse));

    if (hasHorse) {
        float distance = GetHorseDistanceFromPlayer(playerPed, activeHorse);

        WriteLog(LogLevel::Dev, "Active horse detected. Distance from player: " + std::to_string(distance) + " metres.");

        // If the horse is too far away, we consider it "lost" and reset the state.
        // HORSE_MAX_DISTANCE_FROM_PLAYER (400) metres is a generous distance, 350m is the Bonding Level 4 max whistle distance.
        // Deters exploiting the drain mechanics for the horse by leaving it in a distant location while the player is elsewhere.
        if (distance > HORSE_MAX_DISTANCE_FROM_PLAYER) {
            WriteLog(LogLevel::Verbose, "Active horse is too far away (" + std::to_string(distance) + " metres). Assuming it is lost.");
            hasHorse = false;
            activeHorse = 0;
        }
    }

    return { activeHorse, hasHorse };
}

[[nodiscard]] bool HorseManager::IsHorseHitched(const CoreConfigSettings& coreConfig) noexcept {
    WriteLog(LogLevel::Dev, "Getting horse hitched state.");
    std::lock_guard<std::mutex> guard(m_Lock);

    // Calculate the required number of samples based on the configured tick intervals
    // This ensures that the horse is considered "hitched" if it has been detected as immobile for at least 66% of the samples in the drain tick interval
    int requiredSamples = static_cast<int>((coreConfig.coreDrainTickIntervalMs / coreConfig.stateChangeTickIntervalMs) * 0.66);

    return (m_HorseImmobileSamples >= requiredSamples);
}

[[nodiscard]] bool HorseManager::IsHorseBeingLed(const CoreConfigSettings& coreConfig) noexcept {
    WriteLog(LogLevel::Dev, "Getting horse being led state.");
    std::lock_guard<std::mutex> guard(m_Lock);

    // Calculate the required number of samples based on the configured tick intervals
    // This ensures that the horse is considered "being led" if it has been detected as such for at least 75% of the samples in the drain tick interval
    int requiredSamples = static_cast<int>((coreConfig.coreDrainTickIntervalMs / coreConfig.stateChangeTickIntervalMs) * 0.75);

    return (m_HorseLedSamples >= requiredSamples);
}

void HorseManager::RecordSnapshot(Ped playerPed) noexcept {
    WriteLog(LogLevel::Dev, "Recording movement snapshot for horse.");
    std::lock_guard<std::mutex> guard(m_Lock);

    // Check if the player has an active horse (saddled only)
    // We only want to track the horse's speed if the player is unmounted or leading it
    // mounted sppeed is tracked in MovementVelocityTracker
    auto [horsePed, hasHorse] = GetActivePlayerHorse(playerPed, ActiveHorseType::Active);
    if (hasHorse) {
        WriteLog(LogLevel::Dev, "Player has an active horse.");
        float horseSpeed = ENTITY::GET_ENTITY_SPEED(horsePed);

        // Track immobile horse samples to determine if the horse is hitched
        if (horseSpeed == 0.0f) {
            WriteLog(LogLevel::Dev, "Horse is immobile. Incrementing immobile sample count.");
            m_HorseImmobileSamples++;
        }
        else {
            WriteLog(LogLevel::Dev, "Horse is mobile. Resetting immobile sample count.");
            m_HorseImmobileSamples = 0;

            // Track led horse samples to determine if the horse is being led
            auto [horsePed, isBeingLed] = GetLedHorseFromPlayer(playerPed);
            if (isBeingLed) {
                WriteLog(LogLevel::Dev, "Player is leading the horse.");
                m_HorseLedSamples++;
            }
            else {
                WriteLog(LogLevel::Dev, "Player is not leading the horse.");
            }
        }
    }
    else {
        WriteLog(LogLevel::Dev, "Player does not have an active horse.");
    }
}

[[nodiscard]] void HorseManager::ResetHitchedOrLedTracking() noexcept {
    WriteLog(LogLevel::Dev, "Resetting HorseManager Hitched/Led state.");

    std::lock_guard<std::mutex> guard(m_Lock);
    
    m_HorseImmobileSamples = 0;
    m_HorseLedSamples = 0;
}