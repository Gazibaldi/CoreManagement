/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once

#include <unordered_set>
#include <string>

const float HORSE_HITCHED_HEALTH_REQ = 60.0f;

const float POST_MISSION_DECAY_ADDITIVE_MULTIPLIER = 0.2f;
const float POST_MISSION_DECAY_HOURS = 1.0f;

const float OVERSLEEP_THRESHOLD_HOURS = 12.0f;
const float MIN_SLEEP_HOURS = 8.0f;

const float TIMESKIP_MAX_HOURS = 12.0f;
const float MINIGAME_TIMESKIP_MAX_HOURS = 3.0f;

const float BATHING_TIMESKIP_MAX_HOURS = 2.0f;
const std::unordered_set<std::string> VALID_BATHING_STATES{ "BATHING", "Bathing", "Scrub_Idle", "Scrub_Head", "Scrub_Left_Arm", "Scrub_Right_Arm", "Scrub_Left_Leg", "Scrub_Right_Leg" };