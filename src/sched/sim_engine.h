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

int64_t sim_engine_start(const struct sim *in_sim, struct proc *owner);
int64_t sim_engine_poll(struct sim *out_sim);
bool sim_engine_is_active(void);
struct proc *sim_engine_tick(void);
void sim_engine_on_proc_exit(struct proc *proc);

#endif // SCHED_SIM_ENGINE_H
