	.text
	.file	"hello.c"
	.globl	main
	.align	2
	.type	main,@function
main:                                   # @main
.Lfunc_begin0:
# BB#0:
	stw 31, -4(1)
	stwu 1, -32(1)
	mr 31, 1
	li 5, 0
	stw 5, 24(31)
	stw 3, 20(31)
	stw 4, 16(31)
	lwz 5, 20(31)
	addi 5, 5, 42
	stw 3, 12(31)                   # 4-byte Folded Spill
	mr 3, 5
	stw 4, 8(31)                    # 4-byte Folded Spill
	addi 1, 1, 32
	lwz 31, -4(1)
	blr
.Lfunc_end0:
	.size	main, .Lfunc_end0-.Lfunc_begin0


	.ident	"clang version 3.7.1 (tags/RELEASE_371/final)"
	.section	".note.GNU-stack","",@progbits
