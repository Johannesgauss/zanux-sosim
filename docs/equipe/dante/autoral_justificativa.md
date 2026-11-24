# Justificativa do algoritmo Autoral (`pick_autoral`)

Arquivo: [`src/sched/policies/autoral.c`](../../../src/sched/policies/autoral.c)

## Parágrafo para o relatório

O algoritmo Autoral é um escalonador preemptivo baseado em **score dinâmico**, calculado a cada tick em aritmética inteira (o código roda em Ring 0, sem FPU). Cada tarefa pronta recebe a nota `4·urgência + 3·prioridade + 1·idade`, e a de maior nota executa. A *urgência* mede quão perto a tarefa está de perder o deadline (folga = `deadline − agora − tempo restante`; folga pequena gera urgência alta), a *prioridade* respeita a prioridade estática (1 = mais importante) e a *idade* soma o tempo de espera, o que impede *starvation*. Tarefas cujo deadline já está perdido recebem urgência zero, para não gastar CPU numa causa perdida, mas a idade as traz de volta ao final. Um pequeno bônus (+8) para a tarefa que já está na CPU evita trocas de contexto, e portanto overhead, por diferenças pequenas de nota. Empates são resolvidos pelo menor índice, o que torna a simulação determinística. Diferentemente de FCFS, SJF, PRIO e EDF, que olham um único critério, o Autoral combina deadline, prioridade e espera.

## Parâmetros

| Constante | Valor | Papel |
| :--- | :--- | :--- |
| `W_URG` | 4 | Peso da urgência (deadline) — o fator mais importante. |
| `W_PRIO` | 3 | Peso da prioridade estática. |
| `W_AGE` | 1 | Peso da idade (anti-starvation). |
| `W_STICK` | 8 | Bônus de permanência na CPU. |
| `URG_MAX` | 32 | Folga (ticks) a partir da qual a tarefa não é considerada urgente. |
| `PRIO_MAX` | 8 | Prioridades `>= 8` valem 0 pontos. |

> **Honestidade sobre os pesos:** os valores acima foram escolhidos à mão, por uma intenção
> (deadline > prioridade > idade), e **não foram otimizados nem treinados**. Se perguntarem,
> diga isso e explique a intenção.

## Exemplo: `casos/caso3.json` (quantum 2, overhead 1)

| Algoritmo | Deadlines cumpridos | Observação |
| :--- | :--- | :--- |
| **AUTORAL** | 3 de 4 (P1, P2, P3) | Preempte P1 no tick 1 para atender P2, que tem folga apertada. |
| FCFS | 2 de 4 | P2 espera P1 terminar e estoura o deadline. |
| RR | 2 de 4 | 7 trocas de contexto (contra 4 do Autoral). |

P2 (deadline 6) e P4 (deadline 8) não cabem juntos: o tick 0 só pode ser de P1 e o tick 1 é
overhead, então restam os ticks 2 a 7 (6 ticks) para 3 de P2 + 4 de P4 + 1 de overhead (8 ticks).
O Autoral cumpre o que cabe (P2) e, quando o deadline de P4 se perde (folga negativa → urgência
zero), executa P4 por último, sem atrapalhar as demais.

Para reproduzir:

```bash
make -C tools simso
./tools/simso --alg AUTORAL --input casos/caso3.json --gantt /tmp/c3.ppm --log /tmp/c3.log
```

> Nota: no momento em que este texto foi escrito, várias políticas dos colegas (EDF, PRIO, CFS)
> ainda eram esqueletos. A comparação deve ser refeita quando elas estiverem prontas.
