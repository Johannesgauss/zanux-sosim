#ifndef ARCH_INT_ARCH_PROC_H
#define ARCH_INT_ARCH_PROC_H

#include "idt.h"
#include "arch/vmm.h"
#include "mem/pmm.h"
#include "util/alias.h"
#include "arch/gdt_macros.h"
#include <stdbool.h>

struct arch_proc {
	phy_t cr3;
	struct int_frame intf;
};

// TODO modify name
// btw optimize it as well
static inline void arch_proc_init(struct arch_proc *ctx)
{
	virt_array pml4 = (virt_array) pmm_alloc();
	// cpy kernel high half stuff
	for (int i = 0; i < 256; i++) {
		pml4[i] = 0;
	}
	for (int i = 256; i < 512; i++) {
		if (g_pml4[i] & SET_P)
			pml4[i] = g_pml4[i];
	}

	ctx->cr3 = virt_to_phy(pml4);
	asm volatile ("mov %0, %%cr3;" :: "r"(ctx->cr3) :);
	g_pml4 = pml4;
}

/**
 * @brief Clear all general-purpose registers in a process context.
 *
 * @param ctx Process architecture context to zero.
 */
static inline void arch_proc_zero_regs(struct arch_proc *ctx)
{
	ctx->intf.gp = (struct gp_regs){0};
}

/**
 * @brief Initialize the hardware interrupt return frame for a new Ring 3 process.
 *
 * Aligns the initial stack top down to 16 bytes, enables interrupts (IF = 1)
 * in RFLAGS, and sets the entry point RIP.
 *
 * @param ctx        Process architecture context to initialize.
 * @param entry      Initial user-space instruction pointer.
 * @param stack_base Base (lowest address) of the user stack allocation.
 * @param stack_size Size of the user stack in bytes.
 */
static inline void arch_proc_setup_frame(struct arch_proc *ctx, virt_t entry, virt_t stack_base, size_t stack_size)
{
	ctx->intf.hw.rsp = (virt_array)(((uintptr_t)stack_base + stack_size) & ~0xFULL);
	ctx->intf.hw.rflags = (1 << 9) | 0b10;
	ctx->intf.hw.rip = entry;
}

/**
 * @brief Capture the currently active page-table root (CR3).
 *
 * Pair with arch_addr_space_restore() when temporarily switching address spaces
 * (e.g. while mapping pages for a newly created process inside a syscall).
 *
 * @return Physical address currently loaded in CR3.
 */
static inline phy_t arch_addr_space_save(void)
{
	phy_t cr3;
	asm volatile ("mov %%cr3, %0;" : "=r"(cr3));
	return cr3;
}

/**
 * @brief Restore a previously saved address space (CR3 and g_pml4).
 *
 * @param cr3 Physical page-table root returned by arch_addr_space_save().
 */
static inline void arch_addr_space_restore(phy_t cr3)
{
	g_pml4 = phy_to_virt(cr3 & GET_PHY_ADDR);
	asm volatile ("mov %0, %%cr3;" :: "r"(cr3) : "memory");
}

/**
 * @brief Point the VMM's active PML4 pointer (g_pml4) at a process's page table.
 *
 * Called during context switch before assembly reloads CR3 so subsequent
 * vmm_map() calls (e.g. sys_alloc) target the running process.
 *
 * @param ctx Architecture context of the process being switched in.
 */
static inline void arch_proc_sync_vmm(struct arch_proc *ctx)
{
	g_pml4 = phy_to_virt(ctx->cr3);
}

static inline int in_user_mode(struct int_frame *intf)
{
	return (uint64_t)intf->hw.cs == GDT_USER_CODE;
}

/**
 * @brief Save an interrupt frame into a process context if it came from Ring 3.
 *
 * @param ctx  Destination process architecture context.
 * @param intf Interrupt frame captured on IRQ entry.
 * @return true if the frame originated in user mode and was saved; false otherwise.
 */
static inline bool arch_proc_save_user_ctx(struct arch_proc *ctx, struct int_frame *intf)
{
	if (!in_user_mode(intf))
		return false;
	ctx->intf = *intf;
	return true;
}

/**
 * @brief Halt the CPU until the next enabled interrupt arrives.
 */
static inline void arch_halt(void)
{
	asm volatile ("hlt");
}

#endif // ARCH_INT_ARCH_PROC_H
