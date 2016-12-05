main: # comment
push rbp
mov  rbp, rsp
mov  dword ptr [rbp - 4], 0
mov  dword ptr [rbp - 8], edi
mov  qword ptr [rbp - 16], rsi
mov  edi, dword ptr [rbp - 8]
add  edi, 42
mov  eax, edi
pop  rbp
ret

jmp  main
