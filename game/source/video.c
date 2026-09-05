#include "video.h"

#include <string.h>

#include "animated_console_tiles.h"
#include "call_bursts.h"
#include "caller_portraits.h"
#include "city_monitor_tiles.h"
#include "cord_clamps.h"
#include "cord_palette_banks.h"
#include "cord_tiles.h"
#include "crosstalk_fx.h"
#include "cursor.h"
#include "exchange_tiles.h"
#include "font.h"
#include "hud_icons.h"
#include "hud_panels.h"
#include "monitor_markers.h"
#include "plugs.h"
#include "scene_palettes.h"
#include "service_badges.h"
#include "signal_packets.h"
#include "status_lamps.h"
#include "title_screen_map.h"
#include "title_screen_tiles.h"

enum
{
    LN_SCREEN_BLOCK_FIRST = 28,
    LN_FONT_INTERFACE_BASE = 0,
    LN_FONT_TITLE_BASE = 176,
    LN_HUD_PANEL_BASE = 64,
    LN_CITY_TILE_BASE = 192,
    LN_ANIM_TILE_BASE = 320,
    LN_FONT_PALETTE = 8,
    LN_TITLE_BANG_TILE_BASE = 240,
    LN_TITLE_EXCLAMATION_X = 26,
    LN_TITLE_EXCLAMATION_Y = 7,
    LN_TITLE_BANG_TILE_COUNT = 3
};

static const char LN_FONT_ORDER[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:.,!?-/+=<>'#%&() ";

EWRAM_BSS static uint16_t s_maps[4][LN_MAP_CELLS];
EWRAM_BSS static volatile uint32_t
    s_title_bang_tiles[LN_TITLE_BANG_TILE_COUNT * 8];
static bool s_map_dirty[4];
EWRAM_BSS static OBJATTR s_oam[LN_OAM_COUNT];
static uint16_t s_font_base;
static uint16_t s_space_tile;
static volatile uint32_t s_dma_zero;
static uint16_t s_pending_dispcnt;
static bool s_display_enable_pending;

static uint16_t map_entry(uint16_t tile, uint8_t palette_bank,
                          bool hflip, bool vflip)
{
    uint16_t entry = (uint16_t)(tile & 0x03ffu);
    if (hflip)
    {
        entry |= 0x0400u;
    }
    if (vflip)
    {
        entry |= 0x0800u;
    }
    entry |= (uint16_t)((uint16_t)(palette_bank & 0x0fu) << 12);
    return entry;
}

static uint16_t font_tile(char character)
{
    const char *position = strchr(LN_FONT_ORDER, character);
    if (position == NULL)
    {
        position = strchr(LN_FONT_ORDER, ' ');
    }
    return (uint16_t)(s_font_base + (uint16_t)(position - LN_FONT_ORDER));
}

static void title_bang_set_pixel(volatile uint8_t *tiles,
                                 uint8_t x, uint8_t y,
                                 uint8_t palette_index)
{
    uint16_t byte_index = (uint16_t)((y / 8u) * 32u +
                                     (y % 8u) * 4u + x / 2u);
    uint8_t shift = (uint8_t)((x & 1u) * 4u);
    tiles[byte_index] |= (uint8_t)(palette_index << shift);
}

static void load_title_bang(void)
{
    volatile uint8_t *tiles = (volatile uint8_t *)s_title_bang_tiles;

    for (uint8_t word = 0; word < LN_TITLE_BANG_TILE_COUNT * 8u; ++word)
    {
        s_title_bang_tiles[word] = 0;
    }

    for (uint8_t y = 0; y < 14u; ++y)
    {
        title_bang_set_pixel(tiles, 0, y, 15);
        for (uint8_t x = 1; x < 4u; ++x)
        {
            title_bang_set_pixel(tiles, x, y, 7);
        }
        title_bang_set_pixel(tiles, 4, y, 6);
    }
    for (uint8_t y = 18; y < 23u; ++y)
    {
        title_bang_set_pixel(tiles, 0, y, 15);
        for (uint8_t x = 1; x < 4u; ++x)
        {
            title_bang_set_pixel(tiles, x, y, 7);
        }
        title_bang_set_pixel(tiles, 4, y, 6);
    }

    dmaCopy((const void *)s_title_bang_tiles,
            (uint8_t *)TILE_BASE_ADR(0) + LN_TITLE_BANG_TILE_BASE * 32u,
            sizeof(s_title_bang_tiles));
}

static void clear_vram(void)
{
    s_dma_zero = 0;
    REG_DMA3SAD = (uint32_t)(uintptr_t)&s_dma_zero;
    REG_DMA3DAD = (uint32_t)(uintptr_t)VRAM;
    REG_DMA3CNT = 0x80000000u | (uint32_t)DMA_SRC_FIXED |
                  (uint32_t)DMA32 | (0x18000u / 4u);
}

static void repack_frames(const void *source, uint16_t source_width_tiles,
                          uint8_t frame_width_tiles,
                          uint8_t frame_height_tiles, uint8_t frame_count,
                          uint16_t destination_tile)
{
    const uint8_t *source_bytes = (const uint8_t *)source;
    uint8_t frames_per_row =
        (uint8_t)(source_width_tiles / frame_width_tiles);

    for (uint8_t frame = 0; frame < frame_count; ++frame)
    {
        uint16_t frame_x =
            (uint16_t)((frame % frames_per_row) * frame_width_tiles);
        uint16_t frame_y =
            (uint16_t)((frame / frames_per_row) * frame_height_tiles);
        for (uint8_t tile_y = 0; tile_y < frame_height_tiles; ++tile_y)
        {
            for (uint8_t tile_x = 0; tile_x < frame_width_tiles; ++tile_x)
            {
                uint16_t source_tile = (uint16_t)(
                    (frame_y + tile_y) * source_width_tiles +
                    frame_x + tile_x);
                uint16_t frame_tile = (uint16_t)(
                    frame * frame_width_tiles * frame_height_tiles +
                    tile_y * frame_width_tiles + tile_x);
                uint8_t *destination =
                    (uint8_t *)OBJ_BASE_ADR +
                    (uint32_t)(destination_tile + frame_tile) * 32u;
                dmaCopy(source_bytes + (uint32_t)source_tile * 32u,
                        destination, 32u);
            }
        }
    }
}

static void load_object_art(void)
{
    repack_frames(caller_portraitsTiles, 16, 4, 4, 16,
                  LN_OBJ_PORTRAITS);
    repack_frames(signal_packetsTiles, 16, 1, 1, 32,
                  LN_OBJ_PACKETS);
    repack_frames(cursorTiles, 8, 2, 2, 4, LN_OBJ_CURSOR);
    repack_frames(plugsTiles, 16, 2, 2, 16, LN_OBJ_PLUGS);
    repack_frames(service_badgesTiles, 16, 2, 2, 8,
                  LN_OBJ_SERVICE_BADGES);
    repack_frames(status_lampsTiles, 8, 1, 1, 16,
                  LN_OBJ_STATUS_LAMPS);
    repack_frames(crosstalk_fxTiles, 16, 2, 2, 16,
                  LN_OBJ_CROSSTALK);
    repack_frames(call_burstsTiles, 16, 2, 2, 8,
                  LN_OBJ_CALL_BURSTS);
    repack_frames(monitor_markersTiles, 16, 1, 1, 32,
                  LN_OBJ_MONITOR_MARKERS);
    repack_frames(cord_clampsTiles, 16, 2, 2, 8,
                  LN_OBJ_CLAMPS);
    repack_frames(hud_iconsTiles, 16, 2, 2, 8,
                  LN_OBJ_HUD_ICONS);

    dmaCopy(caller_portraitsPal, SPRITE_PALETTE, 32u);
    dmaCopy(signal_packetsPal, SPRITE_PALETTE + 16, 32u);
    for (uint8_t bank = 0; bank < LINE_NOISE_CORD_BANK_COUNT; ++bank)
    {
        dmaCopy(line_noise_cord_palette_banks[bank],
                SPRITE_PALETTE + (uint16_t)(bank + 2u) * 16u, 32u);
    }
}

void ln_video_init(void)
{
    REG_DISPCNT = LCDC_OFF;
    s_pending_dispcnt = LCDC_OFF;
    s_display_enable_pending = false;
    clear_vram();
    memset(s_maps, 0, sizeof(s_maps));
    memset(s_map_dirty, 0, sizeof(s_map_dirty));
    ln_video_hide_sprites();
    load_object_art();
    dmaCopy(s_oam, OAM, sizeof(s_oam));
}

void ln_video_load_title(void)
{
    REG_DISPCNT = LCDC_OFF;
    s_display_enable_pending = false;
    clear_vram();
    load_object_art();

    dmaCopy(title_screen_tilesTiles, TILE_BASE_ADR(0),
            title_screen_tilesTilesLen);
    dmaCopy(fontTiles,
            (uint8_t *)TILE_BASE_ADR(0) + LN_FONT_TITLE_BASE * 32u,
            fontTilesLen);
    load_title_bang();
    dmaCopy(title_screen_tilesPal, BG_PALETTE, 32u);
    dmaCopy(fontPal, BG_PALETTE + LN_FONT_PALETTE * 16u, 32u);

    memcpy(s_maps[0], line_noise_title_screenMap,
           sizeof(line_noise_title_screenMap));
    s_font_base = LN_FONT_TITLE_BASE;
    s_space_tile = font_tile(' ');
    for (uint8_t bg = 1; bg < 4; ++bg)
    {
        ln_video_clear_map(bg, s_space_tile, LN_FONT_PALETTE);
    }
    for (uint8_t tile = 0; tile < LN_TITLE_BANG_TILE_COUNT; ++tile)
    {
        ln_video_set_tile(0, LN_TITLE_EXCLAMATION_X,
                          (uint8_t)(LN_TITLE_EXCLAMATION_Y + tile),
                          (uint16_t)(LN_TITLE_BANG_TILE_BASE + tile),
                          0, false, false);
    }
    s_map_dirty[0] = true;

    REG_BG0CNT = BG_TILE_BASE(0) | BG_MAP_BASE(28) |
                 BG_16_COLOR | BG_SIZE_0 | BG_PRIORITY(1);
    REG_BG1CNT = BG_TILE_BASE(0) | BG_MAP_BASE(29) |
                 BG_16_COLOR | BG_SIZE_0 | BG_PRIORITY(0);
    REG_BG2CNT = BG_TILE_BASE(2) | BG_MAP_BASE(30) |
                 BG_16_COLOR | BG_SIZE_0 | BG_PRIORITY(2);
    REG_BG3CNT = BG_TILE_BASE(2) | BG_MAP_BASE(31) |
                 BG_16_COLOR | BG_SIZE_0 | BG_PRIORITY(3);
    ln_video_hide_sprites();
    s_pending_dispcnt = MODE_0 | BG0_ON | BG1_ON | OBJ_ON | OBJ_1D_MAP;
    s_display_enable_pending = true;
}

void ln_video_apply_scene(uint8_t scene_index, bool high_contrast)
{
    uint8_t selected_scene = high_contrast ? 4u : scene_index;
    if (selected_scene >= LINE_NOISE_SCENE_COUNT)
    {
        selected_scene = 0;
    }

    dmaCopy(line_noise_scene_palettes[selected_scene][0],
            BG_PALETTE, 32u);
    dmaCopy(line_noise_scene_palettes[selected_scene][1],
            BG_PALETTE + 16, 32u);
    for (uint8_t bank = 0; bank < LINE_NOISE_CORD_BANK_COUNT; ++bank)
    {
        dmaCopy(line_noise_cord_palette_banks[bank],
                BG_PALETTE + (uint16_t)(bank + 2u) * 16u, 32u);
    }
    dmaCopy(line_noise_scene_palettes[selected_scene][3],
            BG_PALETTE + LN_FONT_PALETTE * 16u, 32u);
    dmaCopy(line_noise_scene_palettes[selected_scene][5],
            BG_PALETTE + 9u * 16u, 32u);

    dmaCopy(line_noise_scene_palettes[selected_scene][2],
            SPRITE_PALETTE, 32u);
    dmaCopy(line_noise_scene_palettes[selected_scene][3],
            SPRITE_PALETTE + 16, 32u);
    for (uint8_t bank = 0; bank < LINE_NOISE_CORD_BANK_COUNT; ++bank)
    {
        dmaCopy(line_noise_cord_palette_banks[bank],
                SPRITE_PALETTE + (uint16_t)(bank + 2u) * 16u, 32u);
    }
}

void ln_video_load_interface(uint8_t scene_index, bool high_contrast)
{
    REG_DISPCNT = LCDC_OFF;
    s_display_enable_pending = false;
    clear_vram();
    load_object_art();

    dmaCopy(fontTiles, TILE_BASE_ADR(0), fontTilesLen);
    dmaCopy(hud_panelsTiles,
            (uint8_t *)TILE_BASE_ADR(0) + LN_HUD_PANEL_BASE * 32u,
            hud_panelsTilesLen);
    dmaCopy(exchange_tilesTiles, TILE_BASE_ADR(1),
            exchange_tilesTilesLen);
    dmaCopy(city_monitor_tilesTiles,
            (uint8_t *)TILE_BASE_ADR(1) + LN_CITY_TILE_BASE * 32u,
            city_monitor_tilesTilesLen);
    dmaCopy(animated_console_tilesTiles,
            (uint8_t *)TILE_BASE_ADR(1) + LN_ANIM_TILE_BASE * 32u,
            animated_console_tilesTilesLen);
    dmaCopy(cord_tilesTiles, TILE_BASE_ADR(2), cord_tilesTilesLen);

    s_font_base = LN_FONT_INTERFACE_BASE;
    s_space_tile = font_tile(' ');
    ln_video_apply_scene(scene_index, high_contrast);
    ln_video_clear_map(0, s_space_tile, LN_FONT_PALETTE);
    ln_video_clear_map(1, 0, 0);
    ln_video_clear_map(2, 0, 2);
    ln_video_clear_map(3, 0, 2);
    ln_video_hide_sprites();

    REG_BG0CNT = BG_TILE_BASE(0) | BG_MAP_BASE(28) |
                 BG_16_COLOR | BG_SIZE_0 | BG_PRIORITY(0);
    REG_BG1CNT = BG_TILE_BASE(1) | BG_MAP_BASE(29) |
                 BG_16_COLOR | BG_SIZE_0 | BG_PRIORITY(3);
    REG_BG2CNT = BG_TILE_BASE(2) | BG_MAP_BASE(30) |
                 BG_16_COLOR | BG_SIZE_0 | BG_PRIORITY(2);
    REG_BG3CNT = BG_TILE_BASE(2) | BG_MAP_BASE(31) |
                 BG_16_COLOR | BG_SIZE_0 | BG_PRIORITY(1);
    s_pending_dispcnt = MODE_0 | BG0_ON | BG1_ON | BG2_ON | BG3_ON |
                        OBJ_ON | OBJ_1D_MAP;
    s_display_enable_pending = true;
}

void ln_video_clear_map(uint8_t bg, uint16_t tile, uint8_t palette_bank)
{
    if (bg >= 4)
    {
        return;
    }
    uint16_t entry = map_entry(tile, palette_bank, false, false);
    for (uint16_t index = 0; index < LN_MAP_CELLS; ++index)
    {
        s_maps[bg][index] = entry;
    }
    s_map_dirty[bg] = true;
}

void ln_video_set_tile(uint8_t bg, int x, int y, uint16_t tile,
                       uint8_t palette_bank, bool hflip, bool vflip)
{
    if (bg >= 4 || x < 0 || x >= LN_MAP_COLUMNS ||
        y < 0 || y >= LN_MAP_ROWS)
    {
        return;
    }
    s_maps[bg][y * LN_MAP_COLUMNS + x] =
        map_entry(tile, palette_bank, hflip, vflip);
    s_map_dirty[bg] = true;
}

void ln_video_fill(uint8_t bg, int x, int y, int width, int height,
                   uint16_t tile, uint8_t palette_bank)
{
    for (int row = 0; row < height; ++row)
    {
        for (int column = 0; column < width; ++column)
        {
            ln_video_set_tile(bg, x + column, y + row, tile,
                              palette_bank, false, false);
        }
    }
}

void ln_video_text(uint8_t bg, int x, int y, const char *text,
                   uint8_t palette_bank)
{
    if (text == NULL)
    {
        return;
    }
    int origin_x = x;
    while (*text != '\0' && y < LN_TILE_ROWS)
    {
        if (*text == '\n')
        {
            x = origin_x;
            ++y;
        }
        else
        {
            if (x >= 0 && x < LN_TILE_COLUMNS)
            {
                ln_video_set_tile(bg, x, y, font_tile(*text),
                                  palette_bank, false, false);
            }
            ++x;
        }
        ++text;
    }
}

void ln_video_clear_text_rect(int x, int y, int width, int height)
{
    ln_video_fill(0, x, y, width, height, s_space_tile,
                  LN_FONT_PALETTE);
}

void ln_video_hide_sprites(void)
{
    for (uint16_t index = 0; index < LN_OAM_COUNT; ++index)
    {
        s_oam[index].attr0 = ATTR0_DISABLED;
        s_oam[index].attr1 = 0;
        s_oam[index].attr2 = 0;
        s_oam[index].dummy = 0;
    }
}

void ln_video_set_sprite(uint8_t index, int x, int y, uint16_t tile,
                         uint8_t palette_bank, LnSpriteSize size,
                         uint8_t priority, bool hflip, bool vflip)
{
    if (index >= LN_OAM_COUNT)
    {
        return;
    }
    uint16_t size_bits = ATTR1_SIZE_8;
    if (size == LN_SPRITE_16)
    {
        size_bits = ATTR1_SIZE_16;
    }
    else if (size == LN_SPRITE_32)
    {
        size_bits = ATTR1_SIZE_32;
    }
    else if (size == LN_SPRITE_64)
    {
        size_bits = ATTR1_SIZE_64;
    }

    s_oam[index].attr0 = (uint16_t)(ATTR0_SQUARE | ATTR0_COLOR_16 |
                                    ATTR0_NORMAL | (y & 0x00ff));
    s_oam[index].attr1 = (uint16_t)(size_bits | (x & 0x01ff));
    if (hflip)
    {
        s_oam[index].attr1 |= ATTR1_FLIP_X;
    }
    if (vflip)
    {
        s_oam[index].attr1 |= ATTR1_FLIP_Y;
    }
    s_oam[index].attr2 = (uint16_t)(OBJ_CHAR(tile) |
                                    ATTR2_PRIORITY(priority & 3u) |
                                    ATTR2_PALETTE(palette_bank & 15u));
}

void ln_video_present(void)
{
    for (uint8_t bg = 0; bg < 4; ++bg)
    {
        if (s_map_dirty[bg])
        {
            dmaCopy(s_maps[bg], MAP_BASE_ADR(LN_SCREEN_BLOCK_FIRST + bg),
                    sizeof(s_maps[bg]));
            s_map_dirty[bg] = false;
        }
    }
    dmaCopy(s_oam, OAM, sizeof(s_oam));
    if (s_display_enable_pending)
    {
        REG_DISPCNT = s_pending_dispcnt;
        s_display_enable_pending = false;
    }
}
