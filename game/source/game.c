#include "game.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "content.h"
#include "video.h"

enum
{
    LN_FRAMES_PER_SECOND = 60,
    LN_FINAL_SHIFT_SECONDS = 8 * 60,
    LN_SCHOOL_SHIFT_SECONDS = 3 * 60,
    LN_MAX_PATH_CELLS = 24,
    LN_MAX_CROSSINGS = 8,
    LN_MAX_EVENTS = 32,
    LN_EVENT_TEXT_LENGTH = 28,
    LN_NO_SELECTION = 255,
    LN_CROSSING_WINDOW = 152,
    LN_MESSAGE_FRAMES = 180,
    LN_REPEAT_FRAMES = 150,
    LN_MONITOR_HOLD_FRAMES = 10,
    LN_VERIFY_CALLBACK_SECONDS = 12,
    LN_SOURCE_X = 6,
    LN_DESTINATION_X = 14,
    LN_PEG_X = 10,
    LN_CITY_TILE_BASE = 192,
    LN_CITY_MAP_COLUMNS = 7,
    LN_SOURCE_RAIL = LN_CITIZEN_COUNT,
    LN_OBJECTIVE_ALL_MASK = (1u << LN_OBJECTIVE_COUNT) - 1u
};

typedef enum LnScreen
{
    LN_SCREEN_TITLE = 0,
    LN_SCREEN_MODE_SELECT,
    LN_SCREEN_BRIEF,
    LN_SCREEN_GAMEPLAY,
    LN_SCREEN_PAUSE,
    LN_SCREEN_MEMORY,
    LN_SCREEN_REPORT,
    LN_SCREEN_EPILOGUE,
    LN_SCREEN_PLAYBACK
} LnScreen;

typedef enum LnMode
{
    LN_MODE_FINAL_WEEK = 0,
    LN_MODE_OPERATOR_SCHOOL,
    LN_MODE_OPEN_LINES,
    LN_MODE_PLAYBACK,
    LN_MODE_COUNT
} LnMode;

typedef enum LnCallState
{
    LN_CALL_SCHEDULED = 0,
    LN_CALL_RINGING,
    LN_CALL_LIVE,
    LN_CALL_HUNG_UP,
    LN_CALL_COMPLETED
} LnCallState;

typedef enum LnFocus
{
    LN_FOCUS_CALL_0 = 0,
    LN_FOCUS_CALL_1,
    LN_FOCUS_CALL_2,
    LN_FOCUS_CALL_3,
    LN_FOCUS_CROSSING,
    LN_FOCUS_PEG,
    LN_FOCUS_MEMORY,
    LN_FOCUS_COUNT
} LnFocus;

typedef struct LnCell
{
    uint8_t x;
    uint8_t y;
} LnCell;

typedef struct LnCallRuntime
{
    LnCallState state;
    uint16_t scheduled_second;
    uint16_t ring_frames;
    uint8_t retry_count;
    uint8_t delivered_mask;
    bool serviced;
} LnCallRuntime;

typedef struct LnCord
{
    bool live;
    uint8_t call_index;
    uint8_t peg;
    uint8_t palette_bank;
    uint8_t path_count;
    LnCell path[LN_MAX_PATH_CELLS];
    uint16_t phase;
    uint16_t speed;
    uint8_t payload_cursor;
    uint8_t packet_flash_frames;
} LnCord;

typedef struct LnCrossing
{
    uint8_t cord_a;
    uint8_t cord_b;
    uint8_t path_index_a;
    uint8_t path_index_b;
    uint8_t x;
    uint8_t y;
    uint8_t top_cord;
    uint8_t flash_frames;
    bool grounded;
    bool resonance_latched;
} LnCrossing;

typedef struct LnEvent
{
    uint16_t second;
    char first[LN_EVENT_TEXT_LENGTH];
    char second_line[LN_EVENT_TEXT_LENGTH];
} LnEvent;

typedef struct LnState
{
    LnScreen screen;
    LnMode mode;
    uint8_t menu_selection;
    uint8_t focus;
    uint8_t selected_destination;
    uint8_t picked_call;
    uint8_t selected_crossing;
    uint8_t selected_cord;
    uint8_t selected_fact;
    uint8_t pause_selection;
    uint8_t playback_offset;
    uint8_t monitored_call;
    uint8_t last_monitored_call;
    uint16_t b_hold_frames;
    uint16_t repeat_frames;
    uint32_t elapsed_frames;
    uint32_t shift_duration_frames;
    uint16_t verify_callback_frames;
    uint16_t message_frames;
    uint16_t last_header_second;
    bool screen_dirty;
    bool board_built;
    bool high_contrast;
    bool reduced_flash;
    bool bea_bulletin_prepared;
    bool warning_delivered;
    bool owen_verified;
    bool felix_confirmed;
    bool shift_complete;
    uint8_t service_errors;
    uint8_t missed_calls;
    uint8_t privacy_leaks;
    uint8_t useful_leaks;
    uint8_t suppressed_leaks;
    uint8_t cabs_east;
    uint8_t platform_sources;
    uint8_t score_service;
    uint8_t score_discretion;
    uint8_t score_resonance;
    uint8_t score_coherence;
    uint8_t epilogue_index;
    uint16_t knowledge_mask[LN_CITIZEN_COUNT];
    uint8_t confidence[LN_CITIZEN_COUNT][LN_FACT_COUNT];
    uint32_t source_mask[LN_CITIZEN_COUNT][LN_FACT_COUNT];
    LnCallRuntime calls[LN_CALL_COUNT];
    LnCord cords[LN_CALL_COUNT];
    LnCrossing crossings[LN_MAX_CROSSINGS];
    uint8_t crossing_count;
    LnEvent events[LN_MAX_EVENTS];
    uint8_t event_count;
    char message_first[LN_EVENT_TEXT_LENGTH];
    char message_second[LN_EVENT_TEXT_LENGTH];
} LnState;

EWRAM_BSS static LnState s_state;

static const uint8_t LN_CALL_ROWS[LN_CALL_COUNT] = {4, 6, 8, 10};
static const uint8_t LN_DESTINATION_ROWS[DESTINATION_SLOT_COUNT] = {4, 6, 8, 10};
static const uint8_t LN_PEG_ROWS[3] = {4, 7, 10};
static const uint8_t LN_DEFAULT_PEG[LN_CALL_COUNT] = {1, 2, 0, LN_NO_SELECTION};
static const uint16_t LN_CORD_SPEEDS[LN_CALL_COUNT] = {13, 15, 17, 19};
static const uint8_t LN_CITY_MAP[LN_CITY_MAP_COLUMNS * 7] = {
    11, 16, 4, 4, 22, 11, 12,
    5, 19, 5, 12, 5, 23, 5,
    6, 4, 6, 4, 6, 4, 6,
    5, 21, 5, 27, 5, 20, 5,
    6, 4, 6, 4, 6, 4, 6,
    5, 25, 5, 26, 5, 24, 5,
    12, 16, 4, 4, 30, 11, 13
};

static uint8_t fact_index_from_id(uint8_t fact_id)
{
    for (uint8_t index = 0; index < LN_FACT_COUNT; ++index)
    {
        if (LN_FACTS[index].id == fact_id)
        {
            return index;
        }
    }
    return LN_NO_SELECTION;
}

static uint8_t destination_citizen(uint8_t destination_slot)
{
    if (destination_slot == DESTINATION_SLOT_OWEN)
    {
        return 1;
    }
    if (destination_slot == DESTINATION_SLOT_CAL)
    {
        return 5;
    }
    return LN_NO_SELECTION;
}

static uint8_t clamped_score(int value)
{
    if (value < 0)
    {
        return 0;
    }
    if (value > 100)
    {
        return 100;
    }
    return (uint8_t)value;
}

static uint16_t current_shift_second(void)
{
    return (uint16_t)(s_state.elapsed_frames / LN_FRAMES_PER_SECOND);
}

static bool fact_is_active(uint8_t fact_id)
{
    uint8_t fact_index = fact_index_from_id(fact_id);
    if (fact_index == LN_NO_SELECTION)
    {
        return false;
    }
    uint16_t expiry = LN_FACTS[fact_index].expiry_second;
    return expiry == LN_NO_EXPIRY || current_shift_second() < expiry;
}

static uint8_t count_sources(uint32_t mask)
{
    uint8_t count = 0;
    while (mask != 0u)
    {
        count = (uint8_t)(count + (uint8_t)(mask & 1u));
        mask >>= 1;
    }
    return count;
}

static bool citizen_knows_fact(uint8_t citizen_id, uint8_t fact_id,
                               uint8_t minimum_confidence)
{
    uint8_t fact_index = fact_index_from_id(fact_id);
    if (citizen_id >= LN_CITIZEN_COUNT || fact_index == LN_NO_SELECTION)
    {
        return false;
    }
    uint16_t fact_bit = (uint16_t)(1u << fact_index);
    return (s_state.knowledge_mask[citizen_id] & fact_bit) != 0u &&
           s_state.confidence[citizen_id][fact_index] >= minimum_confidence;
}

static uint8_t verification_citizen(uint8_t fact_id)
{
    for (uint8_t index = 0; index < LN_OBJECTIVE_COUNT; ++index)
    {
        const ObjectiveSpec *objective = &LN_OBJECTIVES[index];
        if (objective->type == OBJECTIVE_KNOWLEDGE &&
            objective->fact_id == fact_id &&
            objective->actor_id < LN_CITIZEN_COUNT)
        {
            return objective->actor_id;
        }
    }
    return LN_NO_SELECTION;
}

static bool objective_passes(uint8_t objective_index)
{
    const ObjectiveSpec *objective = &LN_OBJECTIVES[objective_index];
    uint8_t fact_index = fact_index_from_id(objective->fact_id);
    if (objective->type == OBJECTIVE_KNOWLEDGE)
    {
        return citizen_knows_fact(objective->actor_id, objective->fact_id,
                                  objective->minimum_confidence);
    }
    if (objective->type == OBJECTIVE_WORLD_STATE)
    {
        return s_state.cabs_east >= objective->minimum_value;
    }
    if (objective->type == OBJECTIVE_PRIVACY)
    {
        if (fact_index == LN_NO_SELECTION)
        {
            return false;
        }
        for (uint8_t citizen = 0; citizen < LN_CITIZEN_COUNT; ++citizen)
        {
            if ((objective->forbidden_receiver_mask & (1u << citizen)) != 0u &&
                (s_state.knowledge_mask[citizen] & (1u << fact_index)) != 0u)
            {
                return false;
            }
        }
        return true;
    }
    if (objective->type == OBJECTIVE_VERIFICATION)
    {
        uint8_t citizen = verification_citizen(objective->fact_id);
        return citizen != LN_NO_SELECTION && fact_index != LN_NO_SELECTION &&
               count_sources(s_state.source_mask[citizen][fact_index]) >=
                   objective->minimum_value;
    }
    return false;
}

static uint8_t objective_pass_mask(void)
{
    uint8_t result = 0;
    for (uint8_t index = 0; index < LN_OBJECTIVE_COUNT; ++index)
    {
        if (objective_passes(index))
        {
            result |= (uint8_t)(1u << index);
        }
    }
    return result;
}

static void refresh_platform_sources(void)
{
    uint8_t fact_index = fact_index_from_id(1);
    if (fact_index == LN_NO_SELECTION)
    {
        s_state.platform_sources = 0;
        return;
    }
    s_state.platform_sources =
        count_sources(s_state.source_mask[1][fact_index]);
}

static void copy_event_text(char *destination, const char *source)
{
    if (source == NULL)
    {
        destination[0] = '\0';
        return;
    }
    strncpy(destination, source, LN_EVENT_TEXT_LENGTH - 1u);
    destination[LN_EVENT_TEXT_LENGTH - 1u] = '\0';
}

static void set_message(const char *first, const char *second)
{
    copy_event_text(s_state.message_first, first);
    copy_event_text(s_state.message_second, second);
    s_state.message_frames = LN_MESSAGE_FRAMES;
}

static void add_event(const char *first, const char *second)
{
    if (s_state.event_count >= LN_MAX_EVENTS)
    {
        memmove(&s_state.events[0], &s_state.events[1],
                sizeof(s_state.events[0]) * (LN_MAX_EVENTS - 1u));
        s_state.event_count = LN_MAX_EVENTS - 1u;
    }
    LnEvent *event = &s_state.events[s_state.event_count++];
    event->second = (uint16_t)(s_state.elapsed_frames / LN_FRAMES_PER_SECOND);
    copy_event_text(event->first, first);
    copy_event_text(event->second_line, second);
    set_message(first, second);
}

static void enter_screen(LnScreen screen)
{
    if (screen != LN_SCREEN_GAMEPLAY)
    {
        s_state.picked_call = LN_NO_SELECTION;
        s_state.monitored_call = LN_NO_SELECTION;
        s_state.b_hold_frames = 0;
        s_state.repeat_frames = 0;
    }
    s_state.screen = screen;
    s_state.screen_dirty = true;
    s_state.board_built = false;
}

static void draw_box(int x, int y, int width, int height,
                     uint16_t fill_tile, uint16_t border_tile,
                     uint8_t palette_bank)
{
    ln_video_fill(1, x, y, width, height, fill_tile, palette_bank);
    ln_video_fill(1, x, y, width, 1, border_tile, palette_bank);
    ln_video_fill(1, x, y + height - 1, width, 1,
                  border_tile, palette_bank);
    ln_video_fill(1, x, y, 1, height, border_tile, palette_bank);
    ln_video_fill(1, x + width - 1, y, 1, height,
                  border_tile, palette_bank);
}

static void draw_document_base(const char *title, uint8_t scene)
{
    ln_video_load_interface(scene, s_state.high_contrast);
    ln_video_clear_text_rect(0, 0, LN_MAP_COLUMNS, LN_MAP_ROWS);
    ln_video_clear_map(2, 0, 2);
    ln_video_clear_map(3, 0, 2);
    ln_video_fill(1, 0, 0, 30, 20, 0, 0);
    draw_box(1, 1, 28, 19, 31, 16, 0);
    ln_video_fill(1, 2, 3, 26, 1, 18, 0);
    ln_video_text(0, 2, 2, title, 8);
}

static void draw_wrapped(int x, int y, int width, int max_lines,
                         const char *text, uint8_t palette_bank)
{
    char line[32];
    int line_length = 0;
    int line_count = 0;
    const char *cursor = text;

    while (*cursor != '\0' && line_count < max_lines)
    {
        while (*cursor == ' ')
        {
            ++cursor;
        }
        if (*cursor == '\0')
        {
            break;
        }

        const char *word_start = cursor;
        int word_length = 0;
        while (cursor[word_length] != '\0' && cursor[word_length] != ' ')
        {
            ++word_length;
        }

        if (line_length > 0 && line_length + 1 + word_length > width)
        {
            line[line_length] = '\0';
            ln_video_text(0, x, y + line_count, line, palette_bank);
            ++line_count;
            line_length = 0;
            if (line_count >= max_lines)
            {
                break;
            }
        }
        if (line_length > 0)
        {
            line[line_length++] = ' ';
        }
        for (int index = 0;
             index < word_length && line_length < width;
             ++index)
        {
            line[line_length++] = word_start[index];
        }
        cursor += word_length;
    }

    if (line_length > 0 && line_count < max_lines)
    {
        line[line_length] = '\0';
        ln_video_text(0, x, y + line_count, line, palette_bank);
    }
}

static void render_mode_select(void)
{
    static const char *const options[LN_MODE_COUNT] = {
        "FINAL WEEK",
        "OPERATOR SCHOOL",
        "OPEN LINES",
        "PLAYBACK"
    };
    static const char *const details[LN_MODE_COUNT] = {
        "PLATFORM THREE / AUTHORED SHIFT",
        "GUIDED DETERMINISTIC PRACTICE",
        "UNBOUNDED EXCHANGE SIMULATION",
        "REVIEW THE LAST CAUSAL CHAIN"
    };

    draw_document_base("LINE NOISE!", 0);
    ln_video_text(0, 2, 4, "MODES", 8);
    for (uint8_t index = 0; index < LN_MODE_COUNT; ++index)
    {
        int row = 6 + (int)index * 2;
        ln_video_text(0, 3, row,
                      index == s_state.menu_selection ? ">" : " ", 8);
        ln_video_text(0, 5, row, options[index], 8);
    }
    draw_wrapped(2, 14, 26, 2, details[s_state.menu_selection], 8);
    ln_video_text(0, 2, 17, "A SELECT   B TITLE", 8);
}

static void render_brief(void)
{
    char objective[28];
    draw_document_base("PLATFORM THREE", s_state.mode == LN_MODE_OPERATOR_SCHOOL ? 4 : 0);
    ln_video_text(0, 2, 4, "23:47 / CENTRAL EXCHANGE", 8);
    ln_video_text(0, 2, 6, "OBJECTIVES", 8);
    for (uint8_t index = 0; index < LN_OBJECTIVE_COUNT; ++index)
    {
        snprintf(objective, sizeof(objective), "%u %s",
                 (unsigned)(index + 1u), LN_OBJECTIVES[index].label);
        ln_video_text(0, 3, 8 + (int)index * 2, objective, 8);
    }
    if (s_state.mode == LN_MODE_OPERATOR_SCHOOL)
    {
        ln_video_text(0, 2, 16, "VEE: LISTEN, THEN PATCH.", 8);
    }
    else if (s_state.mode == LN_MODE_OPEN_LINES)
    {
        ln_video_text(0, 2, 16, "NO DAWN. PAUSE TO LEAVE.", 8);
    }
    else
    {
        ln_video_text(0, 2, 16, "CROSSINGS CARRY CHOICES.", 8);
    }
    ln_video_text(0, 2, 17, "A BEGIN SHIFT   B MODES", 8);
}

static void render_pause(void)
{
    static const char *const options[3] = {
        "HIGH CONTRAST",
        "REDUCED FLASH",
        "RESUME SHIFT"
    };
    draw_document_base("PAUSED", 0);
    ln_video_text(0, 2, 4, "OBJECTIVES", 8);
    ln_video_text(0, 3, 6, "OWEN: PLATFORM 3", 8);
    ln_video_text(0, 3, 7, "CAL: THREE EAST CABS", 8);
    ln_video_text(0, 3, 8, "PATIENT: KEEP PRIVATE", 8);
    ln_video_text(0, 3, 9, "PLATFORM: TWO SOURCES", 8);
    for (uint8_t index = 0; index < 4; ++index)
    {
        int row = 12 + (int)index;
        ln_video_text(0, 3, row,
                      index == s_state.pause_selection ? ">" : " ", 8);
        const char *label = index < 3u
                                ? options[index]
                                : (s_state.mode == LN_MODE_OPEN_LINES
                                       ? "END SHIFT"
                                       : "SHIFT ENDS AT DAWN");
        ln_video_text(0, 5, row, label, 8);
    }
    ln_video_text(0, 22, 12, s_state.high_contrast ? "ON" : "OFF", 8);
    ln_video_text(0, 22, 13, s_state.reduced_flash ? "ON" : "OFF", 8);
    ln_video_text(0, 2, 18, "START OR B RESUME", 8);
}

static void render_memory(void)
{
    draw_document_base("CITY MEMORY", 0);
    ln_video_text(0, 2, 4, "FACT", 8);
    for (uint8_t index = 0; index < LN_FACT_COUNT; ++index)
    {
        int row = 6 + (int)index * 2;
        ln_video_text(0, 2, row,
                      index == s_state.selected_fact ? ">" : " ", 8);
        ln_video_text(0, 4, row, LN_FACTS[index].display_phrase, 8);
    }

    uint8_t selected = s_state.selected_fact;
    int output_row = 16;
    ln_video_text(0, 2, 15, "KNOWN BY:", 8);
    bool anyone = false;
    for (uint8_t citizen = 0; citizen < LN_CITIZEN_COUNT; ++citizen)
    {
        if ((s_state.knowledge_mask[citizen] & (1u << selected)) != 0u)
        {
            ln_video_text(0, 12, output_row, LN_CITIZENS[citizen].name, 8);
            anyone = true;
            ++output_row;
            if (output_row > 17)
            {
                break;
            }
        }
    }
    if (!anyone)
    {
        ln_video_text(0, 12, output_row, "NO ONE YET", 8);
    }
    ln_video_text(0, 2, 18, "SELECT OR B RETURN", 8);
}

static void render_report(void)
{
    char line[28];
    draw_document_base("SHIFT REPORT", 3);
    ln_video_text(0, 2, 4, "PLATFORM THREE", 8);
    snprintf(line, sizeof(line), "SERVICE       %3u", s_state.score_service);
    ln_video_text(0, 4, 7, line, 8);
    snprintf(line, sizeof(line), "DISCRETION    %3u", s_state.score_discretion);
    ln_video_text(0, 4, 9, line, 8);
    snprintf(line, sizeof(line), "RESONANCE     %3u", s_state.score_resonance);
    ln_video_text(0, 4, 11, line, 8);
    snprintf(line, sizeof(line), "COHERENCE     %3u", s_state.score_coherence);
    ln_video_text(0, 4, 13, line, 8);
    ln_video_text(0, 2, 16,
                  s_state.shift_complete ? "CITY STATE: COHERENT" : "CITY STATE: ALTERED",
                  8);
    ln_video_text(0, 2, 18, "A READ EPILOGUE", 8);
}

static void render_epilogue(void)
{
    draw_document_base("DAWN / PORT MERIDIAN", 3);
    draw_wrapped(3, 7, 24, 5, LN_EPILOGUES[s_state.epilogue_index], 8);
    ln_video_text(0, 2, 17, "A PLAYBACK   B MODES", 8);
}

static void render_playback(void)
{
    char time_text[8];
    draw_document_base("PLAYBACK", 1);
    if (s_state.event_count == 0)
    {
        ln_video_text(0, 3, 8, "NO SHIFT RECORDED", 8);
        ln_video_text(0, 2, 18, "B MODES", 8);
        return;
    }

    uint8_t start = s_state.playback_offset;
    uint8_t shown = 0;
    for (uint8_t index = start;
         index < s_state.event_count && shown < 4;
         ++index, ++shown)
    {
        const LnEvent *event = &s_state.events[index];
        int row = 5 + (int)shown * 3;
        snprintf(time_text, sizeof(time_text), "%02u:%02u",
                 event->second / 60u, event->second % 60u);
        ln_video_text(0, 2, row, time_text, 8);
        ln_video_text(0, 8, row, event->first, 8);
        ln_video_text(0, 8, row + 1, event->second_line, 8);
    }
    ln_video_text(0, 2, 18, "L/R TIMELINE   B MODES", 8);
}

static void append_path_cell(LnCord *cord, int x, int y)
{
    if (cord->path_count >= LN_MAX_PATH_CELLS)
    {
        return;
    }
    if (cord->path_count > 0)
    {
        const LnCell *last = &cord->path[cord->path_count - 1u];
        if (last->x == (uint8_t)x && last->y == (uint8_t)y)
        {
            return;
        }
    }
    cord->path[cord->path_count].x = (uint8_t)x;
    cord->path[cord->path_count].y = (uint8_t)y;
    ++cord->path_count;
}

static int integer_abs(int value)
{
    return value < 0 ? -value : value;
}

static void append_line(LnCord *cord, int x0, int y0, int x1, int y1)
{
    int delta_x = integer_abs(x1 - x0);
    int step_x = x0 < x1 ? 1 : -1;
    int delta_y = -integer_abs(y1 - y0);
    int step_y = y0 < y1 ? 1 : -1;
    int error = delta_x + delta_y;

    while (true)
    {
        append_path_cell(cord, x0, y0);
        if (x0 == x1 && y0 == y1)
        {
            break;
        }
        int doubled_error = error * 2;
        if (doubled_error >= delta_y)
        {
            error += delta_y;
            x0 += step_x;
        }
        if (doubled_error <= delta_x)
        {
            error += delta_x;
            y0 += step_y;
        }
    }
}

static void build_cord_path(uint8_t cord_index)
{
    LnCord *cord = &s_state.cords[cord_index];
    const CallSpec *spec = &LN_CALLS[cord->call_index];
    int source_y = LN_CALL_ROWS[cord->call_index];
    int destination_y = LN_DESTINATION_ROWS[spec->destination_slot];
    uint16_t old_length = cord->path_count > 1u
                              ? (uint16_t)((cord->path_count - 1u) << 8)
                              : 0u;
    uint16_t old_phase = cord->phase;

    cord->path_count = 0;
    if (cord->peg == LN_NO_SELECTION)
    {
        append_line(cord, LN_SOURCE_X, source_y,
                    LN_DESTINATION_X, destination_y);
    }
    else
    {
        append_line(cord, LN_SOURCE_X, source_y,
                    LN_PEG_X, LN_PEG_ROWS[cord->peg]);
        append_line(cord, LN_PEG_X, LN_PEG_ROWS[cord->peg],
                    LN_DESTINATION_X, destination_y);
    }

    uint16_t new_length = cord->path_count > 1u
                              ? (uint16_t)((cord->path_count - 1u) << 8)
                              : 0u;
    if (old_length > 0u && new_length > 0u)
    {
        cord->phase = (uint16_t)(((uint32_t)old_phase * new_length) / old_length);
    }
    else
    {
        cord->phase = 0;
    }
}

static bool crossing_matches(const LnCrossing *crossing,
                             uint8_t cord_a, uint8_t cord_b)
{
    return (crossing->cord_a == cord_a && crossing->cord_b == cord_b) ||
           (crossing->cord_a == cord_b && crossing->cord_b == cord_a);
}

static void recompute_crossings(void)
{
    LnCrossing previous[LN_MAX_CROSSINGS];
    uint8_t previous_count = s_state.crossing_count;
    memcpy(previous, s_state.crossings, sizeof(previous));
    s_state.crossing_count = 0;

    for (uint8_t cord_a = 0; cord_a < LN_CALL_COUNT; ++cord_a)
    {
        if (!s_state.cords[cord_a].live)
        {
            continue;
        }
        for (uint8_t cord_b = (uint8_t)(cord_a + 1u);
             cord_b < LN_CALL_COUNT; ++cord_b)
        {
            if (!s_state.cords[cord_b].live ||
                s_state.crossing_count >= LN_MAX_CROSSINGS)
            {
                continue;
            }

            int best_distance = 1000;
            uint8_t best_a = LN_NO_SELECTION;
            uint8_t best_b = LN_NO_SELECTION;
            for (uint8_t index_a = 1;
                 index_a + 1u < s_state.cords[cord_a].path_count;
                 ++index_a)
            {
                for (uint8_t index_b = 1;
                     index_b + 1u < s_state.cords[cord_b].path_count;
                     ++index_b)
                {
                    const LnCell *cell_a = &s_state.cords[cord_a].path[index_a];
                    const LnCell *cell_b = &s_state.cords[cord_b].path[index_b];
                    if (cell_a->x == cell_b->x && cell_a->y == cell_b->y)
                    {
                        int distance = integer_abs((int)cell_a->x - LN_PEG_X) +
                                       integer_abs((int)cell_a->y - 7);
                        if (distance < best_distance)
                        {
                            best_distance = distance;
                            best_a = index_a;
                            best_b = index_b;
                        }
                    }
                }
            }

            if (best_a != LN_NO_SELECTION)
            {
                LnCrossing *crossing =
                    &s_state.crossings[s_state.crossing_count++];
                memset(crossing, 0, sizeof(*crossing));
                crossing->cord_a = cord_a;
                crossing->cord_b = cord_b;
                crossing->path_index_a = best_a;
                crossing->path_index_b = best_b;
                crossing->x = s_state.cords[cord_a].path[best_a].x;
                crossing->y = s_state.cords[cord_a].path[best_a].y;
                crossing->top_cord = cord_a;
                for (uint8_t old_index = 0;
                     old_index < previous_count; ++old_index)
                {
                    if (crossing_matches(&previous[old_index], cord_a, cord_b))
                    {
                        crossing->top_cord = previous[old_index].top_cord;
                        crossing->grounded = previous[old_index].grounded;
                        break;
                    }
                }
            }
        }
    }

    if (s_state.crossing_count == 0)
    {
        s_state.selected_crossing = 0;
    }
    else if (s_state.selected_crossing >= s_state.crossing_count)
    {
        s_state.selected_crossing = (uint8_t)(s_state.crossing_count - 1u);
    }
}

static uint16_t path_tile(const LnCord *cord, uint8_t path_index)
{
    if (cord->path_count < 2u)
    {
        return 1;
    }
    uint8_t next_index = path_index + 1u < cord->path_count
                             ? (uint8_t)(path_index + 1u)
                             : (uint8_t)(path_index - 1u);
    int delta_x = (int)cord->path[next_index].x - cord->path[path_index].x;
    int delta_y = (int)cord->path[next_index].y - cord->path[path_index].y;
    if (delta_y == 0)
    {
        return 1;
    }
    if (delta_x == 0)
    {
        return 2;
    }
    return delta_x * delta_y > 0 ? 48 : 49;
}

static bool path_is_vertical(const LnCord *cord, uint8_t path_index)
{
    if (cord->path_count < 2u)
    {
        return false;
    }
    uint8_t before = path_index > 0u ? (uint8_t)(path_index - 1u) : path_index;
    uint8_t after = path_index + 1u < cord->path_count
                        ? (uint8_t)(path_index + 1u)
                        : path_index;
    int delta_x = integer_abs((int)cord->path[after].x - cord->path[before].x);
    int delta_y = integer_abs((int)cord->path[after].y - cord->path[before].y);
    return delta_y > delta_x;
}

static void render_cord_maps(void)
{
    ln_video_clear_map(2, 0, 2);
    ln_video_clear_map(3, 0, 2);

    for (uint8_t cord_index = 0; cord_index < LN_CALL_COUNT; ++cord_index)
    {
        const LnCord *cord = &s_state.cords[cord_index];
        if (!cord->live)
        {
            continue;
        }
        for (uint8_t path_index = 0;
             path_index < cord->path_count; ++path_index)
        {
            ln_video_set_tile(2, cord->path[path_index].x,
                              cord->path[path_index].y,
                              path_tile(cord, path_index),
                              cord->palette_bank, false, false);
        }
        if (cord->peg != LN_NO_SELECTION)
        {
            ln_video_set_tile(3, LN_PEG_X, LN_PEG_ROWS[cord->peg],
                              38, cord->palette_bank, false, false);
        }
    }

    for (uint8_t index = 0; index < s_state.crossing_count; ++index)
    {
        const LnCrossing *crossing = &s_state.crossings[index];
        uint8_t lower_cord = crossing->top_cord == crossing->cord_a
                                 ? crossing->cord_b
                                 : crossing->cord_a;
        uint8_t top_cord = crossing->top_cord;
        uint8_t lower_index = lower_cord == crossing->cord_a
                                  ? crossing->path_index_a
                                  : crossing->path_index_b;
        uint8_t top_index = top_cord == crossing->cord_a
                                ? crossing->path_index_a
                                : crossing->path_index_b;
        bool lower_vertical = path_is_vertical(&s_state.cords[lower_cord],
                                               lower_index);
        bool top_vertical = path_is_vertical(&s_state.cords[top_cord], top_index);
        ln_video_set_tile(2, crossing->x, crossing->y,
                          lower_vertical ? 33 : 32,
                          s_state.cords[lower_cord].palette_bank,
                          false, false);
        ln_video_set_tile(3, crossing->x, crossing->y,
                          top_vertical ? 35 : 34,
                          s_state.cords[top_cord].palette_bank,
                          false, false);
    }
}

static void rebuild_routes(void)
{
    for (uint8_t index = 0; index < LN_CALL_COUNT; ++index)
    {
        if (s_state.cords[index].live)
        {
            build_cord_path(index);
        }
    }
    recompute_crossings();
    render_cord_maps();
}

static void reset_shift(void)
{
    bool high_contrast = s_state.high_contrast;
    bool reduced_flash = s_state.reduced_flash;
    LnMode mode = s_state.mode;
    memset(&s_state.calls, 0, sizeof(s_state.calls));
    memset(&s_state.cords, 0, sizeof(s_state.cords));
    memset(&s_state.crossings, 0, sizeof(s_state.crossings));
    memset(&s_state.knowledge_mask, 0, sizeof(s_state.knowledge_mask));
    memset(&s_state.confidence, 0, sizeof(s_state.confidence));
    memset(&s_state.source_mask, 0, sizeof(s_state.source_mask));
    s_state.mode = mode;
    s_state.high_contrast = high_contrast;
    s_state.reduced_flash = reduced_flash;
    s_state.focus = LN_FOCUS_CALL_0;
    s_state.selected_destination = 0;
    s_state.picked_call = LN_NO_SELECTION;
    s_state.selected_crossing = 0;
    s_state.selected_cord = 0;
    s_state.selected_fact = 0;
    s_state.monitored_call = LN_NO_SELECTION;
    s_state.last_monitored_call = LN_NO_SELECTION;
    s_state.b_hold_frames = 0;
    s_state.repeat_frames = 0;
    s_state.elapsed_frames = 0;
    s_state.verify_callback_frames = 0;
    s_state.message_frames = 0;
    s_state.last_header_second = UINT16_MAX;
    s_state.crossing_count = 0;
    s_state.event_count = 0;
    s_state.service_errors = 0;
    s_state.missed_calls = 0;
    s_state.privacy_leaks = 0;
    s_state.useful_leaks = 0;
    s_state.suppressed_leaks = 0;
    s_state.cabs_east = 0;
    s_state.platform_sources = 0;
    s_state.bea_bulletin_prepared = false;
    s_state.warning_delivered = false;
    s_state.owen_verified = false;
    s_state.felix_confirmed = false;
    s_state.shift_complete = false;
    s_state.score_service = 0;
    s_state.score_discretion = 0;
    s_state.score_resonance = 0;
    s_state.score_coherence = 0;
    s_state.epilogue_index = 0;

    for (uint8_t index = 0; index < LN_CALL_COUNT; ++index)
    {
        s_state.calls[index].state = LN_CALL_SCHEDULED;
        uint16_t start_second = LN_CALLS[index].start_second;
        if (mode == LN_MODE_OPERATOR_SCHOOL)
        {
            start_second = (uint16_t)(start_second / 2u);
        }
        else if (mode == LN_MODE_OPEN_LINES)
        {
            start_second = (uint16_t)(start_second / 4u);
        }
        s_state.calls[index].scheduled_second = start_second;
        s_state.cords[index].call_index = index;
        s_state.cords[index].peg = LN_DEFAULT_PEG[index];
        s_state.cords[index].palette_bank =
            (uint8_t)(2u + LN_CITIZENS[LN_CALLS[index].caller_id].voice_color);
        s_state.cords[index].speed = LN_CORD_SPEEDS[index];
    }

    for (uint8_t fact_index = 0; fact_index < LN_FACT_COUNT; ++fact_index)
    {
        uint8_t source = LN_FACTS[fact_index].source_citizen_id;
        if (source < LN_CITIZEN_COUNT)
        {
            s_state.knowledge_mask[source] |=
                (uint16_t)(1u << fact_index);
            s_state.confidence[source][fact_index] =
                LN_FACTS[fact_index].confidence;
            s_state.source_mask[source][fact_index] = (uint32_t)(1u << source);
        }
    }

    if (mode == LN_MODE_OPERATOR_SCHOOL)
    {
        s_state.shift_duration_frames =
            (uint32_t)LN_SCHOOL_SHIFT_SECONDS * LN_FRAMES_PER_SECOND;
    }
    else if (mode == LN_MODE_OPEN_LINES)
    {
        s_state.shift_duration_frames = UINT32_MAX;
    }
    else
    {
        s_state.shift_duration_frames =
            (uint32_t)LN_FINAL_SHIFT_SECONDS * LN_FRAMES_PER_SECOND;
    }
    add_event("SHIFT OPEN", "PLATFORM THREE");
}

static void start_shift(void)
{
    reset_shift();
    enter_screen(LN_SCREEN_GAMEPLAY);
}

static void calculate_report(void)
{
    uint8_t serviced_calls = 0;
    for (uint8_t index = 0; index < LN_CALL_COUNT; ++index)
    {
        if (s_state.calls[index].serviced)
        {
            ++serviced_calls;
        }
    }

    s_state.score_service = clamped_score((int)serviced_calls * 25 -
                                          (int)s_state.service_errors * 10 -
                                          (int)s_state.missed_calls * 8);
    s_state.score_discretion =
        clamped_score(100 - (int)s_state.privacy_leaks * 45);
    s_state.score_resonance =
        clamped_score((int)s_state.useful_leaks * 100);

    uint8_t pass_mask = objective_pass_mask();
    s_state.score_coherence = objective_passes(3) ? 100u : 0u;
    s_state.shift_complete = pass_mask == LN_OBJECTIVE_ALL_MASK;
    if ((pass_mask & (1u << 0)) == 0u)
    {
        s_state.epilogue_index = 3;
    }
    else if ((pass_mask & (1u << 2)) == 0u)
    {
        s_state.epilogue_index = 2;
    }
    else if ((pass_mask & (1u << 1)) == 0u)
    {
        s_state.epilogue_index = 1;
    }
    else if ((pass_mask & (1u << 3)) == 0u)
    {
        s_state.epilogue_index = 3;
    }
    else
    {
        s_state.epilogue_index = 0;
    }
}

static void finish_shift(void)
{
    calculate_report();
    ln_audio_play(LN_SOUND_DAWN_CHIME);
    enter_screen(LN_SCREEN_REPORT);
}

static void connect_call(uint8_t call_index)
{
    LnCallRuntime *call = &s_state.calls[call_index];
    const CallSpec *spec = &LN_CALLS[call_index];
    if (s_state.selected_destination != spec->destination_slot)
    {
        ++s_state.service_errors;
        set_message("WRONG PARTY", "CALLER STILL WAITING");
        ln_audio_play(LN_SOUND_CROSSTALK);
        s_state.picked_call = LN_NO_SELECTION;
        return;
    }

    call->state = LN_CALL_LIVE;
    call->ring_frames = 0;
    LnCord *cord = &s_state.cords[call_index];
    cord->live = true;
    cord->phase = 0;
    cord->payload_cursor = 0;
    cord->packet_flash_frames = 0;
    s_state.selected_cord = call_index;
    s_state.picked_call = LN_NO_SELECTION;
    rebuild_routes();

    char first[24];
    snprintf(first, sizeof(first), "%s CONNECTED", spec->caller_label);
    add_event(first, spec->destination_label);
    ln_audio_play(LN_SOUND_PLUG_CLICK);
}

static void unplug_call(uint8_t call_index)
{
    LnCallRuntime *call = &s_state.calls[call_index];
    call->state = call->serviced ? LN_CALL_COMPLETED : LN_CALL_RINGING;
    call->ring_frames = 0;
    s_state.cords[call_index].live = false;
    rebuild_routes();
    add_event(call->serviced ? "CALL CLEARED" : "CORD RELEASED",
              LN_CALLS[call_index].caller_label);
    ln_audio_play(LN_SOUND_CORD_MOVE);
}

static void cycle_peg(uint8_t cord_index)
{
    LnCord *cord = &s_state.cords[cord_index];
    if (!cord->live)
    {
        set_message("NO LIVE CORD", "PATCH A CALL FIRST");
        return;
    }
    if (cord->peg == LN_NO_SELECTION)
    {
        cord->peg = 0;
    }
    else if (cord->peg >= 2u)
    {
        cord->peg = LN_NO_SELECTION;
    }
    else
    {
        ++cord->peg;
    }
    rebuild_routes();
    add_event("CORD REHUNG", cord->peg == LN_NO_SELECTION ? "NO BRIDGE" : "BRIDGE PEG MOVED");
    ln_audio_play(LN_SOUND_CORD_MOVE);
}

static void flip_crossing(void)
{
    if (s_state.crossing_count == 0)
    {
        set_message("NO CROSSING", "REHANG A LIVE CORD");
        return;
    }
    LnCrossing *crossing = &s_state.crossings[s_state.selected_crossing];
    crossing->top_cord = crossing->top_cord == crossing->cord_a
                             ? crossing->cord_b
                             : crossing->cord_a;
    render_cord_maps();
    add_event("CROSSING FLIPPED", LN_CALLS[crossing->top_cord].caller_label);
    ln_audio_play(LN_SOUND_CORD_MOVE);
}

static void toggle_ground(void)
{
    if (s_state.crossing_count == 0)
    {
        set_message("NO CROSSING", "NOTHING TO GROUND");
        return;
    }
    LnCrossing *crossing = &s_state.crossings[s_state.selected_crossing];
    if (!crossing->grounded)
    {
        for (uint8_t index = 0; index < s_state.crossing_count; ++index)
        {
            if (s_state.crossings[index].grounded)
            {
                set_message("ONE CLAMP AVAILABLE", "REMOVE IT BEFORE MOVING");
                return;
            }
        }
    }
    crossing->grounded = !crossing->grounded;
    set_message(crossing->grounded ? "GROUND CLAMP SET" : "GROUND CLAMP REMOVED",
                "NEXT LEAK SUPPRESSED");
    ln_audio_play(LN_SOUND_GROUND_CLAMP);
}

static void mark_call_serviced(uint8_t call_index)
{
    if (!s_state.calls[call_index].serviced)
    {
        s_state.calls[call_index].serviced = true;
        char first[24];
        snprintf(first, sizeof(first), "%s COMPLETED",
                 LN_CALLS[call_index].caller_label);
        add_event(first, LN_CALLS[call_index].destination_label);
    }
}

static void evaluate_fact_hooks(uint8_t citizen_id, uint8_t fact_id,
                                bool via_leak, bool new_knowledge)
{
    if (citizen_id == 1u && fact_id == 1u)
    {
        refresh_platform_sources();
        if (s_state.platform_sources < 2u && !s_state.owen_verified &&
            s_state.verify_callback_frames == 0u)
        {
            s_state.verify_callback_frames =
                LN_VERIFY_CALLBACK_SECONDS * LN_FRAMES_PER_SECOND;
        }
    }

    if (citizen_id == 5u && (fact_id == 1u || fact_id == 3u) &&
        citizen_knows_fact(5, 1, CONFIDENCE_LIKELY) &&
        citizen_knows_fact(5, 3, CONFIDENCE_LIKELY) &&
        s_state.cabs_east < 3u)
    {
        s_state.cabs_east = 3;
        if (via_leak)
        {
            ++s_state.useful_leaks;
        }
        add_event("CAL CALLS: EAST GARAGE", "THREE CABS MOVE");
    }

    if (citizen_id == 6u && (fact_id == 1u || fact_id == 4u) &&
        citizen_knows_fact(6, 1, CONFIDENCE_LIKELY) &&
        !citizen_knows_fact(6, 4, CONFIDENCE_DISTORTED) &&
        !s_state.bea_bulletin_prepared)
    {
        s_state.bea_bulletin_prepared = true;
        add_event("BEA FILES BULLETIN", "NO PATIENT NAME");
    }

    if (citizen_id == 9u && fact_id == 1u &&
        citizen_knows_fact(9, 1, CONFIDENCE_LIKELY) &&
        !s_state.felix_confirmed)
    {
        uint8_t fact_index = fact_index_from_id(1);
        if (s_state.confidence[9][fact_index] < CONFIDENCE_CERTAIN)
        {
            ++s_state.confidence[9][fact_index];
        }
        s_state.felix_confirmed = true;
        add_event("FELIX CONFIRMS", "PLATFORM THREE");
    }

    if (new_knowledge && fact_id == 4u &&
        (LN_OBJECTIVES[2].forbidden_receiver_mask & (1u << citizen_id)) != 0u)
    {
        ++s_state.privacy_leaks;
        add_event("PRIVACY BREACH", LN_CITIZENS[citizen_id].name);
    }
    if (new_knowledge && citizen_id == 5u && fact_id == 5u)
    {
        s_state.warning_delivered = true;
    }
}

static bool learn_fact(uint8_t citizen_id, uint8_t fact_id,
                       uint8_t confidence, bool via_leak,
                       uint8_t source_id)
{
    uint8_t fact_index = fact_index_from_id(fact_id);
    if (citizen_id >= LN_CITIZEN_COUNT || fact_index == LN_NO_SELECTION)
    {
        return false;
    }
    uint16_t bit = (uint16_t)(1u << fact_index);
    bool new_knowledge = (s_state.knowledge_mask[citizen_id] & bit) == 0u;
    s_state.knowledge_mask[citizen_id] |= bit;
    bool confidence_changed = false;
    if (confidence > s_state.confidence[citizen_id][fact_index])
    {
        s_state.confidence[citizen_id][fact_index] = confidence;
        confidence_changed = true;
    }

    bool source_changed = false;
    if (source_id != LN_NO_ID && source_id <= LN_SOURCE_RAIL)
    {
        uint32_t source_bit = (uint32_t)(1u << source_id);
        uint32_t *sources = &s_state.source_mask[citizen_id][fact_index];
        source_changed = (*sources & source_bit) == 0u;
        *sources |= source_bit;
    }
    if (!new_knowledge && !confidence_changed && !source_changed)
    {
        return false;
    }

    if (new_knowledge)
    {
        char first[28];
        snprintf(first, sizeof(first), "%s HEARD:",
                 LN_CITIZENS[citizen_id].name);
        add_event(first, LN_FACTS[fact_index].display_phrase);
    }
    evaluate_fact_hooks(citizen_id, fact_id, via_leak, new_knowledge);
    return true;
}

static uint8_t current_packet_fact(const LnCord *cord)
{
    const CallSpec *spec = &LN_CALLS[cord->call_index];
    if (spec->payload_count == 0u)
    {
        return LN_NO_ID;
    }
    uint8_t fact_id =
        spec->payload_fact_ids[cord->payload_cursor % spec->payload_count];
    return fact_is_active(fact_id) ? fact_id : LN_NO_ID;
}

static void deliver_packet(uint8_t cord_index)
{
    LnCord *cord = &s_state.cords[cord_index];
    const CallSpec *spec = &LN_CALLS[cord->call_index];
    LnCallRuntime *call = &s_state.calls[cord->call_index];

    if (spec->payload_count == 0u)
    {
        mark_call_serviced(cord->call_index);
        return;
    }

    uint8_t payload_index = cord->payload_cursor % spec->payload_count;
    uint8_t fact_id = spec->payload_fact_ids[payload_index];
    uint8_t payload_bit = (uint8_t)(1u << payload_index);
    if ((call->delivered_mask & payload_bit) == 0u)
    {
        call->delivered_mask |= payload_bit;
        uint8_t receiver = destination_citizen(spec->destination_slot);
        uint8_t fact_index = fact_index_from_id(fact_id);
        if (receiver != LN_NO_SELECTION && fact_index != LN_NO_SELECTION &&
            fact_is_active(fact_id))
        {
            learn_fact(receiver, fact_id, LN_FACTS[fact_index].confidence,
                       false, LN_FACTS[fact_index].source_citizen_id);
        }
        ln_audio_play(LN_SOUND_PACKET_TRANSFER);
    }
    uint8_t complete_mask = (uint8_t)((1u << spec->payload_count) - 1u);
    if ((call->delivered_mask & complete_mask) == complete_mask)
    {
        mark_call_serviced(cord->call_index);
    }
    cord->payload_cursor = (uint8_t)((payload_index + 1u) % spec->payload_count);
}

static uint16_t phase_distance(uint16_t phase, uint16_t target,
                               uint16_t length)
{
    uint16_t distance = phase > target ? (uint16_t)(phase - target)
                                       : (uint16_t)(target - phase);
    if (length == 0u || distance > length)
    {
        return distance;
    }
    uint16_t wrapped = (uint16_t)(length - distance);
    return distance < wrapped ? distance : wrapped;
}

static void perform_leak(LnCrossing *crossing)
{
    uint8_t source_cord = crossing->top_cord;
    uint8_t target_cord = source_cord == crossing->cord_a
                              ? crossing->cord_b
                              : crossing->cord_a;
    uint8_t fact_id = current_packet_fact(&s_state.cords[source_cord]);
    uint8_t fact_index = fact_index_from_id(fact_id);
    uint8_t receiver = LN_CALLS[target_cord].caller_id;
    if (fact_index == LN_NO_SELECTION)
    {
        return;
    }

    if (crossing->grounded)
    {
        crossing->grounded = false;
        crossing->flash_frames = 18;
        ++s_state.suppressed_leaks;
        add_event("CLAMP CAUGHT LEAK", LN_FACTS[fact_index].display_phrase);
        ln_audio_play(LN_SOUND_GROUND_CLAMP);
        return;
    }

    bool changed = learn_fact(receiver, fact_id, CONFIDENCE_LIKELY, true,
                              LN_FACTS[fact_index].source_citizen_id);
    if (!changed)
    {
        return;
    }
    crossing->flash_frames = 24;
    s_state.cords[source_cord].packet_flash_frames = 20;
    ln_audio_play(LN_SOUND_CROSSTALK);
}

static void update_cords(void)
{
    for (uint8_t index = 0; index < LN_CALL_COUNT; ++index)
    {
        LnCord *cord = &s_state.cords[index];
        if (!cord->live || cord->path_count < 2u)
        {
            continue;
        }
        uint16_t length = (uint16_t)((cord->path_count - 1u) << 8);
        uint32_t next_phase = (uint32_t)cord->phase + cord->speed;
        if (next_phase >= length)
        {
            next_phase -= length;
            deliver_packet(index);
        }
        cord->phase = (uint16_t)next_phase;
        if (cord->packet_flash_frames > 0u)
        {
            --cord->packet_flash_frames;
        }
    }

    for (uint8_t index = 0; index < s_state.crossing_count; ++index)
    {
        LnCrossing *crossing = &s_state.crossings[index];
        uint16_t target_a = (uint16_t)(crossing->path_index_a << 8);
        uint16_t target_b = (uint16_t)(crossing->path_index_b << 8);
        uint16_t length_a = (uint16_t)(
            (s_state.cords[crossing->cord_a].path_count - 1u) << 8);
        uint16_t length_b = (uint16_t)(
            (s_state.cords[crossing->cord_b].path_count - 1u) << 8);
        bool near = phase_distance(s_state.cords[crossing->cord_a].phase,
                                   target_a, length_a) <= LN_CROSSING_WINDOW &&
                    phase_distance(s_state.cords[crossing->cord_b].phase,
                                   target_b, length_b) <= LN_CROSSING_WINDOW;
        if (near && !crossing->resonance_latched)
        {
            crossing->resonance_latched = true;
            perform_leak(crossing);
        }
        else if (!near)
        {
            crossing->resonance_latched = false;
        }
        if (crossing->flash_frames > 0u)
        {
            --crossing->flash_frames;
        }
    }
}

static void update_calls(void)
{
    uint16_t current_second =
        (uint16_t)(s_state.elapsed_frames / LN_FRAMES_PER_SECOND);
    for (uint8_t index = 0; index < LN_CALL_COUNT; ++index)
    {
        LnCallRuntime *call = &s_state.calls[index];
        if (call->state == LN_CALL_SCHEDULED &&
            current_second >= call->scheduled_second)
        {
            call->state = LN_CALL_RINGING;
            call->ring_frames = 0;
            char first[24];
            snprintf(first, sizeof(first), "%s RINGING",
                     LN_CALLS[index].caller_label);
            add_event(first, "B HOLD TO LISTEN");
            ln_audio_play(LN_SOUND_RING_SUBSCRIBER);
        }
        else if (call->state == LN_CALL_RINGING)
        {
            ++call->ring_frames;
            uint32_t patience_frames =
                (uint32_t)LN_CALLS[index].patience_seconds * LN_FRAMES_PER_SECOND;
            if (call->ring_frames >= patience_frames)
            {
                ++s_state.missed_calls;
                if (s_state.picked_call == index)
                {
                    s_state.picked_call = LN_NO_SELECTION;
                }
                if (call->retry_count == 0u || s_state.mode == LN_MODE_OPEN_LINES)
                {
                    ++call->retry_count;
                    call->state = LN_CALL_SCHEDULED;
                    call->scheduled_second = (uint16_t)(current_second + 15u);
                    add_event("CALLER TRIES AGAIN", LN_CALLS[index].caller_label);
                }
                else
                {
                    call->state = LN_CALL_HUNG_UP;
                    add_event("CALL LOST", LN_CALLS[index].caller_label);
                }
            }
        }
    }
}

static void update_callbacks(void)
{
    if (s_state.verify_callback_frames > 0u)
    {
        --s_state.verify_callback_frames;
        if (s_state.verify_callback_frames == 0u && !s_state.owen_verified)
        {
            s_state.owen_verified = true;
            learn_fact(1, 1, CONFIDENCE_CERTAIN, false, LN_SOURCE_RAIL);
            refresh_platform_sources();
            add_event("OWEN CALLS: RAIL DESK", "PLATFORM 3 VERIFIED");
            ln_audio_play(LN_SOUND_VERIFY);
        }
    }
}

static void update_monitor(u16 held, u16 released)
{
    if ((held & KEY_B) != 0u && s_state.focus < LN_CALL_COUNT)
    {
        if (s_state.b_hold_frames < UINT16_MAX)
        {
            ++s_state.b_hold_frames;
        }
        s_state.monitored_call = s_state.focus;
        s_state.last_monitored_call = s_state.focus;
    }
    else
    {
        s_state.monitored_call = LN_NO_SELECTION;
    }

    if ((released & KEY_B) != 0u)
    {
        if (s_state.b_hold_frames > 0u &&
            s_state.b_hold_frames < LN_MONITOR_HOLD_FRAMES &&
            s_state.last_monitored_call != LN_NO_SELECTION)
        {
            s_state.repeat_frames = LN_REPEAT_FRAMES;
        }
        s_state.b_hold_frames = 0;
    }
    if (s_state.repeat_frames > 0u)
    {
        --s_state.repeat_frames;
    }
}

static void update_gameplay_input(u16 pressed, u16 held, u16 released)
{
    if ((pressed & KEY_START) != 0u)
    {
        s_state.pause_selection = 0;
        enter_screen(LN_SCREEN_PAUSE);
        return;
    }

    if (s_state.picked_call != LN_NO_SELECTION)
    {
        uint8_t picked_call = s_state.picked_call;
        if ((pressed & KEY_B) != 0u)
        {
            s_state.picked_call = LN_NO_SELECTION;
            s_state.b_hold_frames = 0;
            s_state.monitored_call = LN_NO_SELECTION;
            s_state.repeat_frames = 0;
            set_message("CORD SET DOWN", "CALLER STILL WAITING");
            return;
        }
        if (s_state.calls[picked_call].state != LN_CALL_RINGING)
        {
            s_state.picked_call = LN_NO_SELECTION;
            set_message("CALL NO LONGER RINGING", "CORD RETURNED");
            return;
        }
        if ((pressed & KEY_LEFT) != 0u)
        {
            s_state.selected_destination = s_state.selected_destination == 0u
                                               ? DESTINATION_SLOT_COUNT - 1u
                                               : (uint8_t)(s_state.selected_destination - 1u);
        }
        if ((pressed & KEY_RIGHT) != 0u)
        {
            s_state.selected_destination =
                (uint8_t)((s_state.selected_destination + 1u) % DESTINATION_SLOT_COUNT);
        }
        if ((pressed & KEY_A) != 0u)
        {
            connect_call(picked_call);
        }
        return;
    }

    update_monitor(held, released);
    if ((pressed & KEY_SELECT) != 0u)
    {
        enter_screen(LN_SCREEN_MEMORY);
        return;
    }

    if ((pressed & KEY_UP) != 0u)
    {
        s_state.focus = s_state.focus == 0u
                            ? (uint8_t)(LN_FOCUS_COUNT - 1u)
                            : (uint8_t)(s_state.focus - 1u);
    }
    if ((pressed & KEY_DOWN) != 0u)
    {
        s_state.focus = (uint8_t)((s_state.focus + 1u) % LN_FOCUS_COUNT);
    }
    if ((pressed & KEY_LEFT) != 0u)
    {
        if (s_state.focus == LN_FOCUS_CROSSING &&
            s_state.crossing_count > 0u)
        {
            s_state.selected_crossing = s_state.selected_crossing == 0u
                                            ? (uint8_t)(s_state.crossing_count - 1u)
                                            : (uint8_t)(s_state.selected_crossing - 1u);
        }
        else if (s_state.focus == LN_FOCUS_PEG)
        {
            s_state.selected_cord = s_state.selected_cord == 0u
                                        ? LN_CALL_COUNT - 1u
                                        : (uint8_t)(s_state.selected_cord - 1u);
        }
    }
    if ((pressed & KEY_RIGHT) != 0u)
    {
        if (s_state.focus == LN_FOCUS_CROSSING &&
            s_state.crossing_count > 0u)
        {
            s_state.selected_crossing =
                (uint8_t)((s_state.selected_crossing + 1u) % s_state.crossing_count);
        }
        else if (s_state.focus == LN_FOCUS_PEG)
        {
            s_state.selected_cord =
                (uint8_t)((s_state.selected_cord + 1u) % LN_CALL_COUNT);
        }
    }

    if ((pressed & KEY_L) != 0u)
    {
        uint8_t cord = s_state.focus < LN_CALL_COUNT
                           ? s_state.focus
                           : s_state.selected_cord;
        cycle_peg(cord);
    }
    if ((pressed & KEY_R) != 0u)
    {
        toggle_ground();
    }
    if ((pressed & KEY_A) != 0u)
    {
        if (s_state.focus < LN_CALL_COUNT)
        {
            uint8_t call_index = s_state.focus;
            if (s_state.calls[call_index].state == LN_CALL_RINGING)
            {
                if (s_state.picked_call == call_index)
                {
                    connect_call(call_index);
                }
                else
                {
                    s_state.picked_call = call_index;
                    s_state.selected_destination = 0;
                    set_message("CORD PICKED UP", "LEFT/RIGHT THEN A");
                }
            }
            else if (s_state.calls[call_index].state == LN_CALL_LIVE)
            {
                unplug_call(call_index);
            }
            else if (s_state.calls[call_index].state == LN_CALL_COMPLETED)
            {
                set_message("CALL COMPLETE", "LINE HAS CLEARED");
            }
            else
            {
                set_message("LINE IS QUIET", "WAIT FOR THE LAMP");
            }
        }
        else if (s_state.focus == LN_FOCUS_CROSSING)
        {
            flip_crossing();
        }
        else if (s_state.focus == LN_FOCUS_PEG)
        {
            cycle_peg(s_state.selected_cord);
        }
        else if (s_state.focus == LN_FOCUS_MEMORY)
        {
            enter_screen(LN_SCREEN_MEMORY);
        }
    }
}

static void update_gameplay(u16 pressed, u16 held, u16 released)
{
    update_gameplay_input(pressed, held, released);
    if (s_state.screen != LN_SCREEN_GAMEPLAY)
    {
        return;
    }
    ++s_state.elapsed_frames;
    update_calls();
    update_cords();
    update_callbacks();
    if (s_state.message_frames > 0u)
    {
        --s_state.message_frames;
    }
    if (s_state.shift_duration_frames != UINT32_MAX &&
        s_state.elapsed_frames >= s_state.shift_duration_frames)
    {
        finish_shift();
    }
}

static void build_gameplay_board(void)
{
    uint8_t scene = s_state.mode == LN_MODE_OPERATOR_SCHOOL ? 4u : 0u;
    ln_video_load_interface(scene, s_state.high_contrast);
    ln_video_fill(1, 0, 0, 30, 20, 0, 0);
    ln_video_fill(1, 0, 0, 30, 2, 31, 0);
    draw_box(0, 2, 21, 13, 1, 16, 0);
    draw_box(21, 2, 9, 18, 136, 16, 0);
    draw_box(0, 15, 21, 5, 31, 16, 0);
    ln_video_fill(1, 22, 3, 7, 16, LN_CITY_TILE_BASE, 1);

    ln_video_text(0, 1, 3, "PATCH FIELD", 8);
    ln_video_text(0, 22, 3, "CITY", 8);
    ln_video_text(0, 22, 4, "MEMORY", 8);

    for (uint8_t index = 0; index < LN_CALL_COUNT; ++index)
    {
        uint8_t row = LN_CALL_ROWS[index];
        ln_video_fill(1, 1, row, 5, 1, 112, 0);
        ln_video_fill(1, 15, row, 5, 1, 112, 0);
        ln_video_text(0, 1, row, LN_CALLS[index].caller_label, 0);
        ln_video_text(0, 15, row,
                      LN_DESTINATION_LABELS[index], 0);
        ln_video_set_tile(1, LN_SOURCE_X, row, 32, 0, false, false);
        ln_video_set_tile(1, LN_DESTINATION_X, row, 32, 0, false, false);
    }

    for (int row = 5; row < 12; ++row)
    {
        for (int column = 22; column < 29; ++column)
        {
            uint16_t city_tile = (uint16_t)(LN_CITY_TILE_BASE +
                LN_CITY_MAP[(row - 5) * LN_CITY_MAP_COLUMNS +
                            (column - 22)]);
            ln_video_set_tile(1, column, row, city_tile, 1, false, false);
        }
    }
    ln_video_text(0, 22, 13, "KNOWN", 8);
    ln_video_text(0, 22, 16, "LATEST", 8);
    render_cord_maps();
    s_state.board_built = true;
    s_state.last_header_second = UINT16_MAX;
}

static void render_jacks(void)
{
    for (uint8_t index = 0; index < LN_CALL_COUNT; ++index)
    {
        uint16_t source_tile = 32;
        if (s_state.calls[index].state == LN_CALL_RINGING)
        {
            source_tile = 34u + (uint16_t)((s_state.elapsed_frames / 15u) & 1u);
        }
        else if (s_state.calls[index].state == LN_CALL_LIVE)
        {
            source_tile = 39;
        }
        else if (s_state.calls[index].state == LN_CALL_HUNG_UP)
        {
            source_tile = 41;
        }
        else if (s_state.calls[index].state == LN_CALL_COMPLETED)
        {
            source_tile = 40;
        }
        ln_video_set_tile(1, LN_SOURCE_X, LN_CALL_ROWS[index],
                          source_tile, 0, false, false);
        uint16_t destination_tile = 32;
        for (uint8_t live = 0; live < LN_CALL_COUNT; ++live)
        {
            if (s_state.cords[live].live &&
                LN_CALLS[live].destination_slot == index)
            {
                destination_tile = 39;
                break;
            }
        }
        ln_video_set_tile(1, LN_DESTINATION_X,
                          LN_DESTINATION_ROWS[index],
                          destination_tile, 0, false, false);
    }
}

static void render_header(void)
{
    uint16_t second = (uint16_t)(s_state.elapsed_frames / LN_FRAMES_PER_SECOND);
    if (second == s_state.last_header_second)
    {
        return;
    }
    s_state.last_header_second = second;
    char first_line[20];
    char second_line[24];
    uint16_t clock_minutes = (uint16_t)(47u + second / 60u);
    uint16_t clock_hour = (uint16_t)((23u + clock_minutes / 60u) % 24u);
    clock_minutes %= 60u;
    uint8_t live_count = 0;
    for (uint8_t index = 0; index < LN_CALL_COUNT; ++index)
    {
        if (s_state.calls[index].state == LN_CALL_LIVE)
        {
            ++live_count;
        }
    }
    int trust = 100 - (int)s_state.service_errors * 8 -
                (int)s_state.missed_calls * 5;
    if (trust < 0)
    {
        trust = 0;
    }
    snprintf(first_line, sizeof(first_line), "%02u:%02u  LIVE %u",
             clock_hour, clock_minutes, live_count);
    snprintf(second_line, sizeof(second_line), "TRUST %u  BLEED %u",
             (unsigned)trust, s_state.privacy_leaks);
    ln_video_clear_text_rect(0, 0, 30, 2);
    ln_video_text(0, 0, 0, first_line, 8);
    ln_video_text(0, 0, 1, second_line, 8);
}

static uint8_t monitored_or_focused_call(void)
{
    if (s_state.monitored_call != LN_NO_SELECTION)
    {
        return s_state.monitored_call;
    }
    if (s_state.repeat_frames > 0u &&
        s_state.last_monitored_call < LN_CALL_COUNT)
    {
        return s_state.last_monitored_call;
    }
    if (s_state.focus < LN_CALL_COUNT)
    {
        return s_state.focus;
    }
    return LN_NO_SELECTION;
}

static void render_call_card(void)
{
    ln_video_clear_text_rect(5, 16, 15, 3);
    uint8_t call_index = monitored_or_focused_call();
    if (call_index == LN_NO_SELECTION)
    {
        if (s_state.message_frames > 0u)
        {
            draw_wrapped(5, 16, 15, 1, s_state.message_first, 8);
            draw_wrapped(5, 17, 15, 2, s_state.message_second, 8);
        }
        else
        {
            ln_video_text(0, 5, 16, "B HOLD: MONITOR", 8);
            ln_video_text(0, 5, 18, "SELECT: MEMORY", 8);
        }
        return;
    }

    const CallSpec *spec = &LN_CALLS[call_index];
    ln_video_text(0, 5, 16, spec->caller_label, 8);
    bool listening = s_state.monitored_call == call_index ||
                     s_state.repeat_frames > 0u;
    if (listening)
    {
        ln_video_text(0, 12, 16, "B LISTEN", 8);
        draw_wrapped(5, 17, 15, 2,
                     s_state.calls[call_index].state == LN_CALL_RINGING
                         ? spec->request_phrase
                         : spec->monitor_phrase,
                     8);
    }
    else if (s_state.picked_call == call_index)
    {
        char destination[20];
        snprintf(destination, sizeof(destination), "DEST: <%s>",
                 LN_DESTINATION_LABELS[s_state.selected_destination]);
        ln_video_text(0, 5, 17, destination, 8);
        ln_video_text(0, 5, 18, "L/R DST A:PATCH", 8);
    }
    else
    {
        const char *status = "SCHEDULED";
        if (s_state.calls[call_index].state == LN_CALL_RINGING)
        {
            status = "RING: A PICK UP";
        }
        else if (s_state.calls[call_index].state == LN_CALL_LIVE)
        {
            status = "LIVE: A UNPLUG";
        }
        else if (s_state.calls[call_index].state == LN_CALL_HUNG_UP)
        {
            status = "CALL LOST";
        }
        else if (s_state.calls[call_index].state == LN_CALL_COMPLETED)
        {
            status = "CALL COMPLETE";
        }
        ln_video_text(0, 5, 17, status, 8);
        ln_video_text(0, 5, 18, "HOLD B: REQUEST", 8);
    }
}

static void render_city_memory_panel(void)
{
    uint8_t holder = LN_NO_SELECTION;
    uint8_t held_fact = LN_NO_SELECTION;
    for (uint8_t citizen = 0; citizen < LN_CITIZEN_COUNT; ++citizen)
    {
        for (uint8_t fact = 0; fact < LN_FACT_COUNT; ++fact)
        {
            if ((s_state.knowledge_mask[citizen] & (1u << fact)) != 0u)
            {
                holder = citizen;
                held_fact = fact;
                break;
            }
        }
        if (holder != LN_NO_SELECTION)
        {
            break;
        }
    }

    ln_video_clear_text_rect(22, 13, 7, 6);
    ln_video_text(0, 22, 13, "KNOWN", 8);
    if (holder == LN_NO_SELECTION)
    {
        ln_video_text(0, 22, 14, "NONE", 8);
        ln_video_text(0, 22, 15, "--", 8);
    }
    else
    {
        draw_wrapped(22, 14, 7, 1, LN_CITIZENS[holder].name, 8);
        static const char *const confidence_labels[4] = {
            "DISTORT", "DOUBT", "LIKELY", "CERTAIN"
        };
        uint8_t confidence = s_state.confidence[holder][held_fact];
        ln_video_text(0, 22, 15, confidence_labels[confidence], 8);
    }

    ln_video_text(0, 22, 16, "NEXT", 8);
    if (s_state.verify_callback_frames > 0u && !s_state.owen_verified)
    {
        ln_video_text(0, 22, 17, "OWEN->", 8);
        ln_video_text(0, 22, 18, "RAIL", 8);
        return;
    }
    for (uint8_t call = 0; call < LN_CALL_COUNT; ++call)
    {
        if (s_state.calls[call].state == LN_CALL_RINGING)
        {
            ln_video_text(0, 22, 17, LN_CALLS[call].caller_label, 8);
            ln_video_text(0, 22, 18, "RINGING", 8);
            return;
        }
    }
    ln_video_text(0, 22, 17, "SIGNALS", 8);
    ln_video_text(0, 22, 18, "MOVING", 8);
}

static void packet_position(const LnCord *cord, int *x, int *y)
{
    uint8_t cell_index = (uint8_t)(cord->phase >> 8);
    uint8_t fraction = (uint8_t)(cord->phase & 0xffu);
    if (cell_index + 1u >= cord->path_count)
    {
        cell_index = (uint8_t)(cord->path_count - 1u);
        *x = cord->path[cell_index].x * 8;
        *y = cord->path[cell_index].y * 8;
        return;
    }
    const LnCell *start = &cord->path[cell_index];
    const LnCell *end = &cord->path[cell_index + 1u];
    *x = start->x * 8 + (((int)end->x - start->x) * fraction) / 32;
    *y = start->y * 8 + (((int)end->y - start->y) * fraction) / 32;
}

static void render_gameplay_sprites(void)
{
    ln_video_hide_sprites();
    uint8_t call_card = monitored_or_focused_call();
    if (call_card != LN_NO_SELECTION)
    {
        uint8_t portrait = LN_CALLS[call_card].portrait_index;
        ln_video_set_sprite(0, 8, 124,
                            (uint16_t)(LN_OBJ_PORTRAITS + portrait * 16u),
                            0, LN_SPRITE_32, 0, false, false);
    }

    for (uint8_t index = 0; index < LN_CALL_COUNT; ++index)
    {
        const LnCord *cord = &s_state.cords[index];
        if (cord->live && cord->path_count > 1u)
        {
            int packet_x;
            int packet_y;
            packet_position(cord, &packet_x, &packet_y);
            uint8_t fact_index = fact_index_from_id(current_packet_fact(cord));
            uint8_t icon = fact_index == LN_NO_SELECTION
                               ? FACT_ICON_WARNING
                               : LN_FACTS[fact_index].icon;
            uint16_t packet_tile = (uint16_t)(LN_OBJ_PACKETS + icon +
                (((s_state.elapsed_frames / 8u) & 1u) != 0u ? 16u : 0u));
            uint8_t packet_palette = cord->packet_flash_frames > 0u
                                         ? cord->palette_bank
                                         : 1u;
            ln_video_set_sprite((uint8_t)(1u + index), packet_x, packet_y,
                                packet_tile, packet_palette,
                                LN_SPRITE_8, 0, false, false);

            uint8_t plug_frame = (uint8_t)(8u + index);
            uint16_t plug_tile = (uint16_t)(LN_OBJ_PLUGS + plug_frame * 4u);
            ln_video_set_sprite((uint8_t)(8u + index * 2u),
                                LN_SOURCE_X * 8 - 4,
                                LN_CALL_ROWS[index] * 8 - 4,
                                plug_tile, cord->palette_bank,
                                LN_SPRITE_16, 0, false, false);
            ln_video_set_sprite((uint8_t)(9u + index * 2u),
                                LN_DESTINATION_X * 8 - 4,
                                LN_DESTINATION_ROWS[LN_CALLS[index].destination_slot] * 8 - 4,
                                plug_tile, cord->palette_bank,
                                LN_SPRITE_16, 0, true, false);
        }

        uint8_t lamp_frame = (uint8_t)(index +
            (s_state.calls[index].state == LN_CALL_RINGING ? 8u : 0u));
        if (s_state.calls[index].state != LN_CALL_LIVE &&
            !(s_state.picked_call == LN_NO_SELECTION &&
              s_state.focus == index))
        {
            ln_video_set_sprite((uint8_t)(16u + index),
                                LN_SOURCE_X * 8,
                                LN_CALL_ROWS[index] * 8 - 8,
                                (uint16_t)(LN_OBJ_STATUS_LAMPS + lamp_frame),
                                1, LN_SPRITE_8, 0, false, false);
        }
    }

    for (uint8_t index = 0; index < s_state.crossing_count; ++index)
    {
        const LnCrossing *crossing = &s_state.crossings[index];
        if (crossing->grounded)
        {
            uint8_t cord = crossing->top_cord;
            ln_video_set_sprite((uint8_t)(24u + index),
                                crossing->x * 8 - 4,
                                crossing->y * 8 - 4,
                                (uint16_t)(LN_OBJ_CLAMPS + (index % 8u) * 4u),
                                s_state.cords[cord].palette_bank,
                                LN_SPRITE_16, 0, false, false);
        }
        if (crossing->flash_frames > 0u &&
            (!s_state.reduced_flash || (crossing->flash_frames & 3u) == 0u))
        {
            uint8_t frame = (uint8_t)((crossing->flash_frames / 3u) % 16u);
            ln_video_set_sprite((uint8_t)(40u + index),
                                crossing->x * 8 - 4,
                                crossing->y * 8 - 4,
                                (uint16_t)(LN_OBJ_CROSSTALK + frame * 4u),
                                1, LN_SPRITE_16, 0, false, false);
        }
    }

    for (uint8_t fact = 0; fact < LN_FACT_COUNT; ++fact)
    {
        bool known = false;
        for (uint8_t citizen = 0; citizen < LN_CITIZEN_COUNT; ++citizen)
        {
            if ((s_state.knowledge_mask[citizen] & (1u << fact)) != 0u)
            {
                known = true;
                break;
            }
        }
        if (known)
        {
            ln_video_set_sprite((uint8_t)(56u + fact),
                                176 + (int)(fact % 3u) * 16,
                                48 + (int)(fact / 3u) * 24,
                                (uint16_t)(LN_OBJ_MONITOR_MARKERS +
                                           LN_FACTS[fact].icon),
                                1, LN_SPRITE_8, 0, false, false);
        }
    }

    int cursor_x = LN_SOURCE_X * 8 - 4;
    int cursor_y = LN_CALL_ROWS[0] * 8 - 4;
    uint8_t cursor_frame = 0;
    if (s_state.picked_call != LN_NO_SELECTION)
    {
        cursor_x = LN_DESTINATION_X * 8 - 4;
        cursor_y = LN_DESTINATION_ROWS[s_state.selected_destination] * 8 - 4;
        cursor_frame = 1;
    }
    else if (s_state.focus < LN_CALL_COUNT)
    {
        cursor_y = LN_CALL_ROWS[s_state.focus] * 8 - 4;
    }
    else if (s_state.focus == LN_FOCUS_CROSSING)
    {
        cursor_frame = 2;
        if (s_state.crossing_count > 0u)
        {
            const LnCrossing *crossing =
                &s_state.crossings[s_state.selected_crossing];
            cursor_x = crossing->x * 8 - 4;
            cursor_y = crossing->y * 8 - 4;
        }
        else
        {
            cursor_x = LN_PEG_X * 8 - 4;
            cursor_y = 7 * 8 - 4;
        }
    }
    else if (s_state.focus == LN_FOCUS_PEG)
    {
        cursor_frame = 1;
        cursor_x = LN_PEG_X * 8 - 4;
        uint8_t peg = s_state.cords[s_state.selected_cord].peg;
        cursor_y = (peg == LN_NO_SELECTION ? 7 : LN_PEG_ROWS[peg]) * 8 - 4;
    }
    else
    {
        cursor_frame = 3;
        cursor_x = 168;
        cursor_y = 16;
    }
    ln_video_set_sprite(5, cursor_x, cursor_y,
                        (uint16_t)(LN_OBJ_CURSOR + cursor_frame * 4u),
                        1, LN_SPRITE_16, 0, false, false);
}

static void render_gameplay(void)
{
    if (!s_state.board_built)
    {
        build_gameplay_board();
        rebuild_routes();
    }
    render_header();
    render_jacks();
    render_call_card();
    render_city_memory_panel();
    render_gameplay_sprites();
}

void ln_game_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.screen = LN_SCREEN_TITLE;
    s_state.picked_call = LN_NO_SELECTION;
    s_state.monitored_call = LN_NO_SELECTION;
    s_state.screen_dirty = true;
}

void ln_game_update(u16 pressed, u16 held, u16 released)
{
    switch (s_state.screen)
    {
        case LN_SCREEN_TITLE:
            if ((pressed & (KEY_START | KEY_A)) != 0u)
            {
                enter_screen(LN_SCREEN_MODE_SELECT);
            }
            break;

        case LN_SCREEN_MODE_SELECT:
            if ((pressed & KEY_UP) != 0u)
            {
                s_state.menu_selection = s_state.menu_selection == 0u
                                             ? LN_MODE_COUNT - 1u
                                             : (uint8_t)(s_state.menu_selection - 1u);
                s_state.screen_dirty = true;
            }
            if ((pressed & KEY_DOWN) != 0u)
            {
                s_state.menu_selection =
                    (uint8_t)((s_state.menu_selection + 1u) % LN_MODE_COUNT);
                s_state.screen_dirty = true;
            }
            if ((pressed & (KEY_A | KEY_START)) != 0u)
            {
                s_state.mode = (LnMode)s_state.menu_selection;
                if (s_state.mode == LN_MODE_PLAYBACK)
                {
                    s_state.playback_offset = 0;
                    enter_screen(LN_SCREEN_PLAYBACK);
                }
                else
                {
                    enter_screen(LN_SCREEN_BRIEF);
                }
            }
            if ((pressed & KEY_B) != 0u)
            {
                enter_screen(LN_SCREEN_TITLE);
            }
            break;

        case LN_SCREEN_BRIEF:
            if ((pressed & (KEY_A | KEY_START)) != 0u)
            {
                start_shift();
            }
            else if ((pressed & KEY_B) != 0u)
            {
                enter_screen(LN_SCREEN_MODE_SELECT);
            }
            break;

        case LN_SCREEN_GAMEPLAY:
            update_gameplay(pressed, held, released);
            break;

        case LN_SCREEN_PAUSE:
            if ((pressed & (KEY_START | KEY_B)) != 0u)
            {
                enter_screen(LN_SCREEN_GAMEPLAY);
            }
            else
            {
                if ((pressed & KEY_UP) != 0u)
                {
                    s_state.pause_selection = s_state.pause_selection == 0u
                                                  ? 3u
                                                  : (uint8_t)(s_state.pause_selection - 1u);
                    s_state.screen_dirty = true;
                }
                if ((pressed & KEY_DOWN) != 0u)
                {
                    s_state.pause_selection =
                        (uint8_t)((s_state.pause_selection + 1u) % 4u);
                    s_state.screen_dirty = true;
                }
                if ((pressed & KEY_A) != 0u)
                {
                    if (s_state.pause_selection == 0u)
                    {
                        s_state.high_contrast = !s_state.high_contrast;
                        s_state.screen_dirty = true;
                    }
                    else if (s_state.pause_selection == 1u)
                    {
                        s_state.reduced_flash = !s_state.reduced_flash;
                        s_state.screen_dirty = true;
                    }
                    else if (s_state.pause_selection == 2u)
                    {
                        enter_screen(LN_SCREEN_GAMEPLAY);
                    }
                    else
                    {
                        if (s_state.mode == LN_MODE_OPEN_LINES)
                        {
                            finish_shift();
                        }
                        else
                        {
                            set_message("SHIFT STILL LIVE", "DAWN HAS NOT COME");
                            enter_screen(LN_SCREEN_GAMEPLAY);
                        }
                    }
                }
            }
            break;

        case LN_SCREEN_MEMORY:
            if ((pressed & KEY_UP) != 0u)
            {
                s_state.selected_fact = s_state.selected_fact == 0u
                                            ? LN_FACT_COUNT - 1u
                                            : (uint8_t)(s_state.selected_fact - 1u);
                s_state.screen_dirty = true;
            }
            if ((pressed & KEY_DOWN) != 0u)
            {
                s_state.selected_fact =
                    (uint8_t)((s_state.selected_fact + 1u) % LN_FACT_COUNT);
                s_state.screen_dirty = true;
            }
            if ((pressed & (KEY_SELECT | KEY_B)) != 0u)
            {
                enter_screen(LN_SCREEN_GAMEPLAY);
            }
            break;

        case LN_SCREEN_REPORT:
            if ((pressed & (KEY_A | KEY_START)) != 0u)
            {
                enter_screen(LN_SCREEN_EPILOGUE);
            }
            break;

        case LN_SCREEN_EPILOGUE:
            if ((pressed & KEY_A) != 0u)
            {
                s_state.playback_offset = 0;
                enter_screen(LN_SCREEN_PLAYBACK);
            }
            else if ((pressed & KEY_B) != 0u)
            {
                enter_screen(LN_SCREEN_MODE_SELECT);
            }
            break;

        case LN_SCREEN_PLAYBACK:
            if ((pressed & KEY_LEFT) != 0u && s_state.playback_offset > 0u)
            {
                --s_state.playback_offset;
                s_state.screen_dirty = true;
            }
            if ((pressed & KEY_RIGHT) != 0u &&
                s_state.playback_offset + 1u < s_state.event_count)
            {
                ++s_state.playback_offset;
                s_state.screen_dirty = true;
            }
            if ((pressed & KEY_B) != 0u)
            {
                enter_screen(LN_SCREEN_MODE_SELECT);
            }
            break;
    }
}

void ln_game_render(void)
{
    if (s_state.screen == LN_SCREEN_GAMEPLAY)
    {
        render_gameplay();
        s_state.screen_dirty = false;
        return;
    }
    if (!s_state.screen_dirty)
    {
        return;
    }

    switch (s_state.screen)
    {
        case LN_SCREEN_TITLE:
            ln_video_load_title();
            break;
        case LN_SCREEN_MODE_SELECT:
            render_mode_select();
            break;
        case LN_SCREEN_BRIEF:
            render_brief();
            break;
        case LN_SCREEN_PAUSE:
            render_pause();
            break;
        case LN_SCREEN_MEMORY:
            render_memory();
            break;
        case LN_SCREEN_REPORT:
            render_report();
            break;
        case LN_SCREEN_EPILOGUE:
            render_epilogue();
            break;
        case LN_SCREEN_PLAYBACK:
            render_playback();
            break;
        case LN_SCREEN_GAMEPLAY:
            break;
    }
    s_state.screen_dirty = false;
}

#ifdef LN_GAME_DEBUG
void ln_game_debug_snapshot(LnGameDebugSnapshot *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->screen = (uint8_t)s_state.screen;
    snapshot->mode = (uint8_t)s_state.mode;
    snapshot->focus = s_state.focus;
    snapshot->picked_call = s_state.picked_call;
    snapshot->selected_destination = s_state.selected_destination;
    snapshot->selected_crossing = s_state.selected_crossing;
    snapshot->crossing_count = s_state.crossing_count;
    for (uint8_t index = 0; index < s_state.crossing_count; ++index)
    {
        if (s_state.crossings[index].grounded)
        {
            ++snapshot->grounded_count;
        }
    }
    for (uint8_t index = 0; index < LN_CALL_COUNT; ++index)
    {
        snapshot->call_state[index] = (uint8_t)s_state.calls[index].state;
        snapshot->call_serviced[index] = s_state.calls[index].serviced ? 1u : 0u;
        snapshot->cord_live[index] = s_state.cords[index].live ? 1u : 0u;
        snapshot->packet_fact_id[index] = current_packet_fact(&s_state.cords[index]);
    }
    snapshot->missed_calls = s_state.missed_calls;
    snapshot->privacy_leaks = s_state.privacy_leaks;
    snapshot->useful_leaks = s_state.useful_leaks;
    snapshot->suppressed_leaks = s_state.suppressed_leaks;
    snapshot->cabs_east = s_state.cabs_east;
    snapshot->platform_sources = s_state.platform_sources;
    snapshot->objective_pass_mask = objective_pass_mask();
    snapshot->shift_complete =
        snapshot->objective_pass_mask == LN_OBJECTIVE_ALL_MASK ? 1u : 0u;
    snapshot->score_service = s_state.score_service;
    snapshot->score_discretion = s_state.score_discretion;
    snapshot->score_resonance = s_state.score_resonance;
    snapshot->score_coherence = s_state.score_coherence;
    snapshot->epilogue_index = s_state.epilogue_index;
    snapshot->last_monitored_call = s_state.last_monitored_call;
    snapshot->repeat_frames = s_state.repeat_frames;
    snapshot->elapsed_frames = s_state.elapsed_frames;
    uint8_t platform_fact = fact_index_from_id(1);
    if (platform_fact != LN_NO_SELECTION)
    {
        snapshot->owen_platform_source_mask =
            s_state.source_mask[1][platform_fact];
    }
}
#endif
