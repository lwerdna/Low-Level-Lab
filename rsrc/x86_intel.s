main: # comment
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

jmp main
