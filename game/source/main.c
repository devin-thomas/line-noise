#include <gba.h>

#include "audio.h"
#include "game.h"
#include "video.h"

int main(void)
{
    irqInit();
    irqEnable(IRQ_VBLANK);
    REG_IME = 1;

    ln_video_init();
    ln_audio_init();
    ln_game_init();
    ln_game_render();

    while (1)
    {
        VBlankIntrWait();
        ln_video_present();
        scanKeys();
        ln_audio_update();
        ln_game_update(keysDown(), keysHeld(), keysUp());
        ln_game_render();
    }
}
