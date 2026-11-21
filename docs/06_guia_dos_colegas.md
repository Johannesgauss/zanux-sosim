# 06 — Guia Prático para os Colegas (Algoritmos + UI Nativa)

> **Origem deste guia:** *AI-generated* (`feat/simso`).  
> **Contrato congelado único:** [`src/sched/contract/sim.h`](../src/sched/contract/sim.h) (~70 linhas, 2 structs: `struct task` e `struct sim`).

Você **não precisa** saber assembly x86_64, paginação ou rodar QEMU no dia a dia. O projeto foi estruturado para que o **mesmo arquivo `.c`** que você testa em 1 segundo com `make -C tools ...` rode **sem modificação** dentro do kernel Zanux no pendrive USB.

---

## 1. Onde Cada Um Mexe (Sem Conflito de Merge)

| Integrante | Arquivos que edita | Como testa em 1 segundo |
| :--- | :--- | :--- |
| **Integrante #1** *(Game/Web/Rust + IA)* | `user/simso/simso_render.c` (usando `user/libc/include/zgfx.h`) e `src/sched/policies/autoral.c` | `make -C tools test-ui` e `make -C tools test-sched` |
| **Integrante #2** *(C++)* | `src/sched/policies/fcfs.c`, `sjf.c`, `prio.c`, `edf.c`, `cfs.c` *(use `rr.c` como referência)* | `make -C tools test-sched` |
| **Paulo** *(Kernel & Integração)* | `src/sched/sim_engine.c`, `src/main.c`, `template.iso`, pendrive USB | `make run` |

---

## 2. Implementando uma Política (`src/sched/policies/*.c`)

Cada algoritmo é apenas **1 função** em C11 puro (sem `iostream`, `vector`, `string`, `malloc` ou `float` — pois o kernel roda sem FPU):

```c
int pick_sjf(struct task *tasks, uint32_t task_count, int current_idx, uint32_t now, uint32_t quantum);
```

- Retorne o índice `0..task_count-1` da tarefa que deve rodar no tick `now`, ou `-1` se a CPU ficar ociosa.
- O `sim_core.c` já cuida do relógio `now`, chegadas (`arrival <= now`), sobrecarga de contexto (`overhead`), `remaining`, `slice_used`, `vruntime` inicial (`now * CFS_SCALE`), estouro de deadline e tabela final!
- Use `tasks[i].user_data` se precisar de um campo extra por tarefa (ex.: `rr.c` usa como número de sequência FIFO).

### Dicas rápidas por algoritmo
- **FCFS** (`fcfs.c`): se `current_idx >= 0 && tasks[current_idx].state == TASK_RUNNING`, retorne `current_idx`; senão, menor `arrival` entre `TASK_READY`.
- **SJF** (`sjf.c`): igual ao FCFS, mas escolhe o menor `burst` entre `TASK_READY`.
- **RR** (`rr.c`): **já implementado como referência.**
- **PRIO** (`prio.c`): preemptivo — escolha entre `TASK_READY`/`TASK_RUNNING` o menor `priority` (`1` = maior prioridade).
- **EDF** (`edf.c`): preemptivo — escolha entre `TASK_READY`/`TASK_RUNNING` o menor `deadline`.
- **CFS-Sim** (`cfs.c`): preemptivo — escolha entre `TASK_READY`/`TASK_RUNNING` o menor `vruntime`.
- **Autoral** (`autoral.c`): heurística/IA inteira combinando folga de deadline (`deadline - now - remaining`), prioridade, espera acumulada, etc.

**Testar localmente em 1 segundo:**
```bash
make -C tools test-sched
```

---

## 3. Implementando a UI Nativa (`user/simso/simso_render.c`)

Você edita **`user/simso/simso_render.c`** implementando:
```c
void render_gantt(uint32_t *fb, uint32_t width, uint32_t height,
                  const struct sim *sim, uint32_t visible_ticks);
```

- Ring 3 tem **FPU/SSE habilitado** (`float` e `double` funcionam no Zanux!) e `user/libc/include/zgfx.h` já entrega widgets prontos: `zgfx_clear`, `zgfx_rect`, `zgfx_vline`, `zgfx_panel`, `zgfx_window`, `zgfx_progress`, `zgfx_text`, `zgfx_printf` (com `%.2f`).
- **Testar localmente em 1 segundo (gera `tools/out_gantt.ppm`):**
```bash
make -C tools test-ui
```

---

## 4. Rodando no Zanux Real (QEMU ou Pendrive USB)

```bash
make run
```
No pendrive USB: `sudo dd if=template.iso of=/dev/sdX bs=4M status=progress conv=fsync`.
