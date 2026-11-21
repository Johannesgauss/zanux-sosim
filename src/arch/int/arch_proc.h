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
	uint8_t fxsave_raw[512 + 16];
};

/**
 * @brief Return a 16-byte-aligned pointer inside `ctx->fxsave_raw` for FXSAVE/FXRSTOR.
 */
static inline void *arch_proc_fxbuf(struct arch_proc *ctx)
{
	return (void *)(((uintptr_t)ctx->fxsave_raw + 15ULL) & ~15ULL);
}

/**
 * @brief Enable x87 FPU and SSE/SSE2 instructions for Ring 3 user space.
 *
 * Clears CR0.EM and CR0.TS, sets CR0.MP and CR0.NE, sets CR4.OSFXSR and
 * CR4.OSXMMEXCPT, and loads default x87/MXCSR control words.
 */
static inline void arch_fpu_init(void)
{
	uint64_t cr0, cr4;
	asm volatile ("mov %%cr0, %0" : "=r"(cr0));
	cr0 &= ~((1ULL << 2) | (1ULL << 3)); /* Clear EM, TS */
	cr0 |=  ((1ULL << 1) | (1ULL << 5)); /* Set MP, NE */
	asm volatile ("mov %0, %%cr0" :: "r"(cr0) : "memory");

	asm volatile ("mov %%cr4, %0" : "=r"(cr4));
	cr4 |= (1ULL << 9) | (1ULL << 10);   /* Set OSFXSR, OSXMMEXCPT */
	asm volatile ("mov %0, %%cr4" :: "r"(cr4) : "memory");

	uint32_t mxcsr = 0x1F80U;
	asm volatile ("fninit; ldmxcsr %0" :: "m"(mxcsr) : "memory");
}

/**
 * @brief Initialize a new process's saved x87/SSE state to architectural defaults.
 */
static inline void arch_proc_init_fpu(struct arch_proc *ctx)
{
	uint8_t *buf = (uint8_t *)arch_proc_fxbuf(ctx);
	for (size_t i = 0; i < 512; i++)
		buf[i] = 0;
	/* Default x87 FCW = 0x037F (offset 0), default SSE MXCSR = 0x1F80 (offset 24). */
	buf[0]  = 0x7F;
	buf[1]  = 0x03;
	buf[24] = 0x80;
	buf[25] = 0x1F;
}

/**
 * @brief Save the current hardware x87/SSE registers into @p ctx.
 */
static inline void arch_proc_save_fpu(struct arch_proc *ctx)
{
	uint8_t *buf = (uint8_t *)arch_proc_fxbuf(ctx);
	asm volatile ("fxsave (%0)" :: "r"(buf) : "memory");
}

/**
 * @brief Restore hardware x87/SSE registers from @p ctx.
 */
static inline void arch_proc_restore_fpu(struct arch_proc *ctx)
{
	uint8_t *buf = (uint8_t *)arch_proc_fxbuf(ctx);
	asm volatile ("fxrstor (%0)" :: "r"(buf) : "memory");
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
 * @brief Restore a previously saved address space (CR3).
 *
 * @param cr3 Physical page-table root returned by arch_addr_space_save().
 */
static inline void arch_addr_space_restore(phy_t cr3)
{
	asm volatile ("mov %0, %%cr3;" :: "r"(cr3) : "memory");
}

// TODO modify name
// btw optimize it as well
static inline void arch_proc_init(struct arch_proc *ctx)
{
	virt_array cur_pml4 = phy_to_virt(arch_addr_space_save() & GET_PHY_ADDR);
	virt_array pml4 = (virt_array) pmm_alloc();
	// cpy kernel high half stuff
	for (int i = 0; i < 256; i++) {
		pml4[i] = 0;
	}
	for (int i = 256; i < 512; i++) {
		if (cur_pml4[i] & SET_P)
			pml4[i] = cur_pml4[i];
	}

	ctx->cr3 = virt_to_phy(pml4);
	asm volatile ("mov %0, %%cr3;" :: "r"(ctx->cr3) : "memory");
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
