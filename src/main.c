#include "drivers/gfx/print_text.h"
#include "drivers/gfx/graphical_limine_util.h"
#include "arch/gdt.h"
#include "arch/int/idt.h"
#include "drivers/clk/clk.h"
#include "drivers/kb/kb.h"
#include "fs/gpt.h"
#include "fs/fat32.h"
#include "sched/proc.h"
#include "util/panic.h"

extern void vmm_map(void *addr);
extern void print_hex(uint64_t val);
extern bool pmm_init();
extern bool vmm_init();
extern void vmm_free(void *addr);
extern void *kmalloc(size_t size);
extern void syscall_init();

void kmain()
{
	gfx_init();
	kprint("Welcome to Zanux Operating System\nCopyright (c). All rights reserved.\n");
	if (gdt_init())
		kprint("[ SUCCESS ] Global Descriptor Table (GDT) loaded. \n");

	if (timer_add())
		kprint("[ SUCCESS ] Programmable Interval Timer (PIT) setted on Interrupt Request 0 (IRQ0).\n");

	if (pmm_init())
		kprint("[ SUCCESS ] Physical Memory Managment (PMM) loaded.\n");

	if (vmm_init())
		kprint("[ SUCCESS ] Virtual Memory Managment (VMM) loaded.\n");

	if (!cmdline_request.response || !cmdline_request.response->cmdline)
		panic("NO KERNEL CMDLINE FROM LIMINE");

	struct gpt_partition_entry root_part = gpt_partition_from_cmdline(cmdline_request.response->cmdline);
	if (!fat32_init(root_part.start_lba))
		panic("ROOT PARTITION IS NOT FAT32");
	kprint("[ SUCCESS ] FAT32 root partition mounted.\n");

	struct fat32_dir_entry *root_dir = fat32_open_root_dir();
	struct file shell = fat32_read_file(root_dir, "SHELL   BIN");
	if (!shell.mptr || shell.size == 0)
		panic("FAILED TO LOAD SHELL.BIN");

	proc_create(shell.mptr, shell.size, 65536);
	fat32_close_file(shell);
	kprint("[ SUCCESS ] SHELL.BIN loaded.\n");

	syscall_init();
	irqr_add(1, kb_handler);
	kb_drain();
	if (idt_init())
		kprint("[ SUCCESS ] Interrupt Descriptor Table (IDT) loaded.\n");

	while (1) {
		asm volatile ("hlt");
	}
}
