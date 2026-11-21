# ![Bandeira Não-Binária](./nb_flag.svg) Guia da Petra — Políticas de Escalonamento (FCFS, SJF, PRIO, EDF, CFS)

Oi, Petra! Você está com a parte central da nota (**40% da avaliação**: correção e fidelidade dos algoritmos) — e o motor de relógio, chegadas, sobrecarga de contexto e métricas **já está pronto** em `src/sched/sim_core.c`, então você só precisa escrever **5 funções pequenas em C puro** (15–30 linhas cada)!

---

## 1. Seus Arquivos (Só Mexa Nestes!)

| Arquivo | Função | Preemptivo? | Regra principal |
| :--- | :--- | :---: | :--- |
| **`src/sched/policies/fcfs.c`** | `pick_fcfs()` | Não | Se `current_idx` está `TASK_RUNNING`, mantenha. Senão, escolha `TASK_READY` com **menor `arrival`**. *(Já tem base pronta — confira e valide!)* |
| **`src/sched/policies/sjf.c`** | `pick_sjf()` | Não | Se `current_idx` está `TASK_RUNNING`, mantenha. Senão, escolha `TASK_READY` com **menor `burst`** (desempate por `arrival`). |
| **`src/sched/policies/prio.c`** | `pick_prio()` | **Sim** | Entre `TASK_READY` e `TASK_RUNNING`, escolha o de **menor `priority`** (**atenção: `1` = maior prioridade**, use `<`, não `>`!). |
| **`src/sched/policies/edf.c`** | `pick_edf()` | **Sim** | Entre `TASK_READY` e `TASK_RUNNING`, escolha o de **menor `deadline`**. |
| **`src/sched/policies/cfs.c`** | `pick_cfs()` | **Sim** | Entre `TASK_READY` e `TASK_RUNNING`, escolha o de **menor `vruntime`**. *(O `sim_core.c` já inicializa `vruntime = now * CFS_SCALE` na chegada e incrementa `priority * CFS_SCALE` a cada tick.)* |

> **Exemplo pronto:** Abra [`src/sched/policies/rr.c`](../../../src/sched/policies/rr.c) — o **Round-Robin** já está 100% implementado como referência para você ver o estilo exato!

---

## 2. Regras Importantes (C++ → C do Kernel)

Como suas funções rodam **dentro do kernel Zanux** (sem biblioteca padrão C++):
1. **Não use** `#include <iostream>`, `<vector>`, `<string>`, `<algorithm>`, `malloc` ou `float`/`double`.
2. **Use apenas** `for`, `if`, `uint32_t`, `int`, e os campos de `struct task` definidos em [`src/sched/contract/sim.h`](../../../src/sched/contract/sim.h).
3. **Não edite `src/sched/contract/sim.h`** sem combinar antes com o grupo.
4. **Assinatura de todas as funções:**
   ```c
   int pick_xxx(struct task *tasks, uint32_t task_count, int current_idx, uint32_t now, uint32_t quantum);
   ```
   Retorne o índice `0..task_count-1` do processo escolhido, ou `-1` se nenhum estiver pronto.

---

## 3. Como Testar em 1 Segundo no Seu PC (Sem QEMU)

```bash
make -C tools test-sched
```
Isso compila todas as políticas com o `gcc` normal do seu PC e imprime o **Gantt ASCII**, a **tabela completa** e as **médias** para o *Caso Base* e o *Caso com Ociosidade*!

---

## 4. Checklist de Entrega
- [ ] `fcfs.c`, `sjf.c`, `prio.c`, `edf.c` e `cfs.c` implementados e compilando sem warnings em `make -C tools test-sched`.
- [ ] Conferir na mão os números esperados do *Caso Base* (`quantum=2, overhead=1, P1(0,5,p2,d8), P2(1,4,p1,d12), P3(3,2,p3,d20)`) para garantir 100% de fidelidade na arguição.
- [ ] Escrever 1 parágrafo curto explicando os resultados comparativos dos 5 algoritmos para o relatório.
