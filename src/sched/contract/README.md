# Frozen Contract (`src/sched/contract/sim.h`)

**One file. Two structs (`struct task`, `struct sim`).**

Do **not** edit `sim.h` without team agreement. Everything **outside** `src/sched/contract/` can be freely rewritten from scratch as long as it compiles against `sim.h`:

- **C/C++ teammates:** implement `pick_fcfs` .. `pick_autoral` in `src/sched/policies/*.c` (and optionally rewrite `src/sched/sim_core.c`). Test with `make -C tools test-sched`.
- **Web Dev teammate:** implement `render_gantt()` in `user/simso/simso_render.c`. Test with `make -C tools test-ui`.
