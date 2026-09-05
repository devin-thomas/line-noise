#ifndef LINE_NOISE_GAME_H
#define LINE_NOISE_GAME_H

#include <gba.h>
#include <stdint.h>

void ln_game_init(void);
void ln_game_update(u16 pressed, u16 held, u16 released);
void ln_game_render(void);

#ifdef LN_GAME_DEBUG
enum
{
    LN_GAME_DEBUG_SCREEN_GAMEPLAY = 3,
    LN_GAME_DEBUG_SCREEN_REPORT = 6,
    LN_GAME_DEBUG_CALL_SCHEDULED = 0,
    LN_GAME_DEBUG_CALL_RINGING = 1,
    LN_GAME_DEBUG_CALL_LIVE = 2,
    LN_GAME_DEBUG_CALL_HUNG_UP = 3,
    LN_GAME_DEBUG_CALL_COMPLETED = 4,
    LN_GAME_DEBUG_FOCUS_CROSSING = 4,
    LN_GAME_DEBUG_NO_SELECTION = 255
};

typedef struct LnGameDebugSnapshot
{
    uint8_t screen;
    uint8_t mode;
    uint8_t focus;
    uint8_t picked_call;
    uint8_t selected_destination;
    uint8_t selected_crossing;
    uint8_t crossing_count;
    uint8_t grounded_count;
    uint8_t call_state[4];
    uint8_t call_serviced[4];
    uint8_t cord_live[4];
    uint8_t packet_fact_id[4];
    uint8_t missed_calls;
    uint8_t privacy_leaks;
    uint8_t useful_leaks;
    uint8_t suppressed_leaks;
    uint8_t cabs_east;
    uint8_t platform_sources;
    uint8_t objective_pass_mask;
    uint8_t shift_complete;
    uint8_t score_service;
    uint8_t score_discretion;
    uint8_t score_resonance;
    uint8_t score_coherence;
    uint8_t epilogue_index;
    uint8_t last_monitored_call;
    uint16_t repeat_frames;
    uint32_t elapsed_frames;
    uint32_t owen_platform_source_mask;
} LnGameDebugSnapshot;

void ln_game_debug_snapshot(LnGameDebugSnapshot *snapshot);
#endif

#endif
