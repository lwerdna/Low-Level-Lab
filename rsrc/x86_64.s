main: # comment
pushq	%rbp
movq	%rsp, %rbp
movl	$0, -4(%rbp)
movl	%edi, -8(%rbp)
movq	%rsi, -16(%rbp)
movl	-8(%rbp), %edi
addl	$42, %edi
movl	%edi, %eax
popq	%rbp
retq

jmp main
