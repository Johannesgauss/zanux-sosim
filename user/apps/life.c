#include "syscall.h"

#define W 144U
#define H 72U
#define GW 36U
#define GH 18U

static uint32_t fb[W * H];
static uint8_t grid[GH][GW];
static uint8_t next_grid[GH][GW];
static uint32_t rng = 0xCAFEBABEU;

static uint32_t next_rng(void)
{
	rng = rng * 1664525U + 1013904223U;
	return rng;
}

static void seed_grid(void)
{
	for (uint32_t y = 0; y < GH; y++) {
		for (uint32_t x = 0; x < GW; x++)
			grid[y][x] = (uint8_t)((next_rng() >> 28) < 5U ? 1U : 0U);
	}
}

static void step_life(void)
{
	uint32_t alive = 0;
	for (uint32_t y = 0; y < GH; y++) {
		for (uint32_t x = 0; x < GW; x++) {
			uint32_t nb = 0;
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					if (dx == 0 && dy == 0) continue;
					uint32_t nx = (x + (uint32_t)(dx + (int)GW)) % GW;
					uint32_t ny = (y + (uint32_t)(dy + (int)GH)) % GH;
					nb += grid[ny][nx];
				}
			}
			uint8_t c = grid[y][x];
			uint8_t nc = (c && (nb == 2U || nb == 3U)) || (!c && nb == 3U) ? 1U : 0U;
			next_grid[y][x] = nc;
			alive += nc;
		}
	}
	for (uint32_t y = 0; y < GH; y++)
		for (uint32_t x = 0; x < GW; x++)
			grid[y][x] = next_grid[y][x];

	if (alive < 12U)
		seed_grid();
}

static int draw(uint32_t gen)
{
	uint32_t cell_col = ((gen & 1U) == 0U) ? 0x002ECC71U : 0x0000E5FFU;
	for (uint32_t gy = 0; gy < GH; gy++) {
		for (uint32_t gx = 0; gx < GW; gx++) {
			uint32_t col = grid[gy][gx] ? cell_col : 0x0010161EU;
			for (uint32_t py = 0; py < 4U; py++) {
				uint32_t *row = fb + (gy * 4U + py) * W + gx * 4U;
				for (uint32_t px = 0; px < 4U; px++)
					row[px] = (px == 3U || py == 3U) ? 0x00161E28U : col;
			}
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
	seed_grid();
	uint32_t gen = 0;

	for (;;) {
		if (draw(gen++))
			return 0;
		step_life();
		if ((gen % 60U) == 0U)
			seed_grid();

		for (volatile uint32_t d = 0; d < 650000U; d++)
			asm volatile("pause");
	}
}
