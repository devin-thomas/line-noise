#include "audio.h"
#include "video.h"

void ln_audio_init(void)
{
}

bool ln_audio_play(ln_sound_id id)
{
    (void)id;
    return true;
}

void ln_audio_update(void)
{
}

void ln_audio_stop(void)
{
}

void ln_video_init(void)
{
}

void ln_video_load_title(void)
{
}

void ln_video_load_interface(uint8_t scene_index, bool high_contrast)
{
    (void)scene_index;
    (void)high_contrast;
}

void ln_video_apply_scene(uint8_t scene_index, bool high_contrast)
{
    (void)scene_index;
    (void)high_contrast;
}

void ln_video_clear_map(uint8_t bg, uint16_t tile, uint8_t palette_bank)
{
    (void)bg;
    (void)tile;
    (void)palette_bank;
}

void ln_video_set_tile(uint8_t bg, int x, int y, uint16_t tile,
                       uint8_t palette_bank, bool hflip, bool vflip)
{
    (void)bg;
    (void)x;
    (void)y;
    (void)tile;
    (void)palette_bank;
    (void)hflip;
    (void)vflip;
}

void ln_video_fill(uint8_t bg, int x, int y, int width, int height,
                   uint16_t tile, uint8_t palette_bank)
{
    (void)bg;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)tile;
    (void)palette_bank;
}

void ln_video_text(uint8_t bg, int x, int y, const char *text,
                   uint8_t palette_bank)
{
    (void)bg;
    (void)x;
    (void)y;
    (void)text;
    (void)palette_bank;
}

void ln_video_clear_text_rect(int x, int y, int width, int height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

void ln_video_hide_sprites(void)
{
}

void ln_video_set_sprite(uint8_t index, int x, int y, uint16_t tile,
                         uint8_t palette_bank, LnSpriteSize size,
                         uint8_t priority, bool hflip, bool vflip)
{
    (void)index;
    (void)x;
    (void)y;
    (void)tile;
    (void)palette_bank;
    (void)size;
    (void)priority;
    (void)hflip;
    (void)vflip;
}

void ln_video_present(void)
{
}
