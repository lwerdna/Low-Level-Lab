main: # comment
pushl	%ebp
movl	%esp, %ebp
subl	$12, %esp
movl	12(%ebp), %eax
movl	8(%ebp), %ecx
movl	$0, -4(%ebp)
movl	%ecx, -8(%ebp)
movl	%eax, -12(%ebp)
movl	-8(%ebp), %eax
addl	$42, %eax
addl	$12, %esp
popl	%ebp
retl

jmp main
