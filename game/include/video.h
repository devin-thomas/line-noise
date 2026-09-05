#ifndef LINE_NOISE_VIDEO_H
#define LINE_NOISE_VIDEO_H

#include <gba.h>
#include <stdbool.h>
#include <stdint.h>

enum
{
    LN_TILE_COLUMNS = 30,
    LN_TILE_ROWS = 20,
    LN_MAP_COLUMNS = 32,
    LN_MAP_ROWS = 32,
    LN_MAP_CELLS = LN_MAP_COLUMNS * LN_MAP_ROWS,
    LN_OAM_COUNT = 128
};

enum
{
    LN_OBJ_PORTRAITS = 0,
    LN_OBJ_PACKETS = 256,
    LN_OBJ_CURSOR = 288,
    LN_OBJ_PLUGS = 304,
    LN_OBJ_SERVICE_BADGES = 368,
    LN_OBJ_STATUS_LAMPS = 400,
    LN_OBJ_CROSSTALK = 416,
    LN_OBJ_CALL_BURSTS = 480,
    LN_OBJ_MONITOR_MARKERS = 512,
    LN_OBJ_CLAMPS = 544,
    LN_OBJ_HUD_ICONS = 576
};

typedef enum LnSpriteSize
{
    LN_SPRITE_8 = 0,
    LN_SPRITE_16,
    LN_SPRITE_32,
    LN_SPRITE_64
} LnSpriteSize;

void ln_video_init(void);
void ln_video_load_title(void);
void ln_video_load_interface(uint8_t scene_index, bool high_contrast);
void ln_video_apply_scene(uint8_t scene_index, bool high_contrast);

void ln_video_clear_map(uint8_t bg, uint16_t tile, uint8_t palette_bank);
void ln_video_set_tile(uint8_t bg, int x, int y, uint16_t tile,
                       uint8_t palette_bank, bool hflip, bool vflip);
void ln_video_fill(uint8_t bg, int x, int y, int width, int height,
                   uint16_t tile, uint8_t palette_bank);
void ln_video_text(uint8_t bg, int x, int y, const char *text,
                   uint8_t palette_bank);
void ln_video_clear_text_rect(int x, int y, int width, int height);

void ln_video_hide_sprites(void);
void ln_video_set_sprite(uint8_t index, int x, int y, uint16_t tile,
                         uint8_t palette_bank, LnSpriteSize size,
                         uint8_t priority, bool hflip, bool vflip);
void ln_video_present(void);

#endif
