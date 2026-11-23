.section .text.entry, "ax"
.global _start
.extern vim_main

_start:
	and $-16, %rsp
	call vim_main
	mov %rax, %rdi
	xor %rax, %rax
	syscall
1:
	jmp 1b
