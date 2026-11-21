#include "sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t s_fb[SIMSO_FB_W * SIMSO_FB_H];

int main(int argc, char **argv)
{
	int alg = (argc >= 2) ? atoi(argv[1]) : ALG_RR;
	const char *out_path = (argc >= 3) ? argv[2] : "out_gantt.ppm";

	struct sim s;
	memset(&s, 0, sizeof(s));
	s.alg = (uint32_t)alg; s.quantum = 2; s.overhead = 1; s.disk_cost = 3; s.seed = 42; s.n = 3;
	strcpy(s.tasks[0].name, "P1"); s.tasks[0].arrival = 0; s.tasks[0].burst = 5; s.tasks[0].priority = 2; s.tasks[0].deadline = 8;
	strcpy(s.tasks[1].name, "P2"); s.tasks[1].arrival = 1; s.tasks[1].burst = 4; s.tasks[1].priority = 1; s.tasks[1].deadline = 12;
	strcpy(s.tasks[2].name, "P3"); s.tasks[2].arrival = 3; s.tasks[2].burst = 2; s.tasks[2].priority = 3; s.tasks[2].deadline = 20;

	if (!sim_init(&s)) return 1;
	while (sim_step(&s, NULL)) {}

	render_gantt(s_fb, SIMSO_FB_W, SIMSO_FB_H, &s, s.total_ticks);

	FILE *f = fopen(out_path, "wb");
	if (!f) { perror("fopen"); return 1; }
	fprintf(f, "P6\n%u %u\n255\n", SIMSO_FB_W, SIMSO_FB_H);
	for (uint32_t i = 0; i < SIMSO_FB_W * SIMSO_FB_H; i++) {
		uint32_t p = s_fb[i];
		uint8_t rgb[3] = { (uint8_t)(p >> 16), (uint8_t)(p >> 8), (uint8_t)p };
		fwrite(rgb, 1, 3, f);
	}
	fclose(f);
	printf("Wrote %s (%ux%u, %u ticks)\n", out_path, SIMSO_FB_W, SIMSO_FB_H, s.total_ticks);
	return 0;
}
