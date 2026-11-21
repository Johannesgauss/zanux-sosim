# AI Modifications to `src/`

Modular unified diffs separating all AI-authored changes under `src/` from hand-written code.

| Patch | Files | Summary |
| :--- | :--- | :--- |
| `01_limine_cmdline.diff` | `src/drivers/gfx/graphical_limine_util.{h,c}` | Add Limine `cmdline_request` |
| `02_fs_gpt_fat32.diff` | `src/fs/gpt.{h,c}`, `src/fs/fat32.{h,c}` | `gpt_parse_uuid` / `gpt_partition_from_cmdline`; decouple `fat32_init(start_lba)`; add `fat32_open_root_dir`, `fat32_dir_entries_count`, `fat32_file_size`, contiguous-cluster batching, non-panicking missing-file return |
| `03_syscalls_arch.diff` | `src/arch/sys/syscall.S` | Preserve caller-saved registers across `syscall`, pass `%r10` → `%rcx`, `SYSCALL_NUMBER 12`, fix `sys_exit:` newline |
| `04_syscalls_io.diff` | `src/sys/io.c` | Implement `sys_write` (phosphor green + `\x0C` clear) and `sys_read` (PS/2 + COM1 serial) |
| `05_syscalls_exec_and_table.diff` | `src/sys/exec.c`, `src/sys/syscall.c` | Add generic syscalls `3..11` (`sys_exec`, `sys_alloc`, `sys_read_file`, `sys_fb_blit`, `sys_poll_scancode`, `sys_ticks_ms`, `sys_pcspk`, `sys_opl_io`, `sys_readdir`) |
| `06_sched_and_proc.diff` | `src/sched/proc.c`, `src/sched/sched.c` | Fixed `0x400000` flat-binary mapping, `cr3`/`g_pml4` save/restore on `proc_create` & `proc_int`, `g_timer_ticks`, unblock shell on `proc_exit`, round-robin `0..63` |
| `07_mem_kmalloc.diff` | `src/mem/kmalloc.c` | Fix `usr_kmalloc` unmapped-page bug when `size % PAGE_SIZE == 0` |
| `08_kb_ctrl_l.diff` | `src/drivers/kb/kb.rs` | Track `CTRL` (`0x1D`/`0x9D`) and emit `0x0C` on `Ctrl+L` |
| `09_main_boot_shell.diff` | `src/main.c` | Mount FAT32 root from Limine `cmdline`, load & spawn `SHELL.BIN`, register `kb_handler` before `idt_init` |
