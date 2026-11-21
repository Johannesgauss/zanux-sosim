# 07 — Justificativa Linha a Linha de `src/sched/contract/sim.h`

O contrato inteiro foi consolidado em **um único header de ~140 linhas** ([`src/sched/contract/sim.h`](../src/sched/contract/sim.h)) com **apenas 2 structs** (`struct task` e `struct sim`), zero vtables e zero campos duplicados.

> **Por que apenas `<stdbool.h>` e `<stdint.h>`?**
> O kernel compila sem libc e sem FPU (`-mno-sse -mno-80387`), enquanto o Ring 3 (`SIMSO.BIN`) tem FPU/SSE + `user/libc/` (`libzanuxc.a` + `zgfx.h`), e os testes em `tools/` usam o `gcc` + `glibc` do host. Usar apenas tipos padrão de largura fixa garante compilação idêntica nos 3 ambientes.

---

## 1. Constantes e Cores

| Símbolo | Valor | Por que existe |
| :--- | :--- | :--- |
| `SIM_MAX_TASKS` | `64` | Limite estático de processos por simulação (evita `malloc` no IRQ0 e casa com `SCHED_MAX_PROCS = 64`). |
| `SIM_MAX_TICKS` | `512` | Tamanho máximo da linha do tempo gravada para o Gantt (`512` ticks = 5,12 s no PIT 100 Hz). |
| `CFS_SCALE` | `1024U` | Escala de ponto fixo (`1.0 == 1024`) para o `vruntime` do CFS sem precisar de `float` no kernel (igual ao `NICE_0_LOAD` do Linux). |
| `SIMSO_FB_W`, `SIMSO_FB_H` | `640U`, `400U` | Resolução lógica do canvas passado para `render_gantt()` e `sys_fb_blit()` (escala 2× em `1280×800`). |
| `COL_*` | `0x00RRGGBB` | Paleta RGB 24-bit padronizada: verde (`COL_EXEC`), vermelho (`COL_OVERHEAD`), cinza (`COL_MISS`), amarelo (`COL_DEADLINE`), ciano (`COL_IO`), escuro (`COL_BG`/`COL_PANEL`/`COL_IDLE`) e branco (`COL_TEXT`). |

---

## 2. Enums

- **`enum alg_id` (`ALG_FCFS` .. `ALG_AUTORAL`, `ALG_COUNT = 7`):** os 7 algoritmos obrigatórios do enunciado.
- **`enum tick_kind` (`TICK_IDLE`, `TICK_EXEC`, `TICK_OVERHEAD`, `TICK_MISS`, `TICK_IO`):** o que aconteceu na CPU no tick `t` — define a cor de cada célula do gráfico de Gantt.
- **`enum task_st` (`TASK_PENDING`, `TASK_READY`, `TASK_RUNNING`, `TASK_BLOCKED`, `TASK_DONE`):** ciclo de vida individual de cada tarefa.

---

## 3. `struct task` (Embutida em `struct proc` como `struct task sched`)

Reúne **entrada + estado de execução + resultados finais** de um processo sem duplicar campos em 3 structs diferentes:
- **Identificação:** `name[16]` (rótulo `"P1"`, `"P2"`...), `bin_name[16]` (binário Ring 3 opcional no FAT32; vazio = `WORKER.BIN`).
- **Entradas:** `arrival`, `burst` (`>0` = ticks fixos do *Caso Base*; `0` = processo real autônomo que roda até chamar `sys_exit()`), `priority` (`1` = maior), `deadline` (absoluto), `num_pages` (bônus VM).
- **Estado em execução:** `remaining`, `vruntime` (tempo virtual do CFS), `slice_used` (ticks na fatia atual para o `quantum` do RR), `state` (`enum task_st`), `user_data` (campo livre de 32 bits para cada política, ex.: fila FIFO do RR).
- **Resultados (Tabela Final):** `start` (início), `finish` (término), `wait` (`turnaround - burst`), `turnaround` (`finish - arrival`), `preemptions`, `page_faults`, `ok` (`1` se `finish <= deadline`).

---

## 4. `struct sim`

Struct única passada em `sys_sim_start(&s)` e retornada em `sys_sim_poll(&s)`:
- **Entradas globais:** `alg`, `quantum`, `overhead`, `disk_cost`, `seed`, `n`, `tasks[64]`.
- **Saídas (Gantt + Resumo):** `tl_kind[512]`, `tl_task[512]`, `total_ticks`, `idle_ticks`, `overhead_ticks`, `ctx_switches`, `total_preemptions`.
- **Cursores do motor (`sim_core.c`):** `now`, `done_count`, `overhead_left`, `finished`, `cur`, `last`, `pending_next`.

---

## 5. Funções do Contrato

- **7 algoritmos (`src/sched/policies/*.c`):** `int pick_fcfs/sjf/rr/prio/edf/cfs/autoral(tasks, task_count, current_idx, now, quantum)` — retorna `0..task_count-1` ou `-1` (ocioso).
- **Motor (`src/sched/sim_core.c`):** `sim_init(sim)`, `sim_step(sim, &active_task)` (avança 1 tick discreto), `sim_notify_exit(sim, task_idx)` (chamada pelo kernel em `proc_exit()` para processos reais `burst == 0`).
- **UI (`user/simso/simso_render.c`):** `render_gantt(fb, width, height, sim, visible_ticks)` — desenha o Gantt e métricas em um buffer RGB `uint32_t`, compilando idêntico em `SIMSO.BIN` e `make -C tools test-ui`.
