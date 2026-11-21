.global sys_exit
.type sys_exit, @function
.global proc_exit
.type proc_exit, @function
.extern tss
sys_exit:
	mov tss+4(%rip), %rsp

	call proc_exit
	int $32

.global syscall_handler
.type syscall_handler, @function
.extern syscall_table
#define SYSCALL_NUMBER 2
syscall_handler:
	mov %rsp, user_rps(%rip)
	mov tss+4(%rip), %rsp
	push user_rps(%rip)
	push %r11 // rflags
	push %rcx // rip
	sub $8, %rsp

	cmp $SYSCALL_NUMBER, %rax
	jae INVALID_SYSCALL

	call *syscall_table(,%rax, 8)

	add $8, %rsp
	pop %rcx
	pop %r11
	pop %rsp
	sysretq
INVALID_SYSCALL:
// TODO
	//call proc_crash
	hlt
	jmp INVALID_SYSCALL

.section .bss
.align 8
user_rps:
	.skip 8
