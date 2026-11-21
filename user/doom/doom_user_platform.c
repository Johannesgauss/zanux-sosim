#include "doomtype.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#include "i_sound.h"
#include "w_wad.h"
#include "z_zone.h"
#include "syscall.h"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

extern int doom_entry_main(int argc, char **argv);

static bool s_e0_prefix = false;

static unsigned char scancode_to_doomkey(uint8_t sc, bool e0)
{
	if (e0) {
		switch (sc) {
		case 0x48: return KEY_UPARROW;
		case 0x50: return KEY_DOWNARROW;
		case 0x4B: return KEY_LEFTARROW;
		case 0x4D: return KEY_RIGHTARROW;
		case 0x1D: return KEY_FIRE;
		case 0x38: return KEY_STRAFE_L;
		default:   return 0;
		}
	}

	switch (sc) {
	case 0x01: return KEY_ESCAPE;
	case 0x1C: return KEY_ENTER;
	case 0x0F: return KEY_TAB;
	case 0x0E: return KEY_BACKSPACE;
	case 0x39: return KEY_USE;
	case 0x1D: return KEY_FIRE;
	case 0x2A:
	case 0x36: return KEY_RSHIFT;
	case 0x11: return KEY_UPARROW;
	case 0x1F: return KEY_DOWNARROW;
	case 0x1E: return KEY_STRAFE_L;
	case 0x20: return KEY_STRAFE_R;
	case 0x12: return KEY_USE;
	case 0x15: return 'y';
	case 0x31: return 'n';
	case 0x02: return '1';
	case 0x03: return '2';
	case 0x04: return '3';
	case 0x05: return '4';
	case 0x06: return '5';
	case 0x07: return '6';
	case 0x08: return '7';
	case 0x09: return '8';
	case 0x0A: return '9';
	case 0x0B: return '0';
	case 0x0C: return KEY_MINUS;
	case 0x0D: return KEY_EQUALS;
	case 0x3B: return KEY_F1;
	case 0x3C: return KEY_F2;
	case 0x3D: return KEY_F3;
	case 0x3E: return KEY_F4;
	case 0x3F: return KEY_F5;
	case 0x40: return KEY_F6;
	case 0x41: return KEY_F7;
	case 0x42: return KEY_F8;
	case 0x43: return KEY_F9;
	case 0x44: return KEY_F10;
	case 0x57: return KEY_F11;
	case 0x58: return KEY_F12;
	default:   return 0;
	}
}

void DG_Init(void)
{
}

void DG_DrawFrame(void)
{
	if (DG_ScreenBuffer)
		sys_fb_blit(DG_ScreenBuffer, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
}

void DG_SleepMs(uint32_t ms)
{
	uint32_t start = sys_ticks_ms();
	while ((uint32_t)(sys_ticks_ms() - start) < ms) {
		asm volatile("pause");
	}
}

uint32_t DG_GetTicksMs(void)
{
	return sys_ticks_ms();
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
	for (;;) {
		int32_t raw = sys_poll_scancode();
		if (raw < 0)
			return 0;

		uint8_t sc = (uint8_t)raw;
		if (sc == 0xE0) {
			s_e0_prefix = true;
			continue;
		}
		if (sc == 0xE1)
			continue;

		bool is_release = (sc & 0x80) != 0;
		uint8_t code = sc & 0x7F;
		bool e0 = s_e0_prefix;
		s_e0_prefix = false;

		unsigned char key = scancode_to_doomkey(code, e0);
		if (key != 0) {
			*pressed = is_release ? 0 : 1;
			*doomKey = key;
			return 1;
		}
	}
}

void DG_SetWindowTitle(const char *title)
{
	(void)title;
}

void DG_ReadInput(void)
{
}

void doom_panic_screen(const char *msg)
{
	if (msg) {
		size_t n = 0;
		while (msg[n])
			n++;
		sys_write(2, msg, n);
		sys_write(2, "\n", 1);
	}
	sys_exit(1);
}

/*
 * 8-channel digital PCM mixer for Doom's native `ds*` 8-bit unsigned SFX lumps,
 * output to Sound Blaster 16 via `sys_audio_write()` at 11025 Hz.
 * Respects Doom's menu SFX Volume slider (`vol` in 0..127).
 */
#define SFX_NUM_CHANNELS  8
#define SFX_MIX_CHUNK     512
#define SFX_OUT_RATE      11025U

struct sfx_channel {
	const uint8_t *samples;
	uint32_t len;
	uint32_t pos_fp;   /* 16.16 fixed-point sample position */
	uint32_t step_fp;  /* 16.16 fixed-point step (sample_rate / 11025) */
	int vol;           /* 0..127 from Doom's S_StartSound / S_UpdateSounds */
	bool active;
};

static struct sfx_channel s_ch[SFX_NUM_CHANNELS];
static uint8_t s_mix_buf[SFX_MIX_CHUNK];
static uint32_t s_mix_pending = 0;
static uint32_t s_mix_off = 0;

static void mix_and_feed_sb16(void)
{
	/* Try to drain any leftover bytes from the previous chunk first. */
	if (s_mix_pending > 0) {
		int64_t w = sys_audio_write(s_mix_buf + s_mix_off, s_mix_pending);
		if (w <= 0)
			return;
		s_mix_off += (uint32_t)w;
		s_mix_pending -= (uint32_t)w;
		if (s_mix_pending > 0)
			return;
	}

	/* Generate and push up to 4 chunks per update (~180 ms lookahead max). */
	for (int iter = 0; iter < 4; iter++) {
		for (uint32_t i = 0; i < SFX_MIX_CHUNK; i++) {
			int32_t acc = 0;
			for (int c = 0; c < SFX_NUM_CHANNELS; c++) {
				if (!s_ch[c].active)
					continue;
				uint32_t idx = s_ch[c].pos_fp >> 16;
				if (idx >= s_ch[c].len) {
					s_ch[c].active = false;
					continue;
				}
				int32_t s = (int32_t)s_ch[c].samples[idx] - 128;
				/* Boosted linear volume (vol 0..127 -> ~1.5x gain at max, 0 = mute). */
				acc += (s * s_ch[c].vol * 3) >> 8;
				s_ch[c].pos_fp += s_ch[c].step_fp;
			}
			acc += 128;
			if (acc < 0) acc = 0;
			if (acc > 255) acc = 255;
			s_mix_buf[i] = (uint8_t)acc;
		}

		int64_t w = sys_audio_write(s_mix_buf, SFX_MIX_CHUNK);
		if (w <= 0) {
			s_mix_off = 0;
			s_mix_pending = SFX_MIX_CHUNK;
			return;
		}
		if ((uint32_t)w < SFX_MIX_CHUNK) {
			s_mix_off = (uint32_t)w;
			s_mix_pending = SFX_MIX_CHUNK - (uint32_t)w;
			return;
		}
	}
}

static bool pcsound_init(bool use_sfx_prefix)
{
	(void)use_sfx_prefix;
	for (int i = 0; i < SFX_NUM_CHANNELS; i++)
		s_ch[i].active = false;
	s_mix_pending = 0;
	s_mix_off = 0;
	sys_pcspk(0);
	sys_audio_write(NULL, 0);
	return true;
}

static void pcsound_shutdown(void)
{
	for (int i = 0; i < SFX_NUM_CHANNELS; i++)
		s_ch[i].active = false;
	sys_audio_write(NULL, 0);
	sys_pcspk(0);
}

static int pcsound_get_lump(sfxinfo_t *sfx)
{
	if (!sfx)
		return -1;
	if (sfx->link)
		sfx = sfx->link;
	char namebuf[16];
	snprintf(namebuf, sizeof(namebuf), "ds%s", sfx->name);
	return W_CheckNumForName(namebuf);
}

static void pcsound_update(void)
{
	mix_and_feed_sb16();
}

static void pcsound_update_params(int ch, int vol, int sep)
{
	(void)sep;
	if (ch < 0 || ch >= SFX_NUM_CHANNELS)
		return;
	if (vol < 0) vol = 0;
	if (vol > 127) vol = 127;
	s_ch[ch].vol = vol;
}

static int pcsound_start(sfxinfo_t *sfxinfo, int ch, int vol, int sep)
{
	(void)sep;
	if (!sfxinfo || ch < 0 || ch >= SFX_NUM_CHANNELS)
		return -1;

	int lumpnum = sfxinfo->lumpnum;
	if (lumpnum < 0) {
		lumpnum = pcsound_get_lump(sfxinfo);
		if (lumpnum < 0)
			return -1;
		sfxinfo->lumpnum = lumpnum;
	}
	int lumplen = W_LumpLength(lumpnum);
	if (lumplen <= 8)
		return -1;

	const uint8_t *data = (const uint8_t *)W_CacheLumpNum(lumpnum, PU_STATIC);
	if (!data)
		return -1;

	/* Doom DMX digital SFX header: [0..1]=format(3), [2..3]=rate, [4..7]=num_samples. */
	uint16_t rate = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
	uint32_t num_samples = (uint32_t)data[4]
	                     | ((uint32_t)data[5] << 8)
	                     | ((uint32_t)data[6] << 16)
	                     | ((uint32_t)data[7] << 24);
	if (rate == 0)
		rate = SFX_OUT_RATE;

	/* Skip 8-byte header + 16-byte DMX leading pad if lump is large enough. */
	uint32_t data_off = 8;
	if (num_samples > 32 && (int)(8 + num_samples) <= lumplen) {
		data_off = 24;
		num_samples -= 32;
	} else if ((int)(8 + num_samples) > lumplen) {
		num_samples = (uint32_t)(lumplen - 8);
	}

	if (vol < 0) vol = 0;
	if (vol > 127) vol = 127;

	s_ch[ch].samples = data + data_off;
	s_ch[ch].len     = num_samples;
	s_ch[ch].pos_fp  = 0;
	s_ch[ch].step_fp = ((uint32_t)rate << 16) / SFX_OUT_RATE;
	s_ch[ch].vol     = vol;
	s_ch[ch].active  = (num_samples > 0);
	return ch;
}

static void pcsound_stop(int ch)
{
	if (ch >= 0 && ch < SFX_NUM_CHANNELS)
		s_ch[ch].active = false;
}

static bool pcsound_is_playing(int ch)
{
	if (ch < 0 || ch >= SFX_NUM_CHANNELS)
		return false;
	return s_ch[ch].active;
}

static void pcsound_cache(sfxinfo_t *sounds, int num)
{
	(void)sounds; (void)num;
}

static snddevice_t s_pcsound_devs[] = { SNDDEVICE_PCSPEAKER, SNDDEVICE_SB };
sound_module_t sound_pcsound_module = {
	s_pcsound_devs,
	2,
	pcsound_init,
	pcsound_shutdown,
	pcsound_get_lump,
	pcsound_update,
	pcsound_update_params,
	pcsound_start,
	pcsound_stop,
	pcsound_is_playing,
	pcsound_cache,
};

void exit(int status)
{
	sys_audio_write(NULL, 0);
	sys_pcspk(0);
	sys_exit(status);
}

int doom_user_main(void)
{
	static char arg0[] = "doom";
	static char arg1[] = "-iwad";
	static char arg2[] = "doom1.wad";
	static char *argv[] = { arg0, arg1, arg2, NULL };
	return doom_entry_main(3, argv);
}
