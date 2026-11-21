# 02 — Gerenciamento de Memória (PMM, VMM e Alocadores)

## Tabela de Autoria deste Módulo

| Arquivo | Origem | Resumo |
| :--- | :--- | :--- |
| `src/mem/pmm.{h,c}` | **Autor (Paulo)** | Physical Memory Manager — bitmap/alocação de frames físicos de 4 KiB usando o memory map do Limine. |
| `src/arch/vmm.{h,c}` | **Autor (Paulo)** | Virtual Memory Manager — paginação x86_64 de 4 níveis (PML4 → PDPT → PD → PT), HHDM e `vmm_map`/`vmm_free`. |
| `src/mem/kmalloc.{h,c}` | **AI-assisted** (`7b9c9fc`) | Alocador do kernel (`kmalloc`/`kfree`) e alocador de páginas de usuário (`usr_kmalloc`/`vfree`); IA corrigiu o caso `size % PAGE_SIZE == 0` em `usr_kmalloc`. |
| `src/util/arena.c` | **Autor (Paulo)** | Alocador em arena. |
| `src/util/memory.{h,c}` | **Autor (Paulo)** | Implementações freestanding de `memcpy`, `memset`, `memmove`, `memcmp`. |

---

## 1. PMM — Memória Física (`src/mem/pmm.c`) *(Autor: Paulo)*

- Consulta o `limine_memmap_request` para localizar regiões de RAM utilizável (`LIMINE_MEMMAP_USABLE`).
- Mantém o controle de frames de 4 KiB (`PAGE_SIZE = 4096`) e expõe funções para alocar/liberar blocos físicos contíguos.
- Converte endereços físicos em virtuais do kernel somando o offset **HHDM** (`hhdm_request.response->offset`) fornecido pelo Limine.

---

## 2. VMM — Paginação x86_64 (`src/arch/vmm.c`) *(Autor: Paulo)*

- Gerencia tabelas de páginas de 4 níveis (`PML4 -> PDPT -> PD -> PT`).
- Lê o registrador `CR3` diretamente via `current_pml4()` (fonte única de verdade, sem ponteiro global duplicado).
- `vmm_map(void *virt)`: percorre os 4 níveis criando tabelas intermediárias quando ausentes (com flags `PRESENT | WRITABLE | USER` nos níveis de usuário), aloca um frame físico via PMM, zera a página e instala a entrada na PT.
- Cada processo criado em `proc_create_sized()` recebe sua **própria PML4** (compartilhando o *higher half* do kernel, mas isolando totalmente o espaço de endereços do *lower half* em `0x00400000+`).

---

## 3. Alocadores (`src/mem/kmalloc.c`) *(AI-assisted)*

- **`kmalloc(size)` / `kfree(ptr)`**: aloca memória no *higher half* do kernel (usada para `struct proc`, buffers de arquivos FAT32, diretório raiz, etc.).
- **`usr_kmalloc(size)`**: aloca memória mapeada no espaço virtual de usuário (usada para a pilha de cada processo em `proc_create_sized()` e pela syscall `sys_alloc()` usada por `SHELL.BIN` e `DOOM.BIN`).

### Correção importante em `usr_kmalloc` *(AI-assisted, commit `7b9c9fc`)*

Anteriormente, quando `size` era múltiplo exato de `PAGE_SIZE` (ex.: `4096` bytes), o cálculo de avanço do bump-pointer fazia:
- `s_usr_page_base += PAGE_SIZE` (apontando para a página seguinte, **ainda não mapeada**) e `s_usr_page_offset = 0`.
- Na próxima chamada a `usr_kmalloc()`, o código via `offset == 0` e assumia que `s_usr_page_base` já estava mapeada, causando **Page Fault (`#PF`)**.

**Correção:** quando a alocação termina exatamente na borda da página (`size % PAGE_SIZE == 0`), `s_usr_page_offset` passa a ser definido como `PAGE_SIZE`, forçando a próxima chamada a mapear uma nova página antes de entregar o ponteiro.

---

## 4. Layout Virtual de um Processo Ring 3

```text
0x0000_0000_0040_0000  ── Código (.text), .rodata, .data e .bss do binário flat
                          (mapeado em proc_create_sized: code_size + bss_size)
                          - SHELL.BIN / DOOM.BIN: bss_size = 8 MiB
                          - WORKER.BIN (simso):   bss_size = 4 KiB
0x0000_0000_xxxx_xxxx  ── Pilha de usuário + heap dinâmico (via usr_kmalloc / sys_alloc)
0xFFFF_8000_0000_0000+ ── HHDM (memória física mapeada para o kernel)
0xFFFF_FFFF_8000_0000+ ── Código e dados do kernel (compartilhado entre todas as PML4s)
```

> **Pontos-chave para estudar (modificações de IA):**
> - O bug de `size % PAGE_SIZE == 0` em `usr_kmalloc` (fácil de explicar se perguntarem sobre depuração de `#PF`).
> - A separação entre `proc_create` (8 MiB de `.bss` para apps grandes como DOOM) e `proc_create_sized` (4 KiB de `.bss` para workers leves do `simso`, evitando consumir centenas de MiB ao rodar várias simulações seguidas).
