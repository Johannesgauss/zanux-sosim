# Guia do João Vitor — Casos de Teste, Relatório (4–6 págs) e Vídeo (≤ 6 min)

Fala, João Vitor! Sua parte é organizar os **entregáveis finais** do trabalho (que valem peso direto na nota: **20% análise comparativa + 20% documentação + vídeo**), sem precisar mexer em código do kernel nem configurar QEMU.

---

## 1. Suas Entregas

1. **Casos de Teste Extras (além do Caso Base):**
   - Documentar pelo menos 3 cenários para o grupo rodar e comparar:
     - **Caso Base** (do enunciado: `quantum=2, sobrecarga=1`, `P1`, `P2`, `P3`).
     - **Caso com Ociosidade** (processos chegando espaçados, validando CPU idle).
     - **Caso de Estresse / Deadlines Apertados** (mostrando a vantagem do EDF e a justiça do CFS-Sim vs RR).
2. **Relatório Final (4 a 6 páginas, PDF):**
   - **Design e decisões de implementação:** resumo da arquitetura rodando no kernel real Zanux em pendrive USB (pode usar `docs/04_processos_e_sched.md` e `docs/07_justificativa_contrato.md` como base).
   - **Prints dos Gantts e tabelas finais** de todos os 7 algoritmos (FCFS, SJF, RR, Prioridades, EDF, CFS-Sim, Autoral) — gerados por `make -C tools test-ui` e/ou fotos do Zanux rodando.
   - **Análise comparativa quantitativa:** tabela comparando *Avg Espera*, *Avg Turnaround*, *Throughput*, *% Ociosidade*, *Trocas de Contexto* e *Deadlines cumpridos* entre os 7 algoritmos (com gráfico de barras).
   - *(Se o bônus de VM entrar)* Seção curta sobre impacto dos page faults nas métricas.
3. **Vídeo de Demonstração (máximo 6 minutos):**
   - Roteiro sugerido:
     - `0:00 – 1:00`: Boot do Zanux direto do pendrive USB / apresentação da arquitetura.
     - `1:00 – 4:00`: Execução dos algoritmos mostrando Gantt, sobrecarga vermelha, estouro cinza, linhas de deadline e modo passo-a-passo.
     - `4:00 – 5:30`: Comparação rápida (FCFS vs RR vs EDF vs CFS-Sim vs Autoral).
     - `5:30 – 6:00`: Conclusão.

---

## 2. Como Gerar os Dados/Prints (Opcional, se tiver `gcc`)

```bash
make -C tools test-sched   # Imprime tabelas e métricas de todos os algoritmos
make -C tools test-ui      # Gera imagem tools/out_gantt.ppm do Gantt
```

---

## 3. Prazo Interno
- **Até 28/Nov:** Template do relatório + tabela comparativa + casos de teste prontos.
- **02/Dez – 03/Dez:** Inserir prints finais dos 7 algoritmos e fechar o vídeo (≤ 6 min) para entrega em **04/Dez**.
