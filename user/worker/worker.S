.section .text.entry, "ax"
.global _start

/* Real Ring 3 CPU-bound worker process scheduled by Zanux's IRQ0 sim_engine. */
_start:
	xor %rax, %rax
1:
	inc %rax
	pause
	jmp 1b
