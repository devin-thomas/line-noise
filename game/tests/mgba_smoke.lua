local state_hex = assert(os.getenv("LINE_NOISE_STATE"),
    "LINE_NOISE_STATE is required")
local output_dir = assert(os.getenv("LINE_NOISE_OUTPUT"),
    "LINE_NOISE_OUTPUT is required")
local result_path = assert(os.getenv("LINE_NOISE_RESULT"),
    "LINE_NOISE_RESULT is required")
local state_address = assert(tonumber(state_hex, 16),
    "LINE_NOISE_STATE must be hexadecimal")

local function required_number(name)
    local raw_value = assert(os.getenv(name), name .. " is required")
    return assert(tonumber(raw_value), name .. " must be numeric")
end

local probe_version = required_number("LINE_NOISE_PROBE_VERSION")
local calls_offset = required_number("LINE_NOISE_CALLS_OFFSET")
local call_stride = required_number("LINE_NOISE_CALL_STRIDE")
local call_state_offset = required_number("LINE_NOISE_CALL_STATE_OFFSET")
local cords_offset = required_number("LINE_NOISE_CORDS_OFFSET")
local cord_stride = required_number("LINE_NOISE_CORD_STRIDE")
local cord_live_offset = required_number("LINE_NOISE_CORD_LIVE_OFFSET")
local crossing_count_offset = required_number(
    "LINE_NOISE_CROSSING_COUNT_OFFSET")

local KEY_A = 0
local KEY_B = 1
local KEY_SELECT = 2
local KEY_START = 3
local KEY_RIGHT = 4
local KEY_DOWN = 7
local SCREEN_TITLE = 0
local SCREEN_MODE_SELECT = 1
local SCREEN_BRIEF = 2
local SCREEN_GAMEPLAY = 3
local SCREEN_PAUSE = 4
local SCREEN_MEMORY = 5
local SCREEN_REPORT = 6
local SCREEN_EPILOGUE = 7
local SCREEN_PLAYBACK = 8
local CALL_LIVE = 2
local last_input_trace = "no input yet"

local function run_frames(count)
    for _ = 1, count do
        emu:runFrame()
    end
end

local function tap(key)
    local frame_before = emu:currentFrame()
    emu:addKey(key)
    local active_keys = emu:getKeys()
    local keypad_before = emu:read16(0x04000130)
    run_frames(12)
    local keypad_after = emu:read16(0x04000130)
    emu:clearKey(key)
    run_frames(2)
    last_input_trace = string.format(
        "key=%u active=0x%X keypad=0x%X->0x%X frame=%u->%u",
        key, active_keys, keypad_before, keypad_after,
        frame_before, emu:currentFrame())
end

local function hold(key, frames)
    emu:addKey(key)
    run_frames(frames)
    emu:clearKey(key)
    emu:runFrame()
end

local function assert_equal(actual, expected, label)
    if actual ~= expected then
        error(string.format("%s: expected %s, got %s",
            label, tostring(expected), tostring(actual)))
    end
end

local function assert_screen(expected, label)
    assert_equal(emu:read8(state_address), expected,
        label .. " [" .. last_input_trace .. "]")
end

local function assert_routing(expected_live_mask, expected_crossings, label)
    assert_screen(SCREEN_GAMEPLAY, label .. " gameplay screen")
    local live_mask = 0
    for index = 0, 3 do
        local live_address = state_address + cords_offset
            + index * cord_stride + cord_live_offset
        local live = emu:read8(live_address)
        if live ~= 0 and live ~= 1 then
            error(string.format("%s cord %u live flag: expected 0 or 1, got %u",
                label, index, live))
        end
        if live == 1 then
            live_mask = live_mask + 2 ^ index
            local call_address = state_address + calls_offset
                + index * call_stride + call_state_offset
            assert_equal(emu:read8(call_address), CALL_LIVE,
                string.format("%s call %u state", label, index))
        end
    end
    assert_equal(live_mask, expected_live_mask, label .. " live cord mask")
    assert_equal(emu:read8(state_address + crossing_count_offset),
        expected_crossings, label .. " crossing count")
end

local function screenshot(name)
    emu:screenshot(output_dir .. "/" .. name .. ".png")
end

local function reset_game()
    emu:setKeys(0)
    emu:reset()
    run_frames(120)
    assert_screen(SCREEN_TITLE, "screen after reset")
end

local function start_open_lines()
    reset_game()
    tap(KEY_A)
    assert_screen(SCREEN_MODE_SELECT, "Open Lines mode select")
    tap(KEY_DOWN)
    tap(KEY_DOWN)
    tap(KEY_A)
    assert_screen(SCREEN_BRIEF, "Open Lines brief")
    tap(KEY_A)
    assert_screen(SCREEN_GAMEPLAY, "Open Lines gameplay")
end

local function verify_all_screens()
    reset_game()
    assert_equal(emu:getGameTitle(), "LINE NOISE!", "ROM title")
    assert_equal(emu:getGameCode(), "AGB-LNSE", "ROM game code")
    screenshot("01-title")

    tap(KEY_A)
    assert_screen(SCREEN_MODE_SELECT, "mode select")
    screenshot("02-mode-select")

    tap(KEY_A)
    assert_screen(SCREEN_BRIEF, "shift brief")
    screenshot("03-shift-brief")

    tap(KEY_A)
    assert_screen(SCREEN_GAMEPLAY, "gameplay")
    screenshot("04-gameplay")

    tap(KEY_START)
    assert_screen(SCREEN_PAUSE, "pause")
    screenshot("05-pause")

    tap(KEY_DOWN)
    tap(KEY_DOWN)
    tap(KEY_DOWN)
    tap(KEY_A)
    assert_screen(SCREEN_GAMEPLAY,
        "finite shift remains live when dawn has not come")

    tap(KEY_SELECT)
    assert_screen(SCREEN_MEMORY, "city memory")
    screenshot("06-city-memory")
    tap(KEY_SELECT)
    assert_screen(SCREEN_GAMEPLAY, "return from city memory")

    start_open_lines()
    tap(KEY_START)
    assert_screen(SCREEN_PAUSE, "Open Lines pause")
    tap(KEY_DOWN)
    tap(KEY_DOWN)
    tap(KEY_DOWN)
    tap(KEY_A)
    assert_screen(SCREEN_REPORT, "shift report")
    screenshot("07-shift-report")

    tap(KEY_A)
    assert_screen(SCREEN_EPILOGUE, "epilogue")
    screenshot("08-epilogue")

    tap(KEY_A)
    assert_screen(SCREEN_PLAYBACK, "playback")
    screenshot("09-playback")
end

local function verify_live_routing()
    start_open_lines()

    run_frames(65)
    hold(KEY_B, 12)
    tap(KEY_A)
    tap(KEY_A)
    assert_routing(1, 0, "first connection")

    run_frames(160)
    tap(KEY_DOWN)
    hold(KEY_B, 12)
    tap(KEY_A)
    tap(KEY_RIGHT)
    tap(KEY_A)
    assert_routing(3, 0, "second connection")

    run_frames(220)
    tap(KEY_DOWN)
    hold(KEY_B, 12)
    tap(KEY_A)
    tap(KEY_RIGHT)
    tap(KEY_RIGHT)
    tap(KEY_A)
    assert_routing(7, 2, "third connection")

    run_frames(220)
    tap(KEY_DOWN)
    hold(KEY_B, 12)
    tap(KEY_A)
    tap(KEY_RIGHT)
    tap(KEY_RIGHT)
    tap(KEY_RIGHT)
    tap(KEY_A)
    run_frames(120)

    assert_routing(15, 3, "fourth connection")
    screenshot("10-four-live-cords")
end

local ok, failure = xpcall(function()
    assert_equal(probe_version, 1, "debug probe version")
    verify_all_screens()
    verify_live_routing()
end, debug.traceback)

emu:setKeys(0)
local temporary_result_path = result_path .. ".tmp"
os.remove(temporary_result_path)
local result = assert(io.open(temporary_result_path, "w"))
if ok then
    result:write("PASS\n")
else
    result:write("FAIL\n", failure, "\n")
end
assert(result:close())
assert(os.rename(temporary_result_path, result_path))
