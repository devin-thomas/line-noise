#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "game.h"

enum
{
    TEST_MAX_FRAMES = 36000
};

static void fail_check(const char *expression, int line)
{
    fprintf(stderr, "check failed at line %d: %s\n", line, expression);
    exit(1);
}

#define CHECK(expression) \
    do \
    { \
        if (!(expression)) \
        { \
            fail_check(#expression, __LINE__); \
        } \
    } while (0)

static LnGameDebugSnapshot snapshot(void)
{
    LnGameDebugSnapshot result;
    ln_game_debug_snapshot(&result);
    return result;
}

static void tap(u16 key)
{
    ln_game_update(key, key, 0);
    ln_game_update(0, 0, key);
}

static void idle_frames(uint32_t frames)
{
    for (uint32_t frame = 0; frame < frames; ++frame)
    {
        ln_game_update(0, 0, 0);
    }
}

static LnGameDebugSnapshot wait_for_call(uint8_t call_index,
                                        uint8_t state)
{
    for (uint32_t frame = 0; frame < TEST_MAX_FRAMES; ++frame)
    {
        LnGameDebugSnapshot current = snapshot();
        if (current.call_state[call_index] == state)
        {
            return current;
        }
        ln_game_update(0, 0, 0);
    }
    fail_check("call reached expected state", __LINE__);
    return snapshot();
}

static LnGameDebugSnapshot wait_for_counter(uint8_t counter_kind,
                                            uint8_t minimum)
{
    for (uint32_t frame = 0; frame < TEST_MAX_FRAMES; ++frame)
    {
        LnGameDebugSnapshot current = snapshot();
        uint8_t value = counter_kind == 0u
                            ? current.suppressed_leaks
                            : current.cabs_east;
        if (value >= minimum)
        {
            return current;
        }
        ln_game_update(0, 0, 0);
    }
    fail_check("counter reached expected value", __LINE__);
    return snapshot();
}

static void start_final_week(void)
{
    ln_game_init();
    tap(KEY_A);
    tap(KEY_A);
    tap(KEY_A);
    CHECK(snapshot().screen == LN_GAME_DEBUG_SCREEN_GAMEPLAY);
}

static void start_open_lines(void)
{
    ln_game_init();
    tap(KEY_A);
    tap(KEY_DOWN);
    tap(KEY_DOWN);
    tap(KEY_A);
    tap(KEY_A);
    CHECK(snapshot().screen == LN_GAME_DEBUG_SCREEN_GAMEPLAY);
}

static void connect_focused_call(uint8_t destination_slot)
{
    tap(KEY_A);
    for (uint8_t slot = 0; slot < destination_slot; ++slot)
    {
        tap(KEY_RIGHT);
    }
    tap(KEY_A);
}

static void test_finite_shift_cannot_end_early(void)
{
    start_final_week();
    uint32_t before_pause = snapshot().elapsed_frames;
    tap(KEY_START);
    tap(KEY_DOWN);
    tap(KEY_DOWN);
    tap(KEY_DOWN);
    tap(KEY_A);

    LnGameDebugSnapshot current = snapshot();
    CHECK(current.screen == LN_GAME_DEBUG_SCREEN_GAMEPLAY);
    CHECK(current.elapsed_frames <= before_pause + 1u);
}

static void test_open_lines_can_end_manually(void)
{
    start_open_lines();
    tap(KEY_START);
    tap(KEY_DOWN);
    tap(KEY_DOWN);
    tap(KEY_DOWN);
    tap(KEY_A);
    CHECK(snapshot().screen == LN_GAME_DEBUG_SCREEN_REPORT);
}

static void test_completed_call_never_rerings(void)
{
    start_final_week();
    wait_for_call(0, LN_GAME_DEBUG_CALL_RINGING);
    connect_focused_call(0);
    CHECK(snapshot().call_state[0] == LN_GAME_DEBUG_CALL_LIVE);

    for (uint32_t frame = 0; frame < TEST_MAX_FRAMES; ++frame)
    {
        if (snapshot().call_serviced[0] != 0u)
        {
            break;
        }
        ln_game_update(0, 0, 0);
    }
    CHECK(snapshot().call_serviced[0] != 0u);
    tap(KEY_A);
    LnGameDebugSnapshot cleared = snapshot();
    CHECK(cleared.call_state[0] == LN_GAME_DEBUG_CALL_COMPLETED);
    CHECK(cleared.cord_live[0] == 0u);

    uint8_t missed_before = cleared.missed_calls;
    idle_frames(50u * 60u);
    LnGameDebugSnapshot later = snapshot();
    CHECK(later.call_state[0] == LN_GAME_DEBUG_CALL_COMPLETED);
    CHECK(later.missed_calls == missed_before);
}

static void test_pickup_cancel_and_timeout_clear_selection(void)
{
    start_final_week();
    wait_for_call(0, LN_GAME_DEBUG_CALL_RINGING);
    tap(KEY_A);
    CHECK(snapshot().picked_call == 0u);
    tap(KEY_B);
    CHECK(snapshot().picked_call == LN_GAME_DEBUG_NO_SELECTION);
    CHECK(snapshot().repeat_frames == 0u);

    tap(KEY_A);
    CHECK(snapshot().picked_call == 0u);
    idle_frames(46u * 60u);
    CHECK(snapshot().picked_call == LN_GAME_DEBUG_NO_SELECTION);
}

static void test_monitor_repeat_does_not_survive_overlays(void)
{
    start_final_week();
    tap(KEY_B);
    CHECK(snapshot().repeat_frames > 0u);
    tap(KEY_START);
    CHECK(snapshot().repeat_frames == 0u);
    tap(KEY_START);
    CHECK(snapshot().screen == LN_GAME_DEBUG_SCREEN_GAMEPLAY);
    ln_game_update(0, 0, KEY_B);
    CHECK(snapshot().repeat_frames == 0u);
}

static void test_one_ground_clamp_limit(void)
{
    start_final_week();
    wait_for_call(0, LN_GAME_DEBUG_CALL_RINGING);
    connect_focused_call(0);

    wait_for_call(1, LN_GAME_DEBUG_CALL_RINGING);
    tap(KEY_DOWN);
    connect_focused_call(1);

    wait_for_call(2, LN_GAME_DEBUG_CALL_RINGING);
    tap(KEY_DOWN);
    connect_focused_call(2);
    CHECK(snapshot().crossing_count >= 2u);

    tap(KEY_DOWN);
    tap(KEY_DOWN);
    CHECK(snapshot().focus == LN_GAME_DEBUG_FOCUS_CROSSING);
    tap(KEY_R);
    CHECK(snapshot().grounded_count == 1u);
    tap(KEY_RIGHT);
    tap(KEY_R);
    CHECK(snapshot().grounded_count == 1u);
}

static void test_suppression_does_not_score_as_useful_resonance(void)
{
    start_open_lines();
    wait_for_call(0, LN_GAME_DEBUG_CALL_RINGING);
    connect_focused_call(0);
    wait_for_call(2, LN_GAME_DEBUG_CALL_RINGING);
    tap(KEY_DOWN);
    tap(KEY_DOWN);
    connect_focused_call(2);
    CHECK(snapshot().crossing_count > 0u);
    tap(KEY_R);
    wait_for_counter(0, 1);
    CHECK(snapshot().useful_leaks == 0u);

    tap(KEY_START);
    tap(KEY_DOWN);
    tap(KEY_DOWN);
    tap(KEY_DOWN);
    tap(KEY_A);
    LnGameDebugSnapshot report = snapshot();
    CHECK(report.screen == LN_GAME_DEBUG_SCREEN_REPORT);
    CHECK(report.suppressed_leaks == 1u);
    CHECK(report.score_resonance == 0u);
}

static void test_payloadless_call_has_no_fact(void)
{
    start_final_week();
    wait_for_call(1, LN_GAME_DEBUG_CALL_RINGING);
    tap(KEY_DOWN);
    connect_focused_call(1);
    CHECK(snapshot().packet_fact_id[1] == LN_GAME_DEBUG_NO_SELECTION);
}

static void test_expired_fact_cannot_be_delivered(void)
{
    start_open_lines();
    idle_frames(361u * 60u);
    wait_for_call(0, LN_GAME_DEBUG_CALL_RINGING);
    connect_focused_call(0);
    for (uint32_t frame = 0; frame < TEST_MAX_FRAMES; ++frame)
    {
        if (snapshot().call_serviced[0] != 0u)
        {
            break;
        }
        ln_game_update(0, 0, 0);
    }
    LnGameDebugSnapshot current = snapshot();
    CHECK(current.call_serviced[0] != 0u);
    CHECK(current.owen_platform_source_mask == 0u);
    CHECK((current.objective_pass_mask & 1u) == 0u);
}

static void test_authored_route_reaches_coherent_dawn(void)
{
    start_final_week();
    CHECK(snapshot().last_monitored_call == LN_GAME_DEBUG_NO_SELECTION);
    ln_game_update(0, 0, KEY_B);
    CHECK(snapshot().repeat_frames == 0u);

    wait_for_call(0, LN_GAME_DEBUG_CALL_RINGING);
    connect_focused_call(0);
    for (uint32_t frame = 0; frame < TEST_MAX_FRAMES; ++frame)
    {
        if (snapshot().platform_sources == 1u)
        {
            break;
        }
        ln_game_update(0, 0, 0);
    }
    CHECK(snapshot().platform_sources == 1u);
    idle_frames(11u * 60u);
    CHECK(snapshot().platform_sources == 1u);
    for (uint32_t frame = 0; frame < 2u * 60u; ++frame)
    {
        if (snapshot().platform_sources == 2u)
        {
            break;
        }
        ln_game_update(0, 0, 0);
    }
    CHECK(snapshot().platform_sources == 2u);
    CHECK(snapshot().owen_platform_source_mask ==
          ((uint32_t)(1u << 0) | (uint32_t)(1u << 16)));

    wait_for_call(2, LN_GAME_DEBUG_CALL_RINGING);
    tap(KEY_DOWN);
    tap(KEY_DOWN);
    connect_focused_call(2);
    CHECK(snapshot().crossing_count > 0u);
    tap(KEY_R);
    wait_for_counter(0, 1);
    wait_for_counter(1, 3);
    CHECK(snapshot().privacy_leaks == 0u);

    tap(KEY_DOWN);
    tap(KEY_DOWN);
    CHECK(snapshot().focus == LN_GAME_DEBUG_FOCUS_CROSSING);
    tap(KEY_A);

    for (uint32_t frame = 0; frame < TEST_MAX_FRAMES; ++frame)
    {
        if (snapshot().screen == LN_GAME_DEBUG_SCREEN_REPORT)
        {
            break;
        }
        ln_game_update(0, 0, 0);
    }
    LnGameDebugSnapshot report = snapshot();
    CHECK(report.screen == LN_GAME_DEBUG_SCREEN_REPORT);
    CHECK(report.objective_pass_mask == 0x0fu);
    CHECK(report.shift_complete != 0u);
    CHECK(report.privacy_leaks == 0u);
    CHECK(report.cabs_east == 3u);
    CHECK(report.platform_sources >= 2u);
    CHECK(report.score_resonance == 100u);
    CHECK(report.score_coherence == 100u);
    CHECK(report.epilogue_index == 0u);
}

int main(void)
{
    test_finite_shift_cannot_end_early();
    test_open_lines_can_end_manually();
    test_completed_call_never_rerings();
    test_pickup_cancel_and_timeout_clear_selection();
    test_monitor_repeat_does_not_survive_overlays();
    test_one_ground_clamp_limit();
    test_suppression_does_not_score_as_useful_resonance();
    test_payloadless_call_has_no_fact();
    test_expired_fact_cannot_be_delivered();
    test_authored_route_reaches_coherent_dawn();
    puts("gameplay probe: PASS");
    return 0;
}
