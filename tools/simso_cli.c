#define _POSIX_C_SOURCE 200809L
#include "sim.h"
#include "simso_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *s_alg_names[ALG_COUNT] = {
	"FCFS", "SJF", "RR", "PRIO", "EDF", "CFS", "AUTORAL"
};

static uint32_t s_fb[SIMSO_FB_W * SIMSO_FB_H];

static int streq_ci(const char *a, const char *b)
{
	while (*a && *b) {
		if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
			return 0;
		a++;
		b++;
	}
	return *a == '\0' && *b == '\0';
}

static int parse_alg(const char *s)
{
	if (!s || !s[0]) return -1;
	if (s[0] >= '0' && s[0] <= '6' && s[1] == '\0')
		return s[0] - '0';
	if (streq_ci(s, "FCFS") || streq_ci(s, "FIFO")) return ALG_FCFS;
	if (streq_ci(s, "SJF")) return ALG_SJF;
	if (streq_ci(s, "RR") || streq_ci(s, "ROUND-ROBIN")) return ALG_RR;
	if (streq_ci(s, "PRIO") || streq_ci(s, "PRIORIDADE") || streq_ci(s, "PRIORIDADES")) return ALG_PRIO;
	if (streq_ci(s, "EDF")) return ALG_EDF;
	if (streq_ci(s, "CFS") || streq_ci(s, "CFS-SIM")) return ALG_CFS;
	if (streq_ci(s, "AUTORAL") || streq_ci(s, "IA")) return ALG_AUTORAL;
	return -1;
}

static void sleep_ms(uint32_t ms)
{
	if (ms == 0) return;
	struct timespec ts;
	ts.tv_sec = (time_t)(ms / 1000U);
	ts.tv_nsec = (long)((ms % 1000U) * 1000000U);
	nanosleep(&ts, NULL);
}

static void format_starts(const struct sim *s, uint32_t idx, char *buf, size_t cap)
{
	size_t pos = 0;
	buf[0] = '\0';
	uint32_t lim = (s->total_ticks < SIM_MAX_TICKS) ? s->total_ticks : SIM_MAX_TICKS;
	for (uint32_t t = 0; t < lim; t++) {
		if ((s->tl_kind[t] == TICK_EXEC || s->tl_kind[t] == TICK_MISS) &&
		    s->tl_task[t] == (int8_t)idx) {
			int prev_same = (t > 0 &&
			                 (s->tl_kind[t - 1] == TICK_EXEC || s->tl_kind[t - 1] == TICK_MISS) &&
			                 s->tl_task[t - 1] == (int8_t)idx);
			if (!prev_same) {
				int w = snprintf(buf + pos, cap - pos, "%s%u", (pos > 0) ? "," : "", t);
				if (w > 0 && pos + (size_t)w < cap)
					pos += (size_t)w;
			}
		}
	}
	if (pos == 0)
		snprintf(buf, cap, "-");
}

static void write_report(FILE *out, const struct sim *s)
{
	fprintf(out, "=== SIMSO | Algoritmo: %s | quantum=%u | sobrecarga=%u | seed=%u ===\n",
	        s_alg_names[s->alg], s->quantum, s->overhead, s->seed);
	fprintf(out, "Gantt: ");
	uint32_t lim = (s->total_ticks < SIM_MAX_TICKS) ? s->total_ticks : SIM_MAX_TICKS;
	for (uint32_t t = 0; t < lim; t++) {
		if (s->tl_kind[t] == TICK_OVERHEAD) fprintf(out, "[OV]");
		else if (s->tl_kind[t] == TICK_IDLE) fprintf(out, "[..]");
		else if (s->tl_kind[t] == TICK_IO) fprintf(out, "[IO]");
		else if (s->tl_kind[t] == TICK_MISS) fprintf(out, "[%s!]", s->tasks[s->tl_task[t]].name);
		else fprintf(out, "[%s]", s->tasks[s->tl_task[t]].name);
	}
	fprintf(out, "\n\n%-5s %6s %5s %5s %5s %-14s %6s %6s %6s %6s\n",
	        "ID", "Cheg", "Exec", "Dead", "Prio", "Inicio(s)", "Term", "Esp", "Turn", "OK?");

	double sw = 0.0, st = 0.0;
	for (uint32_t i = 0; i < s->n; i++) {
		const struct task *r = &s->tasks[i];
		char starts[48];
		format_starts(s, i, starts, sizeof(starts));
		sw += r->wait;
		st += r->turnaround;
		fprintf(out, "%-5s %6u %5u %5u %5u %-14s %6u %6u %6u %6s\n",
		        r->name, r->arrival, r->burst, r->deadline, r->priority,
		        starts, r->finish, r->wait, r->turnaround, r->ok ? "SIM" : "NAO");
	}
	double total = (s->total_ticks > 0) ? (double)s->total_ticks : 1.0;
	fprintf(out, "\nResumo Quantitativo:\n");
	fprintf(out, "  Media Espera:       %.2f\n", sw / s->n);
	fprintf(out, "  Media Turnaround:   %.2f\n", st / s->n);
	fprintf(out, "  Throughput:         %.3f proc/u.t.\n", (double)s->n / total);
	fprintf(out, "  CPU Ociosa (+OV):   %.1f%% (idle=%u, ov=%u, total=%u)\n",
	        100.0 * (s->idle_ticks + s->overhead_ticks) / total,
	        s->idle_ticks, s->overhead_ticks, s->total_ticks);
	fprintf(out, "  Trocas de Contexto: %u\n", s->ctx_switches);
	fprintf(out, "  Preempcoes:         %u\n", s->total_preemptions);
}

static int save_ppm(const char *path, const uint32_t *fb, uint32_t w, uint32_t h)
{
	FILE *f = fopen(path, "wb");
	if (!f) {
		perror(path);
		return -1;
	}
	fprintf(f, "P6\n%u %u\n255\n", w, h);
	for (uint32_t i = 0; i < w * h; i++) {
		uint32_t p = fb[i];
		uint8_t rgb[3] = { (uint8_t)(p >> 16), (uint8_t)(p >> 8), (uint8_t)p };
		fwrite(rgb, 1, 3, f);
	}
	fclose(f);
	return 0;
}

static void print_usage(const char *prog)
{
	fprintf(stderr,
	        "Uso: %s --alg <FCFS|SJF|RR|PRIO|EDF|CFS|AUTORAL> --input <caso.json>\n"
	        "          [--gantt <out.ppm>] [--log <out.log>] [--seed <N>] [--delay <ms>]\n"
	        "          [--quantum <N>] [--overhead <N>]\n",
	        prog);
}

int main(int argc, char **argv)
{
	int alg = ALG_RR;
	const char *input_path = NULL;
	const char *gantt_path = NULL;
	const char *log_path = NULL;
	int has_seed = 0, has_quantum = 0, has_overhead = 0;
	uint32_t seed_override = 42, quantum_override = 2, overhead_override = 1;
	uint32_t delay_ms = 0;

	for (int i = 1; i < argc; i++) {
		if ((strcmp(argv[i], "--alg") == 0 || strcmp(argv[i], "-a") == 0) && i + 1 < argc) {
			alg = parse_alg(argv[++i]);
			if (alg < 0) {
				fprintf(stderr, "Algoritmo invalido: %s\n", argv[i]);
				return 1;
			}
		} else if ((strcmp(argv[i], "--input") == 0 || strcmp(argv[i], "-i") == 0) && i + 1 < argc) {
			input_path = argv[++i];
		} else if (strcmp(argv[i], "--gantt") == 0 && i + 1 < argc) {
			gantt_path = argv[++i];
		} else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
			log_path = argv[++i];
		} else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
			seed_override = (uint32_t)strtoul(argv[++i], NULL, 10);
			has_seed = 1;
		} else if (strcmp(argv[i], "--delay") == 0 && i + 1 < argc) {
			delay_ms = (uint32_t)strtoul(argv[++i], NULL, 10);
		} else if (strcmp(argv[i], "--quantum") == 0 && i + 1 < argc) {
			quantum_override = (uint32_t)strtoul(argv[++i], NULL, 10);
			has_quantum = 1;
		} else if ((strcmp(argv[i], "--overhead") == 0 || strcmp(argv[i], "--sobrecarga") == 0) && i + 1 < argc) {
			overhead_override = (uint32_t)strtoul(argv[++i], NULL, 10);
			has_overhead = 1;
		} else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			print_usage(argv[0]);
			return 0;
		} else {
			fprintf(stderr, "Opcao desconhecida: %s\n", argv[i]);
			print_usage(argv[0]);
			return 1;
		}
	}

	if (!input_path) {
		fprintf(stderr, "Erro: informe --input <arquivo.json>\n");
		print_usage(argv[0]);
		return 1;
	}

	FILE *in = fopen(input_path, "rb");
	if (!in) {
		perror(input_path);
		return 1;
	}
	fseek(in, 0, SEEK_END);
	long fsz = ftell(in);
	fseek(in, 0, SEEK_SET);
	if (fsz <= 0 || fsz > 1024 * 1024) {
		fprintf(stderr, "Arquivo JSON vazio ou grande demais.\n");
		fclose(in);
		return 1;
	}
	char *buf = (char *)malloc((size_t)fsz + 1);
	if (!buf) {
		fclose(in);
		return 1;
	}
	size_t nread = fread(buf, 1, (size_t)fsz, in);
	fclose(in);
	buf[nread] = '\0';

	struct sim s;
	memset(&s, 0, sizeof(s));
	s.alg = (uint32_t)alg;
	if (!simso_parse_json(buf, nread, &s)) {
		fprintf(stderr, "Falha ao interpretar JSON: %s\n", input_path);
		free(buf);
		return 1;
	}
	free(buf);

	if (has_seed) s.seed = seed_override;
	if (has_quantum) s.quantum = quantum_override;
	if (has_overhead) s.overhead = overhead_override;

	if (!sim_init(&s)) {
		fprintf(stderr, "Configuracao invalida para sim_init().\n");
		return 1;
	}

	if (delay_ms > 0) {
		printf("Executando %s passo-a-passo (delay=%ums):\n", s_alg_names[s.alg], delay_ms);
		while (sim_step(&s, NULL)) {
			uint32_t t = s.now - 1;
			if (t < SIM_MAX_TICKS) {
				if (s.tl_kind[t] == TICK_OVERHEAD) printf("t=%2u: [OV]\n", t);
				else if (s.tl_kind[t] == TICK_IDLE) printf("t=%2u: [IDLE]\n", t);
				else if (s.tl_kind[t] == TICK_IO) printf("t=%2u: [IO]\n", t);
				else if (s.tl_kind[t] == TICK_MISS) printf("t=%2u: [%s! (ESTOURO)]\n", t, s.tasks[s.tl_task[t]].name);
				else printf("t=%2u: [%s]\n", t, s.tasks[s.tl_task[t]].name);
				fflush(stdout);
			}
			sleep_ms(delay_ms);
		}
	} else {
		while (sim_step(&s, NULL)) {}
	}

	write_report(stdout, &s);

	if (log_path) {
		FILE *lf = fopen(log_path, "w");
		if (!lf) {
			perror(log_path);
			return 1;
		}
		write_report(lf, &s);
		fclose(lf);
		printf("Log salvo em %s\n", log_path);
	}

	if (gantt_path) {
		render_gantt(s_fb, SIMSO_FB_W, SIMSO_FB_H, &s, s.total_ticks);
		if (save_ppm(gantt_path, s_fb, SIMSO_FB_W, SIMSO_FB_H) != 0)
			return 1;
		printf("Gantt salvo em %s (%ux%u)\n", gantt_path, SIMSO_FB_W, SIMSO_FB_H);
	}

	return 0;
}
