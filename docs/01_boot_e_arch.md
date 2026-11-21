# 01 — Boot, Arquitetura x86_64, GDT, IDT e Syscall Entry

## Tabela de Autoria deste Módulo

| Arquivo | Origem | Resumo |
| :--- | :--- | :--- |
| `boot/limine.conf` | **AI-assisted** | Entrada Limine com `cmdline: root=PARTUUID=...` e `module_path: boot():/boot/disk.img`. |
| `src/main.c` | **AI-assisted** | Sequência de boot (`kmain`) integrando GDT, PIT, PMM, VMM, RAM-disk/FAT32, `SHELL.BIN`, syscalls, teclado e IDT. |
| `src/arch/gdt.{h,c,S}`, `gdt_macros.h` | **Autor (Paulo)** | GDT (Ring 0 + Ring 3) e TSS (`tss.rsp0` para troca de pilha em interrupções/syscalls). |
| `src/arch/int/idt.{h,c,S}`, `irq.{h,c}`, `int_handler.S` | **Autor (Paulo)** | IDT, remapeamento do PIC 8259, registro dinâmico de IRQs e stub de contexto. |
| `src/arch/int/arch_proc.h` | **AI-assisted** | Helpers x86_64 de processo (registradores, frame `iretq`, `cr3`, `hlt`) isolados de `proc.c`. |
| `src/arch/sys/syscall.{h,c}` | **Autor (Paulo)** | Configuração dos MSRs de `SYSCALL`/`SYSRET`. |
| `src/arch/sys/syscall.S` | **AI-assisted** | Stub `syscall_handler` (salva/restaura registradores + despacha `syscall_table`) e `sys_exit`. |

---

## 1. Sequência de Boot (`src/main.c :: kmain`)

Quando o Limine transfere controle para `kmain()`, o processador já está em **64-bit Long Mode** com paginação ativa e HHDM (*Higher Half Direct Map*) configurado. A ordem de inicialização em `src/main.c` é crítica:

1. `gfx_init()` — obtém o framebuffer do Limine (`framebuffer_request`).
2. `gdt_init()` — instala segmentos de código/dados de Ring 0 (`0x08`, `0x10`), Ring 3 (`0x1B`, `0x23`) e a TSS (cuja `rsp0` em `tss+4` fornece a pilha de kernel).
3. `timer_add()` — configura o PIT em 100 Hz (1 tick a cada 10 ms) no IRQ0 (vetor `32`).
4. `pmm_init()` e `vmm_init()` — inicializam os gerenciadores de memória física e virtual.
5. **RAM-Disk Limine *(AI-generated)*** — procura um módulo chamado `disk.img` em `module_request.response`. Se encontrado, chama `blk_init_ramdisk(mod->address, mod->size)` para operar em modo pendrive USB sem precisar de driver xHCI.
6. **Montagem do Root FS *(AI-assisted)*** — lê `cmdline_request.response->cmdline` (`root=PARTUUID=...`), encontra a partição no GPT via `gpt_partition_from_cmdline()` e monta o FAT32 via `fat32_init(root_part.start_lba)`.
7. **Carga do PID 0 (`SHELL.BIN`) *(AI-assisted)*** — abre o diretório raiz FAT32, lê `SHELL   BIN`, cria o processo PID 0 via `proc_create(shell.mptr, shell.size, 65536)` e libera o buffer temporário.
8. `sched_policies_init()` *(AI-generated)* — registra as 7 políticas do simulador de escalonamento.
9. `syscall_init()` — habilita a instrução `syscall` configurando `IA32_EFER.SCE`, `STAR`, `LSTAR` e `FMASK`.
10. `irqr_add(1, kb_handler)` + `kb_drain()` — registra o IRQ1 do teclado **antes** de habilitar interrupções!
    - *Por quê?* Assim que `idt_init()` executa `sti`, o primeiro IRQ0 já faz `iretq` para o `SHELL.BIN` em Ring 3 — qualquer código após `idt_init()` não é garantido de rodar antes do usuário.
11. `idt_init()` — carrega a IDT e habilita interrupções; entra em loop `hlt`.

---

## 2. GDT, IDT e `int_handler.S` *(Autor: Paulo)*

- **GDT (`src/arch/gdt.c`, `gdt.S`)**: define descritores nulos, código/dados Ring 0, dados/código Ring 3 (na ordem exigida pelo `SYSRET` do x86_64) e descritor TSS de 16 bytes.
- **IDT & PIC (`src/arch/int/idt.c`, `irq.c`)**: remapeia o PIC 8259 para os vetores `32..47` (evitando conflito com exceções da CPU `0..31`).
  - IRQ0 (vetor `32`) chama `proc_int(struct int_frame *intf)` em `src/sched/proc.c`, que retorna o `struct proc *` do próximo processo a executar.
  - `int_handler.S` carrega `cr3` do processo retornado, restaura seus registradores gerais e executa `iretq` de volta para Ring 3.
  - IRQ1 (vetor `33`) despacha para o handler registrado em `irqr_add(1, kb_handler)`.

---

## 3. Helpers de Arquitetura de Processo (`src/arch/int/arch_proc.h`) *(AI-assisted)*

Para manter `src/sched/proc.c` independente de arquitetura, todas as operações x86_64 sobre `struct arch_proc` foram centralizadas em `src/arch/int/arch_proc.h`:

| Função | O que faz |
| :--- | :--- |
| `arch_proc_init(&ctx)` | Aloca uma nova tabela PML4 via `vmm_new_pml4()`, clona o half superior do kernel e guarda em `ctx.cr3`. |
| `arch_proc_zero_regs(&ctx)` | Zera todos os registradores gerais de `ctx`. |
| `arch_proc_setup_frame(&ctx, entry, stack, size)` | Prepara `rip = entry`, `rsp = stack + size`, `cs = 0x23`, `ss = 0x1B`, `rflags = 0x202` (IF=1). |
| `arch_addr_space_save()` / `arch_addr_space_restore(as)` | Salva/restaura `cr3` (essencial quando `proc_create_sized` é chamado de dentro de uma syscall como `sys_exec` ou `sys_sim_start`!). |
| `arch_proc_save_user_ctx(&ctx, intf)` | Verifica `(intf->cs & 3) == 3`; só copia `intf` para `ctx` se a interrupção ocorreu em Ring 3 (impedindo corrupção de contexto se o IRQ0 disparar no kernel). |
| `arch_halt()` | Executa `hlt`. |

---

## 4. Entrada de Syscall (`src/arch/sys/syscall.S`) *(AI-assisted)*

A instrução x86_64 `syscall` **não** troca de pilha automaticamente e sobrescreve `%rcx` (com `%rip` de retorno) e `%r11` (com `RFLAGS`). Por isso `syscall_handler` em `src/arch/sys/syscall.S`:

1. Salva o `%rsp` de Ring 3 em `user_rps` e troca `%rsp` para `tss+4` (`tss.rsp0`, pilha de kernel).
2. Empilha `user_rps`, `%r11`, `%rcx` e **todos os registradores caller-saved de argumentos** (`%rdi`, `%rsi`, `%rdx`, `%r8`, `%r9`, `%r10`) + alinhamento de 16 bytes (`sub $8, %rsp`).
3. Valida `%rax < SYSCALL_NUMBER` (`14`).
4. Copia `mov %r10, %rcx` (convenção System V AMD64 usa `%rcx` como 4º argumento em C, enquanto `syscall` usa `%r10`).
5. Chama `call *syscall_table(,%rax, 8)` — valor de retorno volta em `%rax`.
6. Desempilha todos os registradores na ordem inversa e retorna para Ring 3 com `sysretq`.

> **Pontos-chave para estudar (modificações de IA):**
> - Por que `syscall.S` precisa salvar `rdi, rsi, rdx, r8, r9, r10`? Sem isso, o GCC em código Ring 3 (como `shell.c`) mantinha variáveis em `%r8` entre duas syscalls consecutivas e a primeira syscall zerava `%r8`.
> - Por que `arch_addr_space_save()` / `arch_addr_space_restore()` são necessários em `proc_create_sized`? Porque `arch_proc_init` troca `cr3` para a PML4 do filho para mapear `0x400000` e copiar o binário; ao retornar para a syscall chamadora (`sys_exec` / `sys_sim_start`), precisamos restaurar o `cr3` do pai antes do `sysretq`. Como `vmm.c` lê `cr3` diretamente (`current_pml4()`), não existe estado duplicado.
