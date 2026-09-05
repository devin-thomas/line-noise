#include "audio.h"

#include <gba_base.h>
#include <gba_sound.h>
#include <gba_timers.h>

#define LN_AUDIO_SAMPLE_RATE 22050u
#define LN_AUDIO_CPU_CLOCK 16777216u
#define LN_AUDIO_TIMER_PERIOD \
	((LN_AUDIO_CPU_CLOCK + (LN_AUDIO_SAMPLE_RATE / 2u)) / LN_AUDIO_SAMPLE_RATE)
#define LN_AUDIO_TIMER_RELOAD ((u16)(0x10000u - LN_AUDIO_TIMER_PERIOD))
#define LN_AUDIO_FRAME_CYCLES 280896u
#define LN_AUDIO_MAX_SAMPLE_BYTES 23152u
#define LN_AUDIO_SAFE_PADDING_BYTES 512u

#define LN_AUDIO_MASTER_ENABLE ((u16)(1u << 7))
#define LN_AUDIO_DIRECT_A_100 ((u16)(1u << 2))
#define LN_AUDIO_DIRECT_A_RIGHT ((u16)(1u << 8))
#define LN_AUDIO_DIRECT_A_LEFT ((u16)(1u << 9))
#define LN_AUDIO_DIRECT_A_RESET ((u16)(1u << 11))
#define LN_AUDIO_TIMER_ENABLE ((u16)(1u << 7))

#define LN_AUDIO_DMA_DEST_FIXED (2u << 21)
#define LN_AUDIO_DMA_REPEAT (1u << 25)
#define LN_AUDIO_DMA_32 (1u << 26)
#define LN_AUDIO_DMA_FIFO (3u << 28)
#define LN_AUDIO_DMA_ENABLE (1u << 31)

#define LN_AUDIO_REG_DMA1SAD (*(vu32 *)(REG_BASE + 0x00BCu))
#define LN_AUDIO_REG_DMA1DAD (*(vu32 *)(REG_BASE + 0x00C0u))
#define LN_AUDIO_REG_DMA1CNT (*(vu32 *)(REG_BASE + 0x00C4u))

#define LN_AUDIO_DIRECT_A_CONTROL \
	((u16)(LN_AUDIO_DIRECT_A_100 | LN_AUDIO_DIRECT_A_RIGHT | LN_AUDIO_DIRECT_A_LEFT))

extern const u8 ln_audio_ring_subscriber_data[];
extern const u8 ln_audio_plug_click_data[];
extern const u8 ln_audio_cord_move_data[];
extern const u8 ln_audio_crosstalk_data[];
extern const u8 ln_audio_packet_transfer_data[];
extern const u8 ln_audio_ground_clamp_data[];
extern const u8 ln_audio_verify_data[];
extern const u8 ln_audio_dawn_chime_data[];

typedef struct ln_audio_clip {
	const u8 *data;
	u32 length;
} ln_audio_clip;

static const ln_audio_clip ln_audio_clips[LN_SOUND_COUNT] = {
	{ln_audio_ring_subscriber_data, 13671u},
	{ln_audio_plug_click_data, 2315u},
	{ln_audio_cord_move_data, 5292u},
	{ln_audio_crosstalk_data, 8379u},
	{ln_audio_packet_transfer_data, 5292u},
	{ln_audio_ground_clamp_data, 6615u},
	{ln_audio_verify_data, 7276u},
	{ln_audio_dawn_chime_data, 23152u},
};

EWRAM_BSS static u8 ln_audio_buffer[LN_AUDIO_MAX_SAMPLE_BYTES + LN_AUDIO_SAFE_PADDING_BYTES]
	__attribute__((aligned(4)));
static u32 ln_audio_samples_remaining;
static u32 ln_audio_cycle_remainder;
static bool ln_audio_playing;

static void ln_audio_prepare_buffer(const ln_audio_clip *clip)
{
	const u32 word_count = clip->length / 4u;
	const u32 *source_words = (const u32 *)(const void *)clip->data;
	u32 *destination_words = (u32 *)(void *)ln_audio_buffer;
	u32 index;
	u32 byte_index;

	/* GBA Direct Sound consumes signed PCM; 8-bit WAV payloads are unsigned. */
	for (index = 0u; index < word_count; ++index) {
		destination_words[index] = source_words[index] ^ 0x80808080u;
	}

	byte_index = word_count * 4u;
	for (; byte_index < clip->length; ++byte_index) {
		ln_audio_buffer[byte_index] = (u8)(clip->data[byte_index] ^ 0x80u);
	}
	for (; byte_index < clip->length + LN_AUDIO_SAFE_PADDING_BYTES; ++byte_index) {
		ln_audio_buffer[byte_index] = 0u;
	}
}

void ln_audio_init(void)
{
	REG_TM0CNT_H = 0u;
	LN_AUDIO_REG_DMA1CNT = 0u;
	REG_SOUNDCNT_X = LN_AUDIO_MASTER_ENABLE;
	REG_SOUNDCNT_H = (u16)(LN_AUDIO_DIRECT_A_CONTROL | LN_AUDIO_DIRECT_A_RESET);

	ln_audio_samples_remaining = 0u;
	ln_audio_cycle_remainder = 0u;
	ln_audio_playing = false;
}

bool ln_audio_play(ln_sound_id id)
{
	const u32 index = (u32)id;
	const ln_audio_clip *clip;

	if (index >= (u32)LN_SOUND_COUNT) {
		return false;
	}

	clip = &ln_audio_clips[index];
	ln_audio_stop();
	ln_audio_prepare_buffer(clip);

	REG_SOUNDCNT_X = LN_AUDIO_MASTER_ENABLE;
	REG_SOUNDCNT_H = (u16)(LN_AUDIO_DIRECT_A_CONTROL | LN_AUDIO_DIRECT_A_RESET);
	REG_TM0CNT_L = LN_AUDIO_TIMER_RELOAD;
	LN_AUDIO_REG_DMA1SAD = (u32)ln_audio_buffer;
	LN_AUDIO_REG_DMA1DAD = (u32)&REG_FIFO_A;
	LN_AUDIO_REG_DMA1CNT = LN_AUDIO_DMA_DEST_FIXED | LN_AUDIO_DMA_REPEAT |
		LN_AUDIO_DMA_32 | LN_AUDIO_DMA_FIFO | LN_AUDIO_DMA_ENABLE;
	REG_TM0CNT_H = LN_AUDIO_TIMER_ENABLE;

	ln_audio_samples_remaining = clip->length;
	ln_audio_cycle_remainder = 0u;
	ln_audio_playing = true;
	return true;
}

void ln_audio_update(void)
{
	u32 elapsed_samples;

	if (!ln_audio_playing) {
		return;
	}

	ln_audio_cycle_remainder += LN_AUDIO_FRAME_CYCLES;
	elapsed_samples = ln_audio_cycle_remainder / LN_AUDIO_TIMER_PERIOD;
	ln_audio_cycle_remainder %= LN_AUDIO_TIMER_PERIOD;

	if (elapsed_samples >= ln_audio_samples_remaining) {
		ln_audio_stop();
		return;
	}

	ln_audio_samples_remaining -= elapsed_samples;
}

void ln_audio_stop(void)
{
	REG_TM0CNT_H = 0u;
	LN_AUDIO_REG_DMA1CNT = 0u;
	REG_SOUNDCNT_H = (u16)(LN_AUDIO_DIRECT_A_CONTROL | LN_AUDIO_DIRECT_A_RESET);

	ln_audio_samples_remaining = 0u;
	ln_audio_cycle_remainder = 0u;
	ln_audio_playing = false;
}
