# Guia do Paulo — Kernel, Integração, Pendrive USB e Arguição

Checklist pessoal para manter tudo sob controle até **04/Dez** (e sem se perder em reescritas antes da entrega!).

---

## 1. O Que Você Comanda
- **Contrato congelado:** [`src/sched/contract/sim.h`](../../../src/sched/contract/sim.h) — qualquer alteração passa por você.
- **Kernel & Boot:** `src/` (exceto `src/sched/policies/`), `boot/limine.conf`, `GNUmakefile`, `template.iso`.
- **Pendrive USB:** Gravar `template.iso` com `dd` e testar no **notebook real da apresentação** com antecedência (verificar boot UEFI/BIOS + teclado PS/2 / USB Legacy!).

---

## 2. Checklist de Estudo para a Arguição (~1–2h)
Veja o mapa completo em [`docs/ATTRIBUTION.md`](../../ATTRIBUTION.md). Os 6 pontos de código gerado/assistido por IA que vale dominar antes da arguição:
1. `src/arch/sys/syscall.S` — por que salva `rdi, rsi, rdx, r8, r9, r10` e move `r10 -> rcx`.
2. `src/arch/int/arch_proc.h` + `src/arch/vmm.c` — leitura direta de `CR3` (`current_pml4`), `arch_addr_space_save/restore` e `fxsave`/`fxrstor` por processo para FPU/SSE em Ring 3.
3. `src/sched/sim_engine.c` — fluxo não-bloqueante `sys_sim_start` → IRQ0 (`sim_engine_tick`) → troca de `CR3` para `WORKER.BIN` → `sys_sim_poll`.
4. `src/sched/sim_core.c` — chegadas, `overhead`, `deadline_ok`, modo `burst == 0` (`sim_notify_exit`).
5. `src/drivers/disk/blk.c` + `boot/limine.conf` — módulo RAM-disk do Limine para pendrive USB + entrada ATA PIO (`rep insw`).
6. `src/sys/exec.c` — `sys_fb_blit` (buffer `s_row_buf` em RAM), `sys_readdir`, `sys_audio_write` (SB16).

---

## 3. Comandos Rápidos de Integração
```bash
# Testar políticas e UI enviadas pela Petra e pelo Dante:
make -C tools test-sched
make -C tools test-ui

# Compilar tudo e testar no QEMU:
make -C user/libc && make -C user/simso && make run

# Gravar no pendrive USB (confira /dev/sdX com lsblk!):
sudo dd if=template.iso of=/dev/sdX bs=4M status=progress conv=fsync
```
