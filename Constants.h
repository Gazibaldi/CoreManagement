#pragma once

#include <unordered_set>
#include <string>

const float HORSE_LEAD_REWARD_PER_GAME_HOUR = 10.0f;

const int HORSE_HITCHED_REQ = 6;
const float HORSE_HITCHED_HEALTH_REQ = 60.0f;
const float HORSE_HITCHED_BONUS_MULT = 1.0f;

const float POST_MISSION_DECAY_MULTIPLIER = 0.2f;
const float POST_MISSION_DECAY_HOURS = 1.0f;

const float OVERSLEEP_THRESHOLD_HOURS = 12.0f;
const float MIN_SLEEP_HOURS = 8.0f;

const float TIMESKIP_MAX_HOURS = 12.0f;
const float MINIGAME_TIMESKIP_MAX_HOURS = 3.0f;

const float BATHING_TIMESKIP_MAX_HOURS = 2.0f;
const std::unordered_set<std::string> VALID_BATHING_STATES{ "BATHING", "Bathing", "Scrub_Idle", "Scrub_Head", "Scrub_Left_Arm", "Scrub_Right_Arm", "Scrub_Left_Leg", "Scrub_Right_Leg" };

const float PROVISION_TRINKET_OWL_FEATHER_PLAYER_HSD_MULTIPLIER = 0.15f;
const float PROVISION_TALISMAN_BEAR_CLAW_PLAYER_H_MULTIPLIER = 0.1f;
const float PROVISION_TALISMAN_BISON_HORN_PLAYER_S_MULTIPLIER = 0.1f;
const float PROVISION_TALISMAN_ALLIGATOR_TOOTH_PLAYER_D_MULTIPLIER = 0.1f;
const float PROVISION_TALISMAN_BOAR_TUSK_HORSE_HS_MULTIPLIER = 0.1f;

