# Mapa de Autoria (Paulo vs. IA) e Guia de Estudo

Este documento separa de forma transparente o que foi escrito diretamente pelo autor do kernel (**Paulo**) do que foi gerado ou refatorado com assistência de IA, tanto para rastreabilidade acadêmica quanto para facilitar o estudo antes da arguição.

> **Verificação via Git:** Todos os commits com código de IA trazem `AI-generated:` ou `AI-assisted:` na mensagem de commit. Para auditar exatamente cada diff:
> ```bash
> git log --stat --grep="AI-"
> ```

---

## Legenda de Origem

| Tag | Significado |
| :--- | :--- |
| **Autor (Paulo)** | Projetado e escrito diretamente pelo autor do kernel. |
| **AI-assisted** | Código original do autor que recebeu correções, refatorações ou extensões com auxílio de IA. |
| **AI-generated** | Módulo novo criado com auxílio de IA sob direção arquitetural do autor. |

---

## Mapa Completo por Subsistema

### 1. Boot, Arquitetura e Utilitários (`src/main.c`, `src/arch/`, `src/util/`)

| Arquivo | Origem | Observação |
| :--- | :--- | :--- |
| `src/main.c` | **AI-assisted** (`81fc2b3`, `7b9c9fc`, `feat/simso`) | Base escrita pelo autor; IA adicionou montagem por `PARTUUID`, carga de `SHELL.BIN`, ordem do `kb_handler` antes de `idt_init()`, e detecção de RAM-disk Limine. |
| `src/arch/gdt.{h,c,S}`, `gdt_macros.h` | **Autor (Paulo)** | GDT, TSS e carregamento de segmentos Ring 0 / Ring 3. |
| `src/arch/int/idt.{h,c,S}`, `irq.{h,c}`, `int_handler.S` | **Autor (Paulo)** | IDT, remapeamento PIC, registro de IRQs e stub de interrupção (`e424cb6` apenas removeu include morto de `sched/test`). |
| `src/arch/int/arch_proc.h` | **AI-assisted** (`e424cb6`, `feat/simso`) | Base `arch_proc_init` do autor; IA extraiu helpers x86_64 (`cr3`, frame `iretq`, `hlt`) de `proc.c` e documentou com Doxygen. |
| `src/arch/sys/syscall.{h,c}` | **Autor (Paulo)** | Configuração dos MSRs (`EFER`, `STAR`, `LSTAR`, `FMASK`). |
| `src/arch/sys/syscall.S` | **AI-assisted** (`e209a78`, `feat/simso`) | Entry original do autor; IA adicionou save/restore de registradores caller-saved (`rdi, rsi, rdx, r8, r9, r10`), `r10 -> rcx` e `SYSCALL_NUMBER 14`. |
| `src/arch/vmm.{h,c}` | **Autor (Paulo)** | Paginação x86_64 de 4 níveis e HHDM. |
| `src/util/{alias.h,arena.c,memory.h,memory.c,panic.h,panic.rs}` | **Autor (Paulo)** | Tipos, arena, `memcpy`/`memset` e panic. |

### 2. Memória (`src/mem/`)

| Arquivo | Origem | Observação |
| :--- | :--- | :--- |
| `src/mem/pmm.{h,c}` | **Autor (Paulo)** | Alocador de páginas físicas. |
| `src/mem/kmalloc.{h,c}` | **AI-assisted** (`7b9c9fc`) | Escrito pelo autor; IA corrigiu bug de borda em `usr_kmalloc` quando `size % PAGE_SIZE == 0`. |

### 3. Drivers e Sistema de Arquivos (`src/drivers/`, `src/fs/`, `src/ffi/`)

| Arquivo | Origem | Observação |
| :--- | :--- | :--- |
| `src/drivers/port.{h,rs}`, `src/drivers/clk/pit.rs`, `clk_macros.h` | **Autor (Paulo)** | Primitivas `inb`/`outb`/`inw`/`outw` e PIT 100 Hz. |
| `src/drivers/gfx/{graphical_limine_util.*,print_text.*}`, `src/ffi/*`, `src/lib.rs` | **Autor (Paulo)** | Inicialização Limine, renderização de texto e FFI Rust/C. |
| `src/drivers/disk/ata.{h,rs}`, `serial/uart.{h,rs}`, `gfx/console.{h,rs}`, `kb/kb.h` | **AI-generated** (`0cdb280`) | Drivers Rust de ATA PIO, UART COM1, console e headers C. |
| `src/drivers/kb/kb.rs` | **AI-assisted** (`0cdb280`, `e209a78`) | Base do autor; IA adicionou `CTRL` (`0x1D`/`0x9D`), `Ctrl+L` (`0x0C`), `kb_drain()` e buffer de scancodes brutos. |
| `src/drivers/disk/blk.{h,c}` | **AI-generated** (`1c9ca16`) | Wrapper de bloco que lê do RAM-disk Limine (pendrive USB) com fallback para ATA PIO (`ata.rs` otimizado com `rep insw`/`outsw`). |
| `src/drivers/audio/sb16.{h,c}` | **AI-generated** (`1c9ca16`) | Driver Sound Blaster 16 (8-bit 11025 Hz PCM via ISA DMA canal 1, IRQ 5) para SFX digital original do DOOM. |
| `src/fs/gpt.{h,c}`, `src/fs/fat32.{h,c}`, `fs_macros.h` | **AI-assisted** (`81fc2b3`, `feat/simso`) | Parser GPT e leitor FAT32 escritos pelo autor; IA adicionou `gpt_parse_uuid`/`gpt_partition_from_cmdline`, batching de clusters contíguos, `fat32_open_root_dir`/`fat32_file_size` e troca para `blk_read_sectors`. |

### 4. Processos, Escalonador e Syscalls (`src/sched/`, `src/sys/`)

| Arquivo | Origem | Observação |
| :--- | :--- | :--- |
| `src/sched/proc.{h,c}`, `proc_util.h` | **AI-assisted** (`7b9c9fc`, `e424cb6`, `feat/simso`) | Base do autor; IA adicionou `proc_create_sized` (mapeamento em `0x400000`), `g_timer_ticks`, `kb_drain()` + desbloqueio da shell no `proc_exit`, e delegação de arch para `arch_proc.h`. |
| `src/sched/sched.c` | **AI-assisted** (`7b9c9fc`, `feat/simso`) | Round-robin 0..63 pulando `PROC_DEAD`/`PROC_BLOCK` + hook `sim_engine_is_active()`. |
| `src/sched/{contract/sim.h,sim_core.c,sim_engine.*,policies/*}` | **AI-generated** (`39183c3`) | Contrato congelado em 1 header (`sim.h`), motor discreto compartilhado, 7 arquivos `policies/*.c` (`rr.c` pronta) e ponte para workers Ring 3 reais. |
| `src/sys/{io.*,exec.*,syscall.*}` | **AI-generated** (`e209a78`, `e424cb6`, `39183c3`) | Implementação e headers documentados das syscalls `0..14`. |
| `src/sys/zm.c` | **Autor (Paulo)** | Experimento Zanux Machine do autor. |

### 5. User Space e Ferramentas (`user/`, `tools/`)

| Diretório | Origem | Observação |
| :--- | :--- | :--- |
| `user/lib/`, `user/shell/`, `user/doom/` | **AI-generated** (`e1ef549`, `1c9ca16`) | Wrappers `sys_*`, mini-shell Ring 3 e port Ring 3 do DOOM (com mixer PCM `ds*` SB16 e OPL2). |
| `user/libc/` (`libzanuxc.a` + `zgfx.h`) | **AI-generated** (`39183c3`) | Libc mínima de Ring 3 (`string`, `stdlib`, `stdio` com `%.2f`) e biblioteca gráfica 2D `zgfx`. |
| `user/worker/`, `user/simso/`, `tools/` | **AI-generated** (`39183c3`) | Worker Ring 3, frontend gráfico `SIMSO.BIN` e harnesses de host (`make -C tools test-sched` / `test-ui`). |

---

## Roteiro Rápido de Estudo para a Arguição (Prioridade)

Se o tempo for curto, foque nestes **6 pontos** escritos/modificados por IA — o resto do kernel já é seu:

1. **Entrada de syscall (`src/arch/sys/syscall.S`)**: por que salvamos `rdi, rsi, rdx, r8, r9, r10` na pilha do kernel e fazemos `mov %r10, %rcx` (porque `syscall` sobrescreve `%rcx` com `%rip` de retorno).
2. **Troca de endereço virtual (`src/arch/int/arch_proc.h` + `src/arch/vmm.c`)**: por que `proc_create_sized` salva/restaura `cr3` ao mapear páginas de um processo filho, e como `vmm.c` lê `cr3` diretamente (`current_pml4()`) sem precisar de variável global.
3. **Fluxo não-bloqueante de `sys_sim_start` / `sys_sim_poll` (`src/sched/sim_engine.c`)**: por que não fazemos `sti; hlt` dentro de Ring 0 (evita corromper o `int_frame` de Ring 3) e como `proc_int` alterna entre `SIMSO.BIN` e os `WORKER.BIN` reais.
4. **Motor discreto compartilhado (`src/sched/sim_core.c`)**: como o mesmo código roda no kernel e em `tools/sched_host_test.c`, garantindo métricas 100% idênticas, e como `ctx_overhead` e `deadline_ok` são contabilizados.
5. **Boot de pendrive USB (`src/drivers/disk/blk.c` + `src/main.c`)**: como o módulo `boot():/boot/disk.img` do Limine permite rodar em hardware real sem driver USB/xHCI.
6. **Syscalls de E/S e gráficos (`src/sys/io.c`, `src/sys/exec.c`)**: `sys_read` não-bloqueante, `sys_fb_blit` com buffer em RAM (evita leitura de VRAM lenta) e `sys_readdir`.
