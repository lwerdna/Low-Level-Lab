	.text
	.file	"hello.c"
	.globl	main
	.align	16, 0x90
	.type	main,@function
main:                                   # @main
# BB#0:
	push	ebp
	mov	ebp, esp
	sub	esp, 12
	mov	eax, dword ptr [ebp + 12]
	mov	ecx, dword ptr [ebp + 8]
	mov	dword ptr [ebp - 4], 0
	mov	dword ptr [ebp - 8], ecx
	mov	dword ptr [ebp - 12], eax
	mov	eax, dword ptr [ebp - 8]
	add	eax, 42
	add	esp, 12
	pop	ebp
	ret
.Lfunc_end0:
	.size	main, .Lfunc_end0-main


	.ident	"clang version 3.7.1 (tags/RELEASE_371/final)"
	.section	".note.GNU-stack","",@progbits
