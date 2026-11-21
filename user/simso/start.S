.section .text.entry, "ax"
.global _start
.extern simso_main

_start:
	and $-16, %rsp
	call simso_main
	mov %rax, %rdi
	xor %rax, %rax
	syscall
1:
	jmp 1b
