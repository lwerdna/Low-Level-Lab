	.text
	.file	"hello.c"
	.globl	main
	.align	2
	.type	main,@function
	.section	.opd,"aw",@progbits
main:                                   # @main
	.align	3
	.quad	.Lfunc_begin0
	.quad	.TOC.@tocbase
	.quad	0
	.text
.Lfunc_begin0:
# BB#0:
	mr 5, 3
	li 6, 0
	stw 6, -12(1)
	stw 5, -16(1)
	std 4, -24(1)
	lwz 5, -16(1)
	addi 5, 5, 42
	extsw 3, 5
	blr
	.long	0
	.quad	0
.Lfunc_end0:
	.size	main, .Lfunc_end0-.Lfunc_begin0


	.ident	"clang version 3.7.1 (tags/RELEASE_371/final)"
	.section	".note.GNU-stack","",@progbits
