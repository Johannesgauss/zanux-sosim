# 05 — Syscalls (0..14), User Space (Ring 3), `libzanuxc` e Ferramentas de Host

## Tabela de Autoria deste Módulo

| Arquivo | Origem | Resumo |
| :--- | :--- | :--- |
| `src/sys/io.{h,c}`, `exec.{h,c}`, `syscall.{h,c}` | **AI-generated** (`e209a78`, `e424cb6`, `39183c3`) | Implementação, headers Doxygen e tabela de despacho das syscalls `0..14`. |
| `src/sys/zm.c` | **Autor (Paulo)** | Experimento Zanux Machine (não usado no caminho de syscalls). |
| `user/lib/{syscall.h,memory.c}` | **AI-generated** (`e1ef549`, `39183c3`) | Wrappers inline `sys_*` em Ring 3 e `memcpy`/`memset` de usuário. |
| `user/libc/` (`libzanuxc.a` + `zgfx.h`) | **AI-generated** (`39183c3`) | Libc mínima de Ring 3 (`string.h`, `stdlib.h`, `stdio.h` com `%.2f`) e widgets 2D `zgfx.h`. |
| `user/shell/*`, `user/doom/*` | **AI-generated** (`e1ef549`, `1c9ca16`) | Mini-shell Ring 3 (`SHELL.BIN`) e port Ring 3 do DOOM (`DOOM.BIN`) com mixer PCM `ds*` no SB16 e OPL2. |
| `user/worker/*`, `user/simso/*` | **AI-generated** (`39183c3`) | Worker Ring 3 (`WORKER.BIN`) e visualizador Gantt nativo (`SIMSO.BIN`, com FPU/SSE habilitado). |
| `tools/*` | **AI-generated** (`39183c3`) | Harnesses de teste em host (`make -C tools test-sched` / `test-ui`). |

---

## 1. Tabela Completa de Syscalls (`src/sys/syscall.h`)

Convenção x86_64: número da syscall em `%rax`, argumentos em `%rdi`, `%rsi`, `%rdx`, `%r10`, `%r8`, `%r9`; retorno em `%rax`.

| # | Nome | Handler | Descrição |
| :-: | :--- | :--- | :--- |
| `0` | `SYS_EXIT` | `sys_exit` (`syscall.S` → `proc_exit`) | Encerra o processo atual, drena teclado e acorda `SHELL.BIN` se bloqueada. |
| `1` | `SYS_WRITE` | `sys_write(fd, buf, len)` | Escreve em `FD_STDOUT` (verde fósforo `#33FF33`; `'\x0C'` limpa tela) ou `FD_STDERR` (vermelho). |
| `2` | `SYS_READ` | `sys_read(fd, buf, len)` | Leitura **não-bloqueante** de `FD_STDIN` (`kb_getchar()` com fallback UART COM1). Retorna `0` se não houver tecla. |
| `3` | `SYS_EXEC` | `sys_exec(path)` | Lê binário flat do FAT32, cria processo filho via `proc_create()`, bloqueia o chamador (`PROC_BLOCK`) até o filho sair. |
| `4` | `SYS_ALLOC` | `sys_alloc(size)` | Aloca memória no espaço virtual de usuário via `usr_kmalloc()`. |
| `5` | `SYS_READ_FILE` | `sys_read_file(name, dst, max)` | Lê um arquivo da raiz FAT32 para o buffer `dst` (se `dst == NULL`, retorna apenas o tamanho). |
| `6` | `SYS_FB_BLIT` | `sys_fb_blit(pixels, w, h)` | Escala (fator inteiro centralizado) e copia um buffer `0x00RRGGBB` para o framebuffer Limine usando buffer de linha em RAM (`s_row_buf`) + escritas 64-bit somente-escrita na VRAM. |
| `7` | `SYS_POLL_SCANCODE` | `sys_poll_scancode()` | Desenfileira 1 scancode PS/2 bruto (ou `-1` se vazio). Usado por `DOOM.BIN` e `SIMSO.BIN`. |
| `8` | `SYS_TICKS_MS` | `sys_ticks_ms()` | Retorna `g_timer_ticks * 10` ms (derivado do PIT 100 Hz). |
| `9` | `SYS_PCSPK` | `sys_pcspk(divisor)` | Controla o PC Speaker (PIT canal 2, porta `0x61`); `0` silencia. |
| `10` | `SYS_OPL_IO` | `sys_opl_io(port, val, is_wr)` | Acesso controlado (whitelist `0x388..0x38B`) ao sintetizador AdLib/OPL2 para música do DOOM em Ring 3. |
| `11` | `SYS_READDIR` | `sys_readdir(idx, name, sz)` | Enumera entradas válidas da raiz FAT32 formatadas como `"NOME.EXT"`. |
| `12` | `SYS_SIM_START` | `sys_sim_start(sim)` | Inicia simulação de escalonamento no kernel com processos `WORKER.BIN` reais. |
| `13` | `SYS_SIM_POLL` | `sys_sim_poll(out_sim)` | Retorna `0` enquanto a simulação roda; `1` após copiar `struct sim` finalizado. |
| `14` | `SYS_AUDIO_WRITE` | `sys_audio_write(pcm, len)` | Envia amostras PCM 8-bit 11025 Hz para o ring buffer DMA do Sound Blaster 16 (`pcm == NULL` silencia). |

---

## 2. Programas Ring 3 (`user/`)

Todos os programas de usuário são compilados como binários flat linkados em **`0x00400000`** (`link.ld`) com ponto de entrada `_start` (`start.S`):

- **`user/shell/` → `SHELL.BIN` (PID 0):**
  - Mini-shell interativa com comandos `help`, `ls`, `cat <arquivo>`, `clear` (ou `Ctrl+L`), `echo`, `doom`, `run <arquivo>` e `exit`.
- **`user/doom/` → `DOOM.BIN`:**
  - Port de `doomgeneric` rodando 100% em Ring 3 usando `sys_read_file` (carrega `DOOM1.WAD`), `sys_alloc`, `sys_fb_blit`, `sys_poll_scancode`, `sys_ticks_ms`, `sys_pcspk` e `sys_opl_io`.
- **`user/worker/` → `WORKER.BIN`:**
  - Loop CPU-bound mínimo em Assembly Ring 3 (`inc %rax; pause; jmp`), instanciado em múltiplos espaços de endereços reais pelo `sim_engine`.
- **`user/simso/` → `SIMSO.BIN`:**
  - Frontend nativo do trabalho de Sistemas Operacionais: chama `sys_sim_start`/`sys_sim_poll` e desenha o gráfico de Gantt diretamente no framebuffer via `simso_render.c` + `sys_fb_blit()`.
  - Controles: `1`–`7` (troca algoritmo ao vivo), `Space` (modo passo-a-passo), `Enter` (Gantt completo), `ESC`/`Q` (volta para a shell).

---

## 3. Ferramentas de Host para os Colegas (`tools/`)

Documentadas em [`tools/README.md`](../tools/README.md):
- **`tools/sched_host_test.c`**: compila `src/sched/sim_core.c` e `src/sched/policies/*.c` com `gcc` comum no Linux/Windows, rodando *Caso Base* e *Caso com Ociosidade* em menos de 1 segundo sem QEMU.
- **`tools/ui_host_test.c`**: compila `user/simso/simso_render.c` com `gcc` comum e gera `out_gantt.ppm`, permitindo ao colega de UI desenvolver o gráfico de Gantt e a tabela final fora do QEMU e exportar imagens para o relatório.

> **Pontos-chave para estudar (modificações de IA):**
> - Por que `sys_fb_blit` usa um buffer de linha em RAM (`s_row_buf`) em vez de `memcpy` direto na VRAM (evita leituras lentas de memória uncached, levando o DOOM de ~2 FPS para fluido).
> - Por que `sys_opl_io` restringe estritamente as portas `0x388..0x38B` (princípio do menor privilégio em Ring 3).
> - Como `user/simso/simso_render.c` é compartilhado entre Ring 3 (`SIMSO.BIN`) e o host (`tools/ui_host_test.c`).
