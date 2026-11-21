# 03 — Drivers (Rust + C), RAM-Disk USB, GPT e FAT32

## Tabela de Autoria deste Módulo

| Arquivo | Origem | Resumo |
| :--- | :--- | :--- |
| `src/drivers/port.{h,rs}`, `src/drivers/clk/{pit.rs,clk_macros.h}` | **Autor (Paulo)** | Portas I/O x86 (`inb`/`outb`/`inw`/`outw`) e timer PIT 100 Hz. |
| `src/drivers/gfx/{graphical_limine_util.*,print_text.*}`, `src/ffi/*`, `src/lib.rs` | **Autor (Paulo)** | Framebuffer Limine, `kprint`, FFI entre Rust e C. |
| `src/drivers/disk/ata.{h,rs}`, `serial/uart.{h,rs}`, `gfx/console.{h,rs}`, `kb/kb.h` | **AI-generated** (`0cdb280`) | Drivers Rust de ATA PIO 28-bit LBA, UART COM1 (`0x3F8`), console e headers C. |
| `src/drivers/kb/kb.rs` | **AI-assisted** (`0cdb280`, `e209a78`) | Base do autor; IA adicionou estado `CTRL` (`0x1D`/`0x9D`), `Ctrl+L` (`0x0C`), `kb_drain()` e fila de scancodes brutos (`kb_poll_scancode`). |
| `src/drivers/disk/blk.{h,c}` | **AI-generated** (`feat/simso`) | Abstração de bloco que lê de um RAM-disk Limine (pendrive USB) com fallback transparente para ATA PIO. |
| `src/fs/gpt.{h,c}`, `src/fs/fat32.{h,c}`, `fs_macros.h` | **AI-assisted** (`81fc2b3`, `feat/simso`) | Parser GPT e leitor FAT32 do autor; IA adicionou `gpt_parse_uuid` / `gpt_partition_from_cmdline`, batching de clusters contíguos, `fat32_open_root_dir`, `fat32_file_size` e uso de `blk_read_sectors`. |

---

## 1. Drivers em Rust e Integração FFI (`src/lib.rs`, `src/drivers/`)

O `GNUmakefile` compila `src/lib.rs` (e todos os submódulos `src/drivers/**/*.rs`) para um único objeto estático `obj/src/lib.rs.o` com `rustc --target x86_64-unknown-none`. Funções exportadas com `#[no_mangle] pub extern "C" fn` são chamadas diretamente pelo C:

- **Portas I/O (`port.rs` / `port.h`) *(Autor: Paulo)*:** `inb`, `outb`, `inw`, `outw`.
- **Timer PIT (`pit.rs` / `clk.h`) *(Autor: Paulo)*:** programa o PIT 8253/8254 em 100 Hz (dispara IRQ0 a cada 10 ms).
- **Teclado PS/2 (`kb.rs` / `kb.h`) *(AI-assisted)*:**
  - `kb_handler()` (registrado no IRQ1) lê o scancode da porta `0x60` e alimenta **duas** filas:
    1. `RAW_BUFFER` (lida por `kb_poll_scancode()`, usada pelo `DOOM.BIN` e `SIMSO.BIN`).
    2. `KB_BUFFER` (ASCII traduzido, lida por `kb_getchar()` em `sys_read()`, incluindo `Ctrl+L` → `0x0C` para limpar tela).
  - `kb_drain()` esvazia ambas as filas — chamada no boot e no `proc_exit()` para evitar que teclas pressionadas dentro do DOOM/SIMSO vazem como comandos na `SHELL.BIN`.
- **Serial UART (`uart.rs` / `uart.h`) *(AI-generated)*:** COM1 (`0x3F8`), usado como fallback de leitura em `sys_read()`.
- **ATA PIO (`ata.rs` / `ata.h`) *(AI-generated)*:** leitura/escrita em setores de 512 bytes no barramento IDE primário (`0x1F0..0x1F7`), LBA de 28 bits, até 128 setores por operação.

---

## 2. Abstração de Bloco e RAM-Disk para Pendrive USB (`src/drivers/disk/blk.{h,c}`) *(AI-generated)*

Em hardware real inicializado via **pendrive USB**, o pendrive está conectado a uma controladora USB (xHCI/EHCI), e **não** ao barramento IDE legado `0x1F0`. Sem um driver xHCI completo, o ATA PIO falharia no pendrive.

**Solução adotada:**
1. O `GNUmakefile` inclui `disk.img` dentro de `template.iso` em `/boot/disk.img`, e `boot/limine.conf` declara `module_path: boot():/boot/disk.img`.
2. Antes de saltar para o kernel, o **Limine** usa o firmware UEFI/BIOS para carregar `disk.img` inteiro na RAM.
3. Em `src/main.c`, se o módulo `disk.img` estiver presente, o kernel chama `blk_init_ramdisk(mod->address, mod->size)`.
4. Tanto `gpt.c` quanto `fat32.c` chamam `blk_read_sectors(lba, count, buf)`:
   - Se o RAM-disk estiver ativo → copia `count * 512` bytes direto da RAM (`memcpy`).
   - Caso contrário → faz fallback transparente para `ata_read_sectors()`.

Resultado: o mesmo kernel e a mesma `template.iso` funcionam igualmente no QEMU e em qualquer PC real via pendrive USB.

---

## 3. GPT e FAT32 (`src/fs/gpt.c`, `src/fs/fat32.c`) *(AI-assisted)*

- **GPT (`gpt.c`)**:
  - Lê o Protective MBR (LBA 0, verifica `os_type == 0xEE`), o cabeçalho GPT (LBA 1) e a tabela de entradas de partição.
  - `gpt_parse_uuid()` / `gpt_partition_from_cmdline()` *(AI-assisted)*: extrai `root=PARTUUID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx` da linha de comando do Limine (convertendo os 3 primeiros campos de big-endian texto para mixed-endian UEFI GUID) e retorna a entrada com `start_lba`.
- **FAT32 (`fat32.c`)**:
  - `fat32_init(start_lba)`: lê o BPB/Volume ID, valida a assinatura `0x55AA`, calcula `s_fat_first_lba` e `s_data_start_cluster_lba`.
  - `fat32_next_cluster(cluster)`: mantém cache de 1 setor da tabela FAT para evitar releituras repetidas.
  - `fat32_read_file(dir, name83)`: procura o nome 8.3 (ex.: `"SHELL   BIN"`), retorna `{ .mptr = NULL }` se não existir (sem dar `panic`), e agrupa **clusters contíguos** em lotes de até 128 setores por leitura — permitindo carregar arquivos grandes como `DOOM1.WAD` (10,4 MiB) em menos de 1 segundo.

> **Pontos-chave para estudar (modificações de IA):**
> - Por que `blk.c` resolve o boot em pendrive USB real sem precisar escrever um driver xHCI.
> - Como `gpt_parse_uuid` mapeia os bytes *mixed-endian* do GUID GPT.
> - Por que o batching de clusters contíguos em `fat32_read_file` reduziu drasticamente o número de leituras de disco.
