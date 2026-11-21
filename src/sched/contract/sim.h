/**
 * @file sim.h
 * @brief Single frozen contract shared by Zanux kernel, scheduling policies, host tests, and Ring 3 UI.
 */

#ifndef SCHED_CONTRACT_SIM_H
#define SCHED_CONTRACT_SIM_H

#include <stdbool.h>
#include <stdint.h>

/* Simulation limits & fixed-point scaling (kernel has no FPU/SSE). */
#define SIM_MAX_TASKS 64     /* Max processes per run (matches Zanux SCHED_MAX_PROCS = 64). */
#define SIM_MAX_TICKS 512    /* Max ticks recorded in the Gantt timeline (tl_kind/tl_task). */
#define CFS_SCALE     1024U  /* Fixed-point scale for CFS vruntime (1.0 == 1024, like Linux NICE_0_LOAD). */

/* Logical canvas size passed to render_gantt() and sys_fb_blit() (scales 2x onto 1280x800). */
#define SIMSO_FB_W    640U
#define SIMSO_FB_H    400U

/* 24-bit RGB hex colors (0x00RRGGBB) for uint32_t framebuffer pixels. */
#define COL_BG        0x0012161CU /* #12161C — Dark background */
#define COL_PANEL     0x001B222CU /* #1B222C — Card / panel background */
#define COL_GRID      0x002B3544U /* #2B3544 — Grid lines & inactive tabs */
#define COL_EXEC      0x002ECC71U /* #2ECC71 — Green: normal CPU execution */
#define COL_OVERHEAD  0x00E74C3CU /* #E74C3C — Red: context-switch overhead */
#define COL_MISS      0x0088929EU /* #88929E — Gray: execution past absolute deadline */
#define COL_DEADLINE  0x00F1C40FU /* #F1C40F — Yellow: vertical absolute deadline marker */
#define COL_IO        0x0000BCD4U /* #00BCD4 — Cyan: page fault / disk I/O stall */
#define COL_IDLE      0x00242B35U /* #242B35 — Dark slate: CPU idle */
#define COL_TEXT      0x00FFFFFFU /* #FFFFFF — White: text labels & step-by-step cursor */

/* The 7 required scheduling algorithms. */
enum alg_id {
	ALG_FCFS    = 0, /* First Come, First Served (non-preemptive) */
	ALG_SJF     = 1, /* Shortest Job First (non-preemptive) */
	ALG_RR      = 2, /* Round-Robin with fixed quantum (preemptive) */
	ALG_PRIO    = 3, /* Static Priority, 1 = highest (preemptive) */
	ALG_EDF     = 4, /* Earliest Deadline First (preemptive) */
	ALG_CFS     = 5, /* Completely Fair Scheduler simplified (preemptive) */
	ALG_AUTORAL = 6, /* Team's custom / AI-inspired algorithm */
	ALG_COUNT   = 7
};

/* What happened on the single-core CPU during tick `t` (controls Gantt bar color). */
enum tick_kind {
	TICK_IDLE     = 0, /* No task ready */
	TICK_EXEC     = 1, /* Task running within deadline (green) */
	TICK_OVERHEAD = 2, /* Context-switch overhead (red) */
	TICK_MISS     = 3, /* Task running past deadline (gray) */
	TICK_IO       = 4  /* Page fault / disk I/O wait (cyan) */
};

/* Lifecycle state of each individual task. */
enum task_st {
	TASK_PENDING = 0, /* Not yet arrived (now < arrival) */
	TASK_READY   = 1, /* Eligible to run */
	TASK_RUNNING = 2, /* Currently on the CPU */
	TASK_BLOCKED = 3, /* Waiting on disk / page fault (VM bonus) */
	TASK_DONE    = 4  /* Finished execution */
};

/**
 * @brief Scheduling metadata, runtime state, and final metrics for one process.
 */
struct task {
	char name[16];     /* Display label (e.g. "P1") */
	char bin_name[16]; /* Optional Ring 3 binary on FAT32 (e.g. "SORT.BIN"); "" = WORKER.BIN */

	/* Inputs (configured before sim_init) */
	uint32_t arrival;   /* Arrival tick (chegada) */
	uint32_t burst;     /* Fixed CPU ticks (>0), or 0 = run until Ring 3 calls sys_exit() */
	uint32_t priority;  /* Static priority (1 = highest) */
	uint32_t deadline;  /* Absolute deadline tick */
	uint32_t num_pages; /* Optional VM bonus: virtual pages used */

	/* Runtime state (updated every tick by sim_core / policy) */
	uint32_t remaining;  /* Remaining CPU ticks before completion */
	uint32_t vruntime;   /* CFS virtual runtime (scaled by CFS_SCALE) */
	uint32_t slice_used; /* Consecutive ticks used in current dispatch (for RR quantum) */
	uint32_t state;      /* Current enum task_st */
	uint32_t user_data;  /* Policy-private scratch field (e.g. RR FIFO seq / Autoral score) */

	/* Results (filled on task completion for the final table) */
	uint32_t start;       /* First tick this task ran (inicio) */
	uint32_t finish;      /* Completion tick, exclusive (termino) */
	uint32_t wait;        /* turnaround - burst (espera) */
	uint32_t turnaround;  /* finish - arrival */
	uint32_t preemptions; /* Number of times preempted mid-burst */
	uint32_t page_faults; /* Optional VM bonus: total page faults */
	uint32_t ok;          /* 1 if finish <= deadline, 0 if missed (deadline_ok?) */
};

/**
 * @brief Full simulation state: inputs, Gantt timeline, global metrics, and engine cursors.
 */
struct sim {
	/* Global inputs */
	uint32_t alg;       /* enum alg_id */
	uint32_t quantum;   /* Time slice in ticks (for RR) */
	uint32_t overhead;  /* Context-switch overhead in ticks */
	uint32_t disk_cost; /* Optional VM bonus: disk delay per page fault */
	uint32_t seed;      /* Deterministic PRNG seed */
	uint32_t n;         /* Number of valid tasks in tasks[] (1..SIM_MAX_TASKS) */
	struct task tasks[SIM_MAX_TASKS];

	/* Outputs: Gantt timeline + global summary counters */
	uint8_t  tl_kind[SIM_MAX_TICKS]; /* enum tick_kind at tick t */
	int8_t   tl_task[SIM_MAX_TICKS]; /* Task index (0..n-1) at tick t, or -1 if none */
	uint32_t total_ticks;            /* Total elapsed ticks */
	uint32_t idle_ticks;             /* Ticks with no task ready */
	uint32_t overhead_ticks;         /* Ticks spent in context-switch overhead */
	uint32_t ctx_switches;           /* Total context switches */
	uint32_t total_preemptions;      /* Total preemptions across all tasks */

	/* Internal engine cursors (managed by sim_core.c) */
	uint32_t now, done_count, overhead_left, finished;
	int32_t  cur, last, pending_next;
};

/* --- Scheduling algorithms (src/sched/policies/) ---
 * Return index 0..task_count-1 of the task to run on tick `now`, or -1 if CPU is idle. */
int pick_fcfs   (struct task *tasks, uint32_t task_count, int current_idx, uint32_t now, uint32_t quantum);
int pick_sjf    (struct task *tasks, uint32_t task_count, int current_idx, uint32_t now, uint32_t quantum);
int pick_rr     (struct task *tasks, uint32_t task_count, int current_idx, uint32_t now, uint32_t quantum);
int pick_prio   (struct task *tasks, uint32_t task_count, int current_idx, uint32_t now, uint32_t quantum);
int pick_edf    (struct task *tasks, uint32_t task_count, int current_idx, uint32_t now, uint32_t quantum);
int pick_cfs    (struct task *tasks, uint32_t task_count, int current_idx, uint32_t now, uint32_t quantum);
int pick_autoral(struct task *tasks, uint32_t task_count, int current_idx, uint32_t now, uint32_t quantum);

/* --- Simulation engine (src/sched/sim/sim_core.c) --- */
bool sim_init(struct sim *sim);                               /* Reset runtime state & validate inputs */
bool sim_step(struct sim *sim, int *active_task);             /* Advance 1 discrete tick; false when done */
void sim_notify_exit(struct sim *sim, uint32_t task_idx);     /* Mark real burst==0 task done on sys_exit() */

/* --- Framebuffer UI renderer (user/simso/simso_render.c) --- */
void render_gantt(uint32_t *fb, uint32_t width, uint32_t height, const struct sim *sim, uint32_t visible_ticks);

#endif // SCHED_CONTRACT_SIM_H
