.section .text.entry, "ax"
.global _start
.extern doom_user_main

_start:
	and $-16, %rsp
	call doom_user_main
	mov %rax, %rdi
	xor %rax, %rax
	syscall
1:
	jmp 1b
