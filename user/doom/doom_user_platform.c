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

static const uint16_t s_pcspk_divisors[96] = {
	0,
	6818, 6628, 6449, 6279, 6087, 5906, 5736, 5575,
	5423, 5279, 5120, 4971, 4830, 4697, 4554, 4435,
	4307, 4186, 4058, 3950, 3836, 3728, 3615, 3519,
	3418, 3323, 3224, 3131, 3043, 2960, 2875, 2794,
	2711, 2633, 2560, 2485, 2415, 2348, 2281, 2213,
	2153, 2089, 2032, 1975, 1918, 1864, 1810, 1757,
	1709, 1659, 1612, 1565, 1521, 1478, 1435, 1395,
	1355, 1316, 1280, 1242, 1207, 1173, 1140, 1107,
	1075, 1045, 1015,  986,  959,  931,  905,  879,
	 854,  829,  806,  783,  760,  739,  718,  697,
	 677,  658,  640,  621,  604,  586,  570,  553,
	 538,  522,  507,  493,  479,  465,  452
};

static const uint8_t *s_sfx_samples = NULL;
static uint32_t s_sfx_len = 0;
static uint32_t s_sfx_start_ms = 0;
static int s_sfx_prio = -1;
static uint16_t s_last_div = 0xFFFF;

static void pcspk_set(uint16_t div)
{
	if (div != s_last_div) {
		sys_pcspk(div);
		s_last_div = div;
	}
}

static bool pcsound_init(bool use_sfx_prefix)
{
	(void)use_sfx_prefix;
	s_sfx_samples = NULL;
	s_sfx_len = 0;
	s_sfx_prio = -1;
	pcspk_set(0);
	return true;
}

static void pcsound_shutdown(void)
{
	s_sfx_samples = NULL;
	pcspk_set(0);
}

static int pcsound_get_lump(sfxinfo_t *sfx)
{
	if (!sfx)
		return -1;
	if (sfx->link)
		sfx = sfx->link;
	char namebuf[16];
	snprintf(namebuf, sizeof(namebuf), "dp%s", sfx->name);
	return W_CheckNumForName(namebuf);
}

static void pcsound_update(void)
{
	if (!s_sfx_samples)
		return;
	uint32_t elapsed_ms = sys_ticks_ms() - s_sfx_start_ms;
	uint32_t pos = (elapsed_ms * 140U) / 1000U;
	if (pos >= s_sfx_len) {
		s_sfx_samples = NULL;
		s_sfx_prio = -1;
		pcspk_set(0);
		return;
	}
	uint8_t tone = s_sfx_samples[pos];
	pcspk_set((tone < 96) ? s_pcspk_divisors[tone] : 0);
}

static void pcsound_update_params(int ch, int vol, int sep)
{
	(void)ch; (void)vol; (void)sep;
}

static int pcsound_start(sfxinfo_t *sfxinfo, int ch, int vol, int sep)
{
	(void)vol; (void)sep;
	if (!sfxinfo)
		return -1;
	if (s_sfx_samples && sfxinfo->priority > s_sfx_prio)
		return -1;

	int lumpnum = sfxinfo->lumpnum;
	if (lumpnum < 0) {
		lumpnum = pcsound_get_lump(sfxinfo);
		if (lumpnum < 0)
			return -1;
		sfxinfo->lumpnum = lumpnum;
	}
	int lumplen = W_LumpLength(lumpnum);
	if (lumplen <= 4)
		return -1;

	const uint8_t *data = (const uint8_t *)W_CacheLumpNum(lumpnum, PU_STATIC);
	if (!data)
		return -1;

	uint16_t num_samples = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
	if ((int)num_samples + 4 > lumplen)
		num_samples = (uint16_t)(lumplen - 4);

	s_sfx_samples = data + 4;
	s_sfx_len = num_samples;
	s_sfx_start_ms = sys_ticks_ms();
	s_sfx_prio = sfxinfo->priority;
	return ch;
}

static void pcsound_stop(int ch)
{
	(void)ch;
	s_sfx_samples = NULL;
	s_sfx_prio = -1;
	pcspk_set(0);
}

static bool pcsound_is_playing(int ch)
{
	(void)ch;
	return s_sfx_samples != NULL;
}

static void pcsound_cache(sfxinfo_t *sounds, int num)
{
	(void)sounds; (void)num;
}

static snddevice_t s_pcsound_devs[] = { SNDDEVICE_PCSPEAKER };
sound_module_t sound_pcsound_module = {
	s_pcsound_devs,
	1,
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
	pcspk_set(0);
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
