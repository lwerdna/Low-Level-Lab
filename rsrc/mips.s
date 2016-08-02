	.text
	.abicalls
	.option	pic0
	.section	.mdebug.abi32,"",@progbits
	.nan	legacy
	.file	"hello.c"
	.text
	.globl	main
	.align	2
	.type	main,@function
	.set	nomicromips
	.set	nomips16
	.ent	main
main:
	.frame	$fp,16,$ra
	.mask 	0x40000000,-4
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	.set	noat
	addiu	$sp, $sp, -16
	sw	$fp, 12($sp)
	move	 $fp, $sp
	sw	$zero, 8($fp)
	sw	$4, 4($fp)
	sw	$5, 0($fp)
	lw	$4, 4($fp)
	addiu	$2, $4, 42
	move	 $sp, $fp
	lw	$fp, 12($sp)
	addiu	$sp, $sp, 16
	jr	$ra
	nop
	.set	at
	.set	macro
	.set	reorder
	.end	main
$func_end0:
	.size	main, ($func_end0)-main


	.ident	"clang version 3.7.1 (tags/RELEASE_371/final)"
	.section	".note.GNU-stack","",@progbits
	.text
