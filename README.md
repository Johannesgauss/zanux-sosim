# Zanux Kernel & SIMSO

An x86-64 hobby OS kernel written in C, Assembly, and Rust — featuring **SIMSO**, a discrete-event process scheduling simulator that schedules **real Ring 3 processes** on IRQ0 with live visual mini-windows, Gantt chart rendering, JSON test cases, and a host CLI.

---

## What Works

* **Boot & Arch (x86-64):** Limine bootloader (BIOS & UEFI hybrid ISO, bootable from USB via Limine RAM-disk module), GDT, IDT, 100 Hz PIT timer (`IRQ0`), PS/2 keyboard driver, and per-process FPU/SSE (`fxsave`/`fxrstor`).
* **Memory:** Physical & Virtual Memory Managers (PMM/VMM), per-process isolated `CR3` address spaces, `kmalloc`/`kfree`, and `usr_kmalloc`.
* **Storage & Filesystem:** ATA PIO + RAM-disk block layer, GPT partition parsing, and FAT32 root-directory reader (`SHELL.BIN`, `SIMSO.BIN`, `SORT.BIN`, `LIFE.BIN`, `PLASMA.BIN`, `CASO1.JSN`, `DOOM.BIN`).
* **Syscalls & Userspace (Ring 3):** `syscall`/`sysretq` dispatch table (`18` syscalls), custom `libzanuxc` + `zgfx` 2D graphics library, interactive mini-shell (`help`, `ls`, `cat`, `clear`, `simso`, `doom`, `run`, `exit`), and Sound Blaster 16 / AdLib audio.
* **SIMSO (Scheduling Simulator):**
  * Frozen single-header contract in [`src/sched/contract/sim.h`](src/sched/contract/sim.h).
  * 7 scheduling policies (`FCFS`, `SJF`, `RR`, `PRIO`, `EDF`, `CFS-Sim`, `Autoral`) in `src/sched/policies/`.
  * Real Ring 3 visual worker binaries (`SORT.BIN`, `LIFE.BIN`, `PLASMA.BIN`) scheduled live in `src/sched/sim/sim_engine.c` and composited into live windows via `SYS_WIN_BLIT`/`SYS_WIN_READ` (also runnable standalone in fullscreen via `run SORT.BIN`).
  * Freestanding JSON test-case loader (`casos/caso1.json`, `casos/caso2.json`) and CLI flags (`--alg`, `--input`, `--seed`, `--delay`, `--quantum`, `--overhead`) both in the Zanux shell and on the Linux host.

---

## Quick Start for the Team (No QEMU Needed)

Teammates working on scheduling policies, UI rendering, or comparative analysis can compile and test natively on Linux in ~1 second:

```bash
# Run all 7 scheduling policies on Caso Base & Caso Ociosidade
make -C tools test-sched

# Render the 640x400 Gantt framebuffer to tools/out_gantt.ppm
make -C tools test-ui

# Build and run the full host CLI with JSON input, log, and Gantt output
make -C tools simso
./tools/simso --alg RR --input casos/caso1.json --gantt /tmp/caso1.ppm --log /tmp/caso1.log --seed 42
```

See the documentation index in [`docs/README.md`](docs/README.md) and the individual team guides in [`docs/08_divisao_do_grupo.md`](docs/08_divisao_do_grupo.md).

---

## Building & Running the Kernel (QEMU)

### Dependencies
`make`, `gcc`, `nasm`, `xorriso`, `mtools`, `util-linux` (`sfdisk`), `qemu-system-x86_64`, `curl`, `tar`, and `rustc`/`rustup` with the `x86_64-unknown-none` target.

#### Ubuntu / Debian
```bash
sudo apt update
sudo apt install -y build-essential nasm xorriso mtools fdisk qemu-system-x86 curl
rustup target add x86_64-unknown-none
```

#### Arch Linux
```bash
sudo pacman -S --needed base-devel nasm xorriso mtools util-linux qemu-system-x86 curl rustup
rustup target add x86_64-unknown-none
```

#### Fedora
```bash
sudo dnf install -y @development-tools gcc nasm xorriso mtools util-linux qemu-system-x86-core curl
rustup target add x86_64-unknown-none
```

### Run in QEMU
```bash
make run
```

Inside the Zanux shell:
```text
zanux> simso --alg RR --input CASO1.JSON
zanux> simso --alg CFS --input CASO2.JSON --delay 150
zanux> run SORT.BIN
zanux> run LIFE.BIN
zanux> run PLASMA.BIN
```
In `simso`, press **`1`–`7`** to switch algorithms, **`Space`** for step-by-step ticks, **`Enter`** for the full Gantt chart, and **`ESC`/`Q`** to return to the shell.
