#include "syscall.h"

#define W 144U
#define H 72U
#define N_BARS 24U

static uint32_t fb[W * H];
static uint32_t arr[N_BARS];
static uint32_t rng = 0x12345678U;

static uint32_t next_rng(void)
{
	rng = rng * 1664525U + 1013904223U;
	return rng;
}

static void shuffle(void)
{
	for (uint32_t i = 0; i < N_BARS; i++)
		arr[i] = 4U + (i * (H - 8U)) / N_BARS;
	for (uint32_t i = N_BARS - 1; i > 0; i--) {
		uint32_t j = next_rng() % (i + 1U);
		uint32_t tmp = arr[i];
		arr[i] = arr[j];
		arr[j] = tmp;
	}
}

static int draw(uint32_t active_j, uint32_t sorted_from)
{
	for (uint32_t i = 0; i < W * H; i++)
		fb[i] = 0x0010161EU;

	uint32_t bw = W / N_BARS;
	for (uint32_t i = 0; i < N_BARS; i++) {
		uint32_t bh = arr[i];
		uint32_t col = 0x003498DBU; /* Blue */
		if (i == active_j || i == active_j + 1U)
			col = 0x00F1C40FU; /* Yellow: comparing/swapping */
		else if (i >= sorted_from)
			col = 0x002ECC71U; /* Green: sorted */

		uint32_t x0 = i * bw + 1U;
		uint32_t x1 = (i + 1U) * bw - 1U;
		uint32_t y0 = (bh < H) ? (H - bh) : 0U;
		for (uint32_t y = y0; y < H; y++) {
			for (uint32_t x = x0; x < x1 && x < W; x++)
				fb[y * W + x] = col;
		}
	}
	if (sys_win_blit(fb, W, H) < 0) {
		sys_fb_blit(fb, W, H);
		int32_t sc = sys_poll_scancode();
		if (sc == 0x01 || sc == 0x10)
			return 1;
	}
	return 0;
}

int app_main(void)
{
	shuffle();
	uint32_t end = N_BARS - 1U;
	uint32_t j = 0;

	for (;;) {
		if (arr[j] > arr[j + 1U]) {
			uint32_t tmp = arr[j];
			arr[j] = arr[j + 1U];
			arr[j + 1U] = tmp;
		}
		if (draw(j, end + 1U))
			return 0;

		j++;
		if (j >= end) {
			j = 0;
			if (end > 1U) {
				end--;
			} else {
				shuffle();
				end = N_BARS - 1U;
			}
		}

		/* Pace animation slightly while burning real CPU cycles in Ring 3. */
		for (volatile uint32_t d = 0; d < 450000U; d++)
			asm volatile("pause");
	}
}
