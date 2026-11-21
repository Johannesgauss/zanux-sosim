# Teammate Quickstart — Zanux Scheduling Simulator (`feat/simso`)

You **do not** need QEMU, x86_64 assembly, or a cross-compiler to develop and test your part.
The exact same `.c` files compile on your laptop with `gcc` **and** inside the real Zanux kernel!

---

## 1. Scheduling Engine & Policies (C / C++ Teammates)

- **Frozen contract (do not edit without team agreement):** `src/sched/contract/sim_types.h`, `src/sched/contract/sched_policy.h`, `src/sched/contract/sim_core.h`
- **Engine you can own/rewrite:** `src/sched/sim_core.c` (as long as it implements `sim_core_init` + `sim_core_step`, it plugs directly into Zanux's real kernel!)
- **Ready-to-fill policy files:** `src/sched/policies/fcfs.c`, `sjf.c`, `prio.c`, `edf.c`, `cfs.c`, `autoral.c` (plus `rr.c` as a working reference).
- **Rule:** Keep your `.c` files pure freestanding C11 (no `<stdio.h>`, no `malloc`, no floats — use integers / `CFS_VRUNTIME_SCALE`).

**Compile & test locally in 1 second:**
```bash
make -C tools test-sched
```

---

## 2. Native Framebuffer Gantt & Metrics UI (Web Dev Teammate)

- **File you own:** `user/simso/simso_render.c` (frozen signature in `src/sched/contract/simso_render.h`)
- Ready-to-use helpers already included: `fill_rect()`, `draw_vline()`, `draw_text()`, `draw_uint()` (using `font8x8.h`).
- Colors defined in `simso_render.h`:
  - `SIMSO_COL_EXEC` (green), `SIMSO_COL_OVERHEAD` (red), `SIMSO_COL_MISS` (gray), `SIMSO_COL_DEADLINE` (vertical deadline line).
- **Rule:** Keep `simso_render.c` pure freestanding C11 (no `<stdio.h>`, no `malloc`).

**Compile & preview locally (`tools/out_gantt.ppm`):**
```bash
make -C tools test-ui
```

---

## 3. Running on Real Zanux (QEMU or USB Pendrive)

```bash
make -C user/worker
make -C user/simso
make -C user/shell
# Copy WORKER.BIN, SIMSO.BIN, SHELL.BIN into disk.img, then build bootable hybrid ISO:
make template.iso
```
Write `template.iso` to a USB pendrive with `dd if=template.iso of=/dev/sdX bs=4M status=progress` — Limine loads `disk.img` as a RAM-disk module automatically, so it boots on bare metal with zero USB driver needed.
