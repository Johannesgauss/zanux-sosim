# 08 — Divisão do Grupo e Cronograma (Entrega: 04/Dez)

> **Regra de ouro:** **Ninguém edita `src/sched/contract/sim.h`** sem combinar com o grupo inteiro.  
> Tudo fora de `src/sched/contract/` pode ser editado e testado em paralelo **sem conflito de merge** e **sem precisar de QEMU**.

---

## 1. Divisão de Responsabilidades

| Integrante | Papel | Arquivos que edita | Como testa em 1 segundo |
| :--- | :--- | :--- | :--- |
| **Paulo** | Kernel, contrato `sim.h`, boot em pendrive USB real e integração final | `src/` (exceto `policies/`), `boot/limine.conf`, `GNUmakefile` | `make run` |
| **Integrante #1** *(Game/Web/Rust + IA)* | **UI Nativa no Framebuffer** (Gantt + tabela + resumo + 3 janelas animadas: Plasma+Primos, Game of Life, Bubble Sort + modo passo-a-passo) **+ Política Autoral (IA)** *(+ bônus VM opcional)* | `user/simso/simso_render.c` (usando `user/libc/include/zgfx.h`) e `src/sched/policies/autoral.c` | `make -C tools test-ui` e `make -C tools test-sched` |
| **Integrante #2** *(C++)* | **5 Políticas Padrão** (`fcfs`, `sjf`, `prio`, `edf`, `cfs` — `rr.c` já está pronta como referência!) + conferência manual das métricas do *Caso Base* e *Caso com Ociosidade* | `src/sched/policies/fcfs.c`, `sjf.c`, `prio.c`, `edf.c`, `cfs.c` *(15–30 linhas de C puro cada)* | `make -C tools test-sched` |
| **Integrante #3** *(Apoio / Entregáveis)* | Casos de teste extras, montagem do **relatório (4–6 páginas)** com tabelas e prints do Gantt, edição do **vídeo (≤ 6 min)** | Relatório, casos de teste e vídeo | — |

---

## 2. Primeiros Passos para Cada Colega (5 minutos)

1. Baixar a branch `feat/simso`:
   ```bash
   git checkout feat/simso
   ```
2. Ler **`src/sched/contract/sim.h`** (~70 linhas, autoexplicativo) e **`tools/README.md`**.
3. Rodar seu comando de teste local:
   - **Algoritmos (#1 e #2):** `make -C tools test-sched`
   - **Visualização (#1):** `make -C tools test-ui` → abre `tools/out_gantt.ppm`

> **Dica para o Integrante #1 (UI):** `user/libc/include/zgfx.h` já fornece `zgfx_clear`, `zgfx_rect`, `zgfx_vline`, `zgfx_panel`, `zgfx_window`, `zgfx_progress`, `zgfx_text` e `zgfx_printf` (com suporte a `%.2f` em Ring 3 via SSE!).

> **Dica para o Integrante #2 (Políticas):** Cada arquivo em `src/sched/policies/` tem só **1 função** `pick_xxx(tasks, task_count, current_idx, now, quantum)`. Não use `<vector>`, `<string>` ou `float` — apenas `for` e `uint32_t` (veja `rr.c` e `fcfs.c` como exemplo).

---

## 3. Cronograma até 04/Dezembro

| Período | Marco |
| :--- | :--- |
| **21/Nov (Hoje)** | Push de `feat/simso` com `sim.h` congelado e harnesses funcionando. |
| **22/Nov – 28/Nov** | #2 implementa e valida `fcfs`, `sjf`, `prio`, `edf`, `cfs`; #1 implementa `autoral.c` e o layout completo em `simso_render.c`; Paulo valida boot no pendrive USB no notebook real da apresentação. |
| **29/Nov – 01/Dez** | Integração final na `feat/simso`, teste dos 7 algoritmos + passo-a-passo no pendrive USB real, coleta de prints e métricas finais. |
| **02/Dez – 03/Dez** | Redação/revisão do relatório (4–6 páginas), gravação/edição do vídeo (≤ 6 min) e ensaio rápido da arguição. |
| **04/Dez** | Entrega final. |
