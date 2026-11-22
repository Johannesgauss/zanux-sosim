#include "syscall.h"

#define W 144U
#define H 72U

static uint32_t fb[W * H];

/* Compact 64-entry integer sine LUT (0..255). */
static const uint8_t sin_lut[64] = {
	128,140,152,165,176,188,198,208,218,226,234,240,245,250,253,254,
	255,254,253,250,245,240,234,226,218,208,198,188,176,165,152,140,
	128,115,103, 90, 79, 67, 57, 47, 37, 29, 21, 15, 10,  5,  2,  1,
	  0,  1,  2,  5, 10, 15, 21, 29, 37, 47, 57, 67, 79, 90,103,115
};

int app_main(void)
{
	uint32_t t = 0;
	for (;;) {
		for (uint32_t y = 0; y < H; y++) {
			uint32_t *row = fb + y * W;
			uint8_t sy = sin_lut[(y * 2U + t) & 63U];
			for (uint32_t x = 0; x < W; x++) {
				uint8_t sx = sin_lut[(x + t * 2U) & 63U];
				uint8_t sd = sin_lut[((x + y) + t) & 63U];
				uint32_t v = ((uint32_t)sx + (uint32_t)sy + (uint32_t)sd) / 3U;
				uint32_t r = sin_lut[(v + t) & 63U];
				uint32_t g = sin_lut[(v + 21U) & 63U];
				uint32_t b = sin_lut[(v + 42U) & 63U];
				row[x] = (r << 16) | (g << 8) | b;
			}
		}
		if (sys_win_blit(fb, W, H) < 0) {
			sys_fb_blit(fb, W, H);
			int32_t sc = sys_poll_scancode();
			if (sc == 0x01 || sc == 0x10)
				return 0;
		}
		t++;

		for (volatile uint32_t d = 0; d < 250000U; d++)
			asm volatile("pause");
	}
}
