# Guia do Dante — UI Nativa no Framebuffer + Política Autoral (IA)

Fala, Dante! Você está com a parte mais visual e criativa do trabalho — e **não precisa rodar QEMU** para desenvolver e testar.

---

## 1. Seus Arquivos (Só Mexa Nestes!)

| Arquivo | O que você faz |
| :--- | :--- |
| **`user/simso/simso_render.c`** | Renderização da tela inteira no framebuffer (`render_gantt()`): gráfico de Gantt, legenda, tabela final, resumo quantitativo e (se der tempo) os 3 painéis animados (*Plasma+Primos*, *Game of Life*, *Bubble Sort*) avançando conforme cada processo recebe CPU! |
| **`src/sched/policies/autoral.c`** | Algoritmo **Autoral** (`pick_autoral()`): política baseada em IA / heurística adaptativa em C inteiro (ex.: perceptron inteiro, Q-table ou score dinâmico combinando folga de deadline + prioridade + espera). |
| *(Opcional — Bônus VM)* **`src/sched/sim_core.c`** | Simular page faults determinísticos com `num_pages`, `disk_cost` e `seed` marcando `TICK_IO` (ciano) e incrementando `page_faults`. |

> **Regra de ouro:** **Não edite `src/sched/contract/sim.h`** sem combinar antes com o grupo (é o contrato compartilhado por todos).

---

## 2. Como Testar em 1 Segundo no Seu PC (Sem QEMU)

```bash
# Testar sua UI -> gera tools/out_gantt.ppm (abra em qualquer visualizador de imagens!)
make -C tools test-ui

# Testar a CLI completa com JSON (gera log + Gantt PPM):
make -C tools simso
./tools/simso --alg RR --input casos/caso1.json --gantt /tmp/caso1.ppm --log /tmp/caso1.log

# Testar o algoritmo Autoral (+ todos os outros 6) no terminal:
make -C tools test-sched
```

---

## 3. Ferramentas Prontas Para Você

1. **Contrato de dados:** [`src/sched/contract/sim.h`](../../../src/sched/contract/sim.h) (~70 linhas) — todos os dados (`tasks[]`, `tl_kind[]`, `tl_task[]`, `total_ticks`, `ctx_switches`, `quantum`, `overhead`) já chegam prontos em `const struct sim *sim`.
2. **Biblioteca gráfica 2D pronta:** [`user/libc/include/zgfx.h`](../../../user/libc/include/zgfx.h) — funções com clipping automático:
   - `zgfx_clear`, `zgfx_pixel`, `zgfx_hline`, `zgfx_vline`, `zgfx_rect`, `zgfx_rect_outline`
   - `zgfx_panel` (card com borda), `zgfx_window` (janela com título + badge `OK`/`ESTOUR` à direita), `zgfx_progress` (barra de progresso 0..100%)
   - `zgfx_text(..., scale)` (`scale=2` para título grande) e `zgfx_printf(..., "Esp: %.2f", val)`
3. **Ponto flutuante liberado na UI:** O Zanux já tem **FPU/SSE habilitado em Ring 3**, então `simso_render.c` pode usar `float` e `double` normalmente.
   - *Atenção:* `src/sched/policies/autoral.c` roda dentro do **kernel (Ring 0)**, então em `autoral.c` use **apenas inteiros** (sem `float`/`double`, sem `malloc`, sem `stdio`).
4. **Área reservada para janelas Ring 3 ao vivo (`y = 46..136`):** No Zanux, `simso.c` desenha as janelas ao vivo dos processos reais (`SORT.BIN`, `LIFE.BIN`, `PLASMA.BIN`) na faixa **`y = 46..136`** por cima do framebuffer retornado por `render_gantt()`. Mantenha o cabeçalho em `y = 0..42` e o Gantt + tabela final + resumo quantitativo em **`y = 142..399`** para não sobrepor as janelas ao vivo!

---

## 4. Checklist de Entrega (Rubrica)
- [ ] Gantt com cores obrigatórias: verde (`COL_EXEC`), vermelho (`COL_OVERHEAD`), cinza (`COL_MISS`), linha vertical amarela (`COL_DEADLINE`) + ciano (`COL_IO` se VM ativa).
- [ ] Tabela final visível na tela: `ID, Cheg, Exec, Dead, Prio, Ini, Term, Esp, TAT, DeadOK`.
- [ ] Resumo quantitativo: média de espera, média de turnaround, throughput, `%` ociosidade, trocas de contexto.
- [ ] Modo passo-a-passo respeitando `visible_ticks` (já ligado na tecla `Espaço` no Zanux).
- [ ] `pick_autoral()` implementado e justificado em 1 parágrafo para o relatório.
