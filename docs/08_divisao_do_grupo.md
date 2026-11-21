# 08 — Divisão do Grupo e Cronograma (Entrega: 04/Dez)

> **Regra de ouro:** **Ninguém edita `src/sched/contract/sim.h`** sem combinar com o grupo inteiro.  
> Tudo fora de `src/sched/contract/` pode ser editado e testado em paralelo **sem conflito de merge** e **sem precisar de QEMU**.

---

## 1. Divisão de Responsabilidades (Pastas Individuais)

| Integrante | Guia Individual | Arquivos que edita | Como testa em 1 segundo |
| :--- | :--- | :--- | :--- |
| **Paulo** | [`docs/equipe/paulo/`](./equipe/paulo/README.md) | Kernel (`src/` exceto `policies/`), contrato `sim.h`, pendrive USB e integração final | `make run` |
| **Dante** *(Game/Web/Rust + IA)* | [`docs/equipe/dante/`](./equipe/dante/README.md) | `user/simso/simso_render.c` (UI nativa com `zgfx.h`) e `src/sched/policies/autoral.c` (IA) *(+ bônus VM opcional)* | `make -C tools test-ui` e `make -C tools test-sched` |
| **Petra** *(C++)* | [`docs/equipe/petra/`](./equipe/petra/README.md) | `src/sched/policies/{fcfs,sjf,prio,edf,cfs}.c` (5 políticas padrão; `rr.c` já pronta como referência) | `make -C tools test-sched` |
| **João Vitor** *(Entregáveis)* | [`docs/equipe/joao_vitor/`](./equipe/joao_vitor/README.md) | Casos de teste extras, relatório (4–6 páginas) e vídeo (≤ 6 min) *(fora do caminho crítico de código)* | `make -C tools test-sched` / `test-ui` |

---

## 2. Cronograma até 04/Dezembro

| Período | Marco |
| :--- | :--- |
| **21/Nov (Hoje)** | Repo [`zanux-sosim`](https://github.com/Johannesgauss/zanux-sosim) no ar com `sim.h` congelado e pastas individuais prontas. |
| **22/Nov – 28/Nov** | **Petra** implementa/valida `fcfs`, `sjf`, `prio`, `edf`, `cfs`; **Dante** implementa `autoral.c` e a UI completa em `simso_render.c`; **Paulo** valida boot no pendrive USB real; **João Vitor** prepara casos de teste e estrutura do relatório. |
| **29/Nov – 01/Dez** | Integração final, teste dos 7 algoritmos + passo-a-passo no pendrive USB real, coleta de prints e métricas finais. |
| **02/Dez – 03/Dez** | Fechamento do relatório (4–6 páginas), gravação/edição do vídeo (≤ 6 min) e ensaio rápido da arguição. |
| **04/Dez** | Entrega final. |
