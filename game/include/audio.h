#ifndef LINE_NOISE_AUDIO_H
#define LINE_NOISE_AUDIO_H

#include <stdbool.h>

typedef enum ln_sound_id {
	LN_SOUND_RING_SUBSCRIBER = 0,
	LN_SOUND_PLUG_CLICK,
	LN_SOUND_CORD_MOVE,
	LN_SOUND_CROSSTALK,
	LN_SOUND_PACKET_TRANSFER,
	LN_SOUND_GROUND_CLAMP,
	LN_SOUND_VERIFY,
	LN_SOUND_DAWN_CHIME,
	LN_SOUND_COUNT
} ln_sound_id;

void ln_audio_init(void);
bool ln_audio_play(ln_sound_id id);
void ln_audio_update(void);
void ln_audio_stop(void);

#endif
