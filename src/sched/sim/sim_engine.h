/**
 * @file sim_engine.h
 * @brief Kernel-side simulation engine driving real Ring 3 worker processes on IRQ0.
 */

#ifndef SCHED_SIM_ENGINE_H
#define SCHED_SIM_ENGINE_H

#include "sched/proc.h"
#include "sim.h"
#include <stdbool.h>
#include <stdint.h>

#define SIM_WIN_SLOTS 4U
#define SIM_WIN_W     144U
#define SIM_WIN_H     72U

int64_t sim_engine_start(const struct sim *in_sim, struct proc *owner);
int64_t sim_engine_poll(struct sim *out_sim);
bool sim_engine_is_active(void);
struct proc *sim_engine_tick(void);
void sim_engine_on_proc_exit(struct proc *proc);

int64_t sim_engine_win_blit(struct proc *caller, const uint32_t *buf, uint32_t w, uint32_t h);
int64_t sim_engine_win_read(uint32_t slot, uint32_t *dst, uint32_t max_pixels);

#endif // SCHED_SIM_ENGINE_H
